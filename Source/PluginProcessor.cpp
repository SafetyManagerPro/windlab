#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Dsp/WindEngine.h"
#include "Dsp/VelocityResponse.h"
#include "Dsp/HybridMidiRoute.h"
#include "Dsp/BreathCalibration.h"
#include <array>
#include <cmath>

namespace
{
    struct ChannelControls
    {
        float mod = 0.0f;
        float expression = 1.0f;
        float breath = 1.0f;
        float pressure = 0.0f;
        bool breathEnabled = false;
    };

    // A voice that can be told "the next note slurs over you": fade out quickly.
    struct LegatoYield
    {
        virtual ~LegatoYield() = default;
        virtual void yieldToLegato() noexcept = 0;
    };

    // Cache CC events in MIDI-event order, including CCs received before note-on.
    // JUCE's base Synthesiser dispatches controllers to currently active voices only.
    class PerformanceSynthesiser final : public juce::Synthesiser
    {
    public:
        PerformanceSynthesiser(windlab::MidiLearnBindings& learned,
                               std::atomic<float>* articulationParameter,
                               juce::AudioProcessorValueTreeState& params)
            : ccAssignments(learned), articulation(articulationParameter),
              breathThreshold(params.getRawParameterValue("breathThreshold")),
              breathCurve(params.getRawParameterValue("breathCurve")),
              breathSource(params.getRawParameterValue("breathSource")),
              legatoMode(params.getRawParameterValue("legatoMode")) {}
        // Frequency of the note being slurred from, or 0 when the new note starts fresh.
        double currentlyStartingLegatoFrom() const noexcept { return legatoFrom; }
        ChannelControls currentlyStartingControls() const noexcept
        {
            return channels[static_cast<std::size_t>(juce::jlimit(1, 16, startingChannel) - 1)];
        }
        void resetControllers()
        {
            channels.fill(ChannelControls{});
            startingChannel = 1;
            heldKeys.fill(0);
        }
        void noteOn(int midiChannel, int midiNoteNumber, float velocity) override
        {
            startingChannel = midiChannel;
            legatoFrom = 0.0;
            const auto channelIndex = static_cast<std::size_t>(juce::jlimit(1, 16, midiChannel) - 1);
            // Monophonic-style slur: exactly one key still held, the new key comes at
            // least 60 ms later (so chords are not slurred), within an octave, and the
            // articulation is Sustain.
            const bool sustainArticulation = articulation == nullptr || articulation->load() < .5f;
            const double elapsedSeconds = static_cast<double>(renderedSamples - lastNoteOnSample[channelIndex])
                                        / std::max(1.0, getSampleRate());
            // 0.9.7 legato mode: 0 auto, 1 always (also fast overlaps), 2 never (chords).
            const int mode = legatoMode != nullptr ? static_cast<int>(legatoMode->load()) : 0;
            const bool slurAllowed = mode == 1 ? true : (mode == 2 ? false : elapsedSeconds > .06);
            if (sustainArticulation && slurAllowed && heldKeys[channelIndex] == 1
                && std::abs(midiNoteNumber - lastNote[channelIndex]) <= 12
                && midiNoteNumber != lastNote[channelIndex])
            {
                legatoFrom = 440.0 * std::exp2((lastNote[channelIndex] - 69.0) / 12.0);
                for (auto* voice : voices)
                    if (voice->isVoiceActive() && voice->isPlayingChannel(midiChannel) && voice->isKeyDown())
                        if (auto* yielding = dynamic_cast<LegatoYield*>(voice))
                            yielding->yieldToLegato();
            }
            lastNote[channelIndex] = midiNoteNumber;
            lastNoteOnSample[channelIndex] = renderedSamples;
            heldKeys[channelIndex] = juce::jmin(16, heldKeys[channelIndex] + 1);
            juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
            legatoFrom = 0.0;
        }
        void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
        {
            if (juce::isPositiveAndBelow(midiChannel - 1, 16))
            {
                auto& held = heldKeys[static_cast<std::size_t>(midiChannel - 1)];
                held = juce::jmax(0, held - 1);
            }
            juce::Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);
        }
        void allNotesOff(int midiChannel, bool allowTailOff) override
        {
            if (midiChannel <= 0) heldKeys.fill(0);
            else if (juce::isPositiveAndBelow(midiChannel - 1, 16))
                heldKeys[static_cast<std::size_t>(midiChannel - 1)] = 0;
            juce::Synthesiser::allNotesOff(midiChannel, allowTailOff);
        }
        void handleController(int midiChannel, int controllerNumber, int controllerValue) override
        {
            // Process mappings on the audio thread. The internal Phrase Editor
            // uses channel 16 and never participates in MIDI Learn.
            controllerNumber = ccAssignments.route(midiChannel, controllerNumber);
            if (controllerNumber < 0) return;
            if (juce::isPositiveAndBelow(midiChannel - 1, 16))
            {
                auto& c = channels[static_cast<std::size_t>(midiChannel - 1)];
                const float v = controllerValue / 127.0f;
                if (controllerNumber == 1) c.mod = v;
                if (controllerNumber == 2) { c.breath = v; c.breathEnabled = true; }
                if (controllerNumber == 11) c.expression = v;
                if (controllerNumber == 121) c = ChannelControls{};
            }
            if (controllerNumber == 2 && midiChannel >= 1 && midiChannel < 16)
            {
                const auto normalized = windlab::calibrateBreath(controllerValue / 127.0f,
                    breathThreshold->load(), breathCurve->load());
                controllerValue = juce::jlimit(0, 127, static_cast<int>(std::lround(normalized * 127.0f)));
                channels[static_cast<std::size_t>(midiChannel - 1)].breath = controllerValue / 127.0f;
            }
            juce::Synthesiser::handleController(midiChannel, controllerNumber, controllerValue);
        }
        void handleChannelPressure(int midiChannel, int value) override
        {
            if (juce::isPositiveAndBelow(midiChannel - 1, 16))
            {
                auto& c = channels[static_cast<std::size_t>(midiChannel - 1)];
                c.pressure = value / 127.0f;
                // Aftertouch as breath (EWI/Aerophone set to Channel Pressure).
                // The internal Phrase Editor channel 16 is never re-routed.
                if (midiChannel < 16 && breathSource != nullptr && breathSource->load() >= .5f)
                {
                    const auto normalized = windlab::calibrateBreath(value / 127.0f,
                        breathThreshold->load(), breathCurve->load());
                    const int breathValue = juce::jlimit(0, 127, static_cast<int>(std::lround(normalized * 127.0f)));
                    c.breath = breathValue / 127.0f;
                    c.breathEnabled = true;
                    juce::Synthesiser::handleController(midiChannel, 2, breathValue);
                }
            }
            juce::Synthesiser::handleChannelPressure(midiChannel, value);
        }
    protected:
        // Sample clock for the slur timing (events are processed between sub-blocks).
        void renderVoices(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
        {
            juce::Synthesiser::renderVoices(buffer, startSample, numSamples);
            renderedSamples += numSamples;
        }
    private:
        windlab::MidiLearnBindings& ccAssignments;
        std::atomic<float>* articulation = nullptr;
        std::atomic<float>* breathThreshold;
        std::atomic<float>* breathCurve;
        std::atomic<float>* breathSource;
        std::atomic<float>* legatoMode;
        std::array<ChannelControls, 16> channels{};
        std::array<int, 16> heldKeys{};
        std::array<int, 16> lastNote{};
        std::array<std::int64_t, 16> lastNoteOnSample{};
        std::int64_t renderedSamples = 0;
        double legatoFrom = 0.0;
        int startingChannel = 1;
    };

    class WindSound final : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    class WindVoice final : public juce::SynthesiserVoice, public LegatoYield
    {
    public:
        WindVoice(juce::AudioProcessorValueTreeState& params, PerformanceSynthesiser& synth,
                  const windlab::RecordedWinds& samples)
            : controllerSource(synth), sampleSource(samples),
              sourceMode(params.getRawParameterValue("sourceMode")),
              velocityResponse(params.getRawParameterValue("velocityResponse")),
              instrument(params.getRawParameterValue("instrument")),
              articulation(params.getRawParameterValue("articulation")),
              attack(params.getRawParameterValue("attack")),
              release(params.getRawParameterValue("release")),
              tone(params.getRawParameterValue("tone")),
              air(params.getRawParameterValue("air")),
              breathLength(params.getRawParameterValue("breathLength")),
              vibrato(params.getRawParameterValue("vibrato")),
              humanize(params.getRawParameterValue("humanize")),
              legatoTime(params.getRawParameterValue("legatoTime")),
              vibratoRate(params.getRawParameterValue("vibratoRate")),
              vibratoBreath(params.getRawParameterValue("vibratoBreath")),
              growl(params.getRawParameterValue("growl")),
              flutter(params.getRawParameterValue("flutter")),
              jazzEffect(params.getRawParameterValue("jazzEffect")) {}

        bool canPlaySound(juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<WindSound*>(sound) != nullptr;
        }

        void startNote(int midiNoteNumber, float velocity,
                       juce::SynthesiserSound*, int currentPitchWheelPosition) override
        {
            const auto c = controllerSource.currentlyStartingControls();
            engine.setPitchWheel(currentPitchWheelPosition);
            engine.setModWheel(c.mod);
            engine.setExpression(c.expression);
            engine.setBreath(c.breath, c.breathEnabled);
            engine.setPressure(c.pressure);
            engine.setTone(tone->load());
            engine.setAir(air->load());
            engine.setBreathLength(breathLength->load());
            engine.setVibrato(vibrato->load());
            engine.setHumanize(humanize->load());
            applyExpression();
            engine.setJazzEffect(static_cast<windlab::JazzEffect>(juce::jlimit(0, 4, static_cast<int>(jazzEffect->load()))));
            const auto legatoFrom = controllerSource.currentlyStartingLegatoFrom();
            const auto curvedVelocity = windlab::applyVelocityResponse(
                velocity, juce::jlimit(0, 3, static_cast<int>(velocityResponse->load())));
            const auto instrumentIndex = juce::jlimit(0, 8, static_cast<int>(instrument->load()));
            const auto articulationIndex = juce::jlimit(0, 2, static_cast<int>(articulation->load()));
            modeAtNoteOn = juce::jlimit(0, 2, static_cast<int>(sourceMode->load()));
            const auto* sample = modeAtNoteOn == 2 ? nullptr : sampleSource.select(instrumentIndex, midiNoteNumber, curvedVelocity, articulationIndex);
            engine.setRecordedSource(sample != nullptr);
            engine.start(midiNoteNumber, curvedVelocity,
                         static_cast<windlab::Instrument>(instrumentIndex),
                         attack->load(), release->load(),
                         static_cast<windlab::Articulation>(articulationIndex), legatoFrom);
            // A slurred note joins the sustained body of the recording instead of replaying its attack.
            entryRandom = entryRandom * 1664525u + 1013904223u;
            cursor.start(sample, 440.0 * std::exp2((midiNoteNumber - 69.0) / 12.0), getSampleRate(),
                         legatoFrom > 0.0 && articulationIndex == 0,
                         static_cast<float>((entryRandom >> 8) & 0xffffffu) / 16777216.0f);
        }

        void stopNote(float, bool allowTailOff) override
        {
            if (allowTailOff) engine.noteOff();
            else { engine.reset(); clearCurrentNote(); }
        }

        void yieldToLegato() noexcept override { engine.fastRelease(.05f); }

        void pitchWheelMoved(int newValue) override { engine.setPitchWheel(newValue); }
        void controllerMoved(int controllerNumber, int newValue) override
        {
            if (controllerNumber == 1) engine.setModWheel(newValue / 127.0f);
            if (controllerNumber == 2) engine.setBreath(newValue / 127.0f, true);
            if (controllerNumber == 11) engine.setExpression(newValue / 127.0f);
            if (controllerNumber == 121)
            {
                engine.setModWheel(0.0f);
                engine.setBreath(1.0f, false);
                engine.setExpression(1.0f);
                engine.setPressure(0.0f);
            }
        }
        void aftertouchChanged(int value) override { engine.setPressure(value / 127.0f); }
        void channelPressureChanged(int value) override { engine.setPressure(value / 127.0f); }

        void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override
        {
            render(output, startSample, numSamples);
        }
        void renderNextBlock(juce::AudioBuffer<double>& output, int startSample, int numSamples) override
        {
            render(output, startSample, numSamples);
        }
        void setCurrentPlaybackSampleRate(double newRate) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
            engine.prepare(newRate);
        }

    private:
        template <typename Sample>
        void render(juce::AudioBuffer<Sample>& output, int startSample, int numSamples)
        {
            engine.setTone(tone->load());
            engine.setAir(air->load());
            engine.setBreathLength(breathLength->load());
            engine.setVibrato(vibrato->load());
            engine.setHumanize(humanize->load());
            applyExpression();
            for (int i = 0; i < numSamples; ++i)
            {
                if (!engine.active()) { clearCurrentNote(); break; }
                const float model = engine.render();
                float result = model;
                if (cursor.valid())
                {
                    const float wav = cursor.read(engine.currentFrequency());
                    const float recorded = wav * engine.currentLevel() * 3.6f;
                    result = modeAtNoteOn == 1 ? (.8f * recorded + .20f * model)
                                               : (recorded + .055f * model);
                }
                result += engine.currentClick();
                const auto sample = static_cast<Sample>(result);
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                    output.addSample(channel, startSample + i, sample);
            }
        }
        void applyExpression()
        {
            engine.setGlideTime(legatoTime->load());
            engine.setVibratoRate(vibratoRate->load());
            engine.setVibratoFromBreath(vibratoBreath->load());
            engine.setGrowl(growl->load());
            engine.setFlutter(flutter->load());
        }
        windlab::WindEngine engine;
        PerformanceSynthesiser& controllerSource;
        const windlab::RecordedWinds& sampleSource;
        windlab::RecordedCursor cursor;
        std::atomic<float>* sourceMode;
        std::atomic<float>* velocityResponse;
        int modeAtNoteOn = 0;
        std::uint32_t entryRandom = 0x2545f491u;
        std::atomic<float>* instrument;
        std::atomic<float>* articulation;
        std::atomic<float>* attack;
        std::atomic<float>* release;
        std::atomic<float>* tone;
        std::atomic<float>* air;
        std::atomic<float>* breathLength;
        std::atomic<float>* vibrato;
        std::atomic<float>* humanize;
        std::atomic<float>* legatoTime;
        std::atomic<float>* vibratoRate;
        std::atomic<float>* vibratoBreath;
        std::atomic<float>* growl;
        std::atomic<float>* flutter;
        std::atomic<float>* jazzEffect;
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout WindLabAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"instrument", 1}, "Instrument",
        juce::StringArray {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn",
                           "Soprano Sax", "Alto Sax", "Baritone Sax"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"sourceMode", 1}, "Sound Engine",
        juce::StringArray {"Acoustic (recorded)", "Hybrid (sample + synth)", "Synthetic"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"articulation", 1}, "Articulation",
        juce::StringArray {"Sustain", "Staccato", "Swell"}, 0));
    layout.add(std::make_unique<Float>(juce::ParameterID {"attack", 1}, "Attack",
        juce::NormalisableRange<float>(.005f, 1.5f, .001f, .4f), .04f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"release", 1}, "Release",
        juce::NormalisableRange<float>(.02f, 3.0f, .001f, .5f), .35f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"tone", 1}, "Brightness",
        juce::NormalisableRange<float>(0.0f, 1.0f), .55f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"air", 1}, "Air",
        juce::NormalisableRange<float>(0.0f, 1.0f), .3f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"space", 1}, "Early Reflections",
        juce::NormalisableRange<float>(0.0f, 0.5f), .08f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"breathLength", 1}, "Breath Length",
        juce::NormalisableRange<float>(2.0f, 30.0f, .1f), 30.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"vibrato", 1}, "Vibrato",
        juce::NormalisableRange<float>(0.0f, 1.0f), .09f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"humanize", 1}, "Natural Variation",
        juce::NormalisableRange<float>(0.0f, 1.0f), .35f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"bass", 1}, "Bass dB",
        juce::NormalisableRange<float>(-12.0f, 12.0f, .1f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"mid", 1}, "Mid dB",
        juce::NormalisableRange<float>(-12.0f, 12.0f, .1f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"treble", 1}, "Treble dB",
        juce::NormalisableRange<float>(-12.0f, 12.0f, .1f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"reverb", 1}, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), .16f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"reverbSize", 1}, "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f), .52f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"reverbDamp", 1}, "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f), .48f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"gain", 1}, "Output",
        juce::NormalisableRange<float>(0.0f, 1.0f), .75f));
    // Append new IDs after all original parameters: preserves older host index order.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"velocityResponse", 1}, "Velocity Response",
        juce::StringArray {"Natural", "Light Touch", "Weighted Piano", "Breath Controller"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"inputMode", 1}, "Keyboard Input Mode",
        juce::StringArray {"Piano + MIDI", "PC / Notebook"}, 0));
    // Appended to retain existing parameter indices and saved DAW automation.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"performanceMode", 1}, "MIDI Performance Mode",
        juce::StringArray {"Keyboard / Universal", "Wind MIDI", "Hybrid: Ch1 keys + Ch2 breath"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"noteMidiChannel", 1}, "Hybrid Note MIDI Channel",
        juce::StringArray {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"breathMidiChannel", 1}, "Hybrid Breath MIDI Channel",
        juce::StringArray {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"}, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"breathThreshold", 1}, "Breath Activation Threshold",
        juce::NormalisableRange<float>(0.0f, 0.5f, 0.005f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"breathCurve", 1}, "Breath Response Curve",
        juce::NormalisableRange<float>(0.35f, 3.0f, 0.01f), 1.0f));
    // 0.9.6: breath can also come from Channel Pressure (aftertouch), as in PR #16.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"breathSource", 1}, "Breath Source",
        juce::StringArray {"MIDI CC (MIDI Learn)", "Aftertouch / Channel Pressure"}, 0));
    // 0.9.7 expression (appended: existing automation indices stay the same).
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"legatoMode", 1}, "Legato Mode",
        juce::StringArray {"Auto (legato + chords)", "Always legato (mono)", "Never legato (poly)"}, 0));
    layout.add(std::make_unique<Float>(juce::ParameterID {"legatoTime", 1}, "Legato Time",
        juce::NormalisableRange<float>(.004f, .30f, .001f, .5f), .022f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"vibratoRate", 1}, "Vibrato Rate",
        juce::NormalisableRange<float>(.5f, 1.8f, .01f), 1.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"vibratoBreath", 1}, "Vibrato From Breath",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"growl", 1}, "Growl",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID {"flutter", 1}, "Flutter Tongue",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"jazzEffect", 1}, "Jazz Effect",
        juce::StringArray {"None", "Scoop", "Fall", "Doit", "Shake"}, 0));
    layout.add(std::make_unique<Float>(juce::ParameterID {"pan", 1}, "Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, .01f), 0.0f));
    return layout;
}

WindLabAudioProcessor::WindLabAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("WindLabState"), createParameterLayout()),
      synthesiser(std::make_unique<PerformanceSynthesiser>(
          midiLearn, parameters.getRawParameterValue("articulation"), parameters))
{
    auto& synth = *static_cast<PerformanceSynthesiser*>(synthesiser.get());
    for (int i = 0; i < 12; ++i)
        synthesiser->addVoice(new WindVoice(parameters, synth, sampleBank));
    synthesiser->addSound(new WindSound());
    synthesiser->setMinimumRenderingSubdivisionSize(1, false);
    learnableCount = getParameters().size();
    pendingCCValues = std::make_unique<std::atomic<float>[]>(static_cast<std::size_t>(learnableCount));
    for (int i = 0; i < learnableCount; ++i) pendingCCValues[static_cast<std::size_t>(i)].store(-1.0f);
    startTimerHz(30); // applies learned CC moves to parameters on the message thread
}

int WindLabAudioProcessor::parameterIndexFor(const juce::String& parameterID) const
{
    const auto& all = getParameters();
    for (int i = 0; i < all.size(); ++i)
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(all[i]))
            if (withID->paramID == parameterID) return i;
    return -1;
}

juce::String WindLabAudioProcessor::parameterIDAt(int index) const
{
    const auto& all = getParameters();
    if (index >= 0 && index < all.size())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(all[index]))
            return withID->paramID;
    return {};
}

void WindLabAudioProcessor::timerCallback()
{
    const auto& all = getParameters();
    for (int i = 0; i < learnableCount && i < all.size(); ++i)
    {
        const float value = pendingCCValues[static_cast<std::size_t>(i)].exchange(-1.0f);
        if (value >= 0.0f) all[i]->setValueNotifyingHost(value);
    }
}

bool WindLabAudioProcessor::presetSkips(const juce::String& id)
{
    static const juce::StringArray routing {"inputMode", "performanceMode", "noteMidiChannel", "breathMidiChannel",
                                            "breathThreshold", "breathCurve", "breathSource", "velocityResponse"};
    return routing.contains(id);
}

void WindLabAudioProcessor::setParameterPlain(const juce::String& id, float plainValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(id)))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

namespace
{
    struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<FactoryPreset>& factoryPresets()
    {
        // instrument: 0 trumpet, 1 tenor, 2 trombone, 3 flute, 4 clarinet, 5 horn, 6 soprano, 7 alto, 8 baritone
        // jazzEffect: 0 none, 1 scoop, 2 fall, 3 doit, 4 shake. legatoMode: 0 auto, 1 always, 2 never.
        static const std::vector<FactoryPreset> list {
            {"Sax contralto - ballata", {{"instrument", 7}, {"vibrato", .42f}, {"humanize", .5f}, {"tone", .45f}, {"air", .38f},
                                         {"reverb", .30f}, {"reverbSize", .62f}, {"legatoTime", .045f}, {"vibratoBreath", .6f}}},
            {"Sax tenore - jazz con growl", {{"instrument", 1}, {"tone", .68f}, {"air", .32f}, {"vibrato", .25f}, {"growl", .38f},
                                             {"jazzEffect", 1}, {"reverb", .18f}, {"legatoTime", .03f}}},
            {"Sax soprano - lirico", {{"instrument", 6}, {"vibrato", .5f}, {"vibratoBreath", .5f}, {"reverb", .30f}, {"tone", .5f}}},
            {"Sax baritono - funky", {{"instrument", 8}, {"tone", .7f}, {"growl", .2f}, {"attack", .01f}, {"reverb", .12f}}},
            {"Tromba - big band", {{"instrument", 0}, {"tone", .78f}, {"air", .18f}, {"vibrato", .18f}, {"reverb", .20f}, {"attack", .02f}}},
            {"Tromba - fall finali", {{"instrument", 0}, {"tone", .70f}, {"jazzEffect", 2}, {"reverb", .18f}}},
            {"Tromba - shake", {{"instrument", 0}, {"tone", .74f}, {"jazzEffect", 4}, {"reverb", .18f}}},
            {"Flauto - morbido", {{"instrument", 3}, {"air", .52f}, {"vibrato", .5f}, {"vibratoBreath", .7f}, {"tone", .4f},
                                  {"reverb", .35f}, {"reverbSize", .70f}}},
            {"Flauto - flutter tongue", {{"instrument", 3}, {"flutter", .55f}, {"air", .45f}, {"reverb", .28f}}},
            {"Clarinetto - classico", {{"instrument", 4}, {"vibrato", .04f}, {"humanize", .30f}, {"tone", .50f}, {"reverb", .32f},
                                       {"legatoTime", .02f}}},
            {"Trombone - glissato", {{"instrument", 2}, {"legatoTime", .16f}, {"legatoMode", 1}, {"tone", .55f}, {"reverb", .22f}}},
            {"Corno - orchestrale", {{"instrument", 5}, {"tone", .38f}, {"reverb", .45f}, {"reverbSize", .78f}, {"attack", .07f},
                                     {"vibrato", .05f}}},
        };
        return list;
    }
}

juce::StringArray WindLabAudioProcessor::factoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : factoryPresets()) names.add(preset.name);
    return names;
}

void WindLabAudioProcessor::applyFactoryPreset(int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= static_cast<int>(list.size())) return;
    // Start from the default sound, then apply the preset's own values.
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!presetSkips(ranged->paramID))
                ranged->setValueNotifyingHost(ranged->getDefaultValue());
    for (const auto& [id, value] : list[static_cast<std::size_t>(index)].values)
        setParameterPlain(id, value);
}

juce::File WindLabAudioProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("WindLab").getChildFile("Presets");
}

bool WindLabAudioProcessor::saveUserPreset(const juce::File& file) const
{
    juce::XmlElement xml("WindLabPresetV1");
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!presetSkips(ranged->paramID))
                xml.setAttribute(ranged->paramID, ranged->convertFrom0to1(ranged->getValue()));
    file.getParentDirectory().createDirectory();
    return xml.writeTo(file);
}

bool WindLabAudioProcessor::loadUserPreset(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("WindLabPresetV1")) return false;
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!presetSkips(ranged->paramID) && xml->hasAttribute(ranged->paramID))
                setParameterPlain(ranged->paramID, static_cast<float>(xml->getDoubleAttribute(ranged->paramID)));
    return true;
}

void WindLabAudioProcessor::prepareToPlay(double sampleRate, int)
{
    virtualInstrumentInput.reset(sampleRate);
    virtualInputReady.store(true);
    synthesiser->setCurrentPlaybackSampleRate(sampleRate);
    synthesiser->allNotesOff(0, false);
    static_cast<PerformanceSynthesiser*>(synthesiser.get())->resetControllers();
    keyboardState.reset();
    eq.prepare(sampleRate);
    stereoReverb.setSampleRate(sampleRate);
    stereoReverb.reset();
    reverbMix.reset(sampleRate, .045);
    reverbSize.reset(sampleRate, .075);
    reverbDamping.reset(sampleRate, .075);
    reverbMix.setCurrentAndTargetValue(parameters.getRawParameterValue("reverb")->load());
    reverbSize.setCurrentAndTargetValue(parameters.getRawParameterValue("reverbSize")->load());
    reverbDamping.setCurrentAndTargetValue(parameters.getRawParameterValue("reverbDamp")->load());
    outputGain.reset(sampleRate, .02);
    roomAmount.reset(sampleRate, .03);
    panPosition.reset(sampleRate, .03);
    outputGain.setCurrentAndTargetValue(parameters.getRawParameterValue("gain")->load());
    roomAmount.setCurrentAndTargetValue(parameters.getRawParameterValue("space")->load());
    earlyReflectionL.assign(static_cast<std::size_t>(std::max(1.0, sampleRate * .047)), 0.0f);
    earlyReflectionR.assign(static_cast<std::size_t>(std::max(1.0, sampleRate * .067)), 0.0f);
    reflectionPositionL = reflectionPositionR = 0;
}

void WindLabAudioProcessor::releaseResources()
{
    synthesiser->allNotesOff(0, false);
}

bool WindLabAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WindLabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    double hostQuarterNotes = -1.0;
    if (auto* host = getPlayHead())
        if (const auto position = host->getPosition())
        {
            // If a standalone player has no host, the last known (or 110) BPM is used.
            if (const auto bpm = position->getBpm())
                phrase.setHostTempo(*bpm);
            if (phrase.followsHostTransport())
            {
                // 0.9.7: the phrase starts and stops with the DAW and follows its song position.
                const bool hostPlaying = position->getIsPlaying();
                if (hostPlaying != lastHostPlaying)
                {
                    phrase.setPlaying(hostPlaying);
                    lastHostPlaying = hostPlaying;
                }
                if (hostPlaying)
                    if (const auto ppq = position->getPpqPosition())
                        hostQuarterNotes = juce::jmax(0.0, *ppq);
            }
        }
    if (!phrase.followsHostTransport()) lastHostPlaying = false;

    // Capture external MIDI and virtual PC/piano notes BEFORE the internal
    // Phrase Editor injects its channel-16 playback. The recording is strictly
    // MIDI (notes + CC curves), not a bounced WAV file. Recording and playing
    // use separate buffers so REC never destroys an existing phrase.
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    if (virtualInputReady.load() && buffer.getNumSamples() > 0)
        virtualInstrumentInput.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    // Observe original incoming MIDI before hybrid-channel normalization.
    // The monitor is diagnostic only; it does not modify MIDI or sound.
    for (const auto event : midiMessages)
    {
        const auto message = event.getMessage();
        if (message.isNoteOnOrOff())
            midiInputMonitor.note(message.getChannel(), message.getNoteNumber(), message.isNoteOn());
        else if (message.isController())
        {
            midiInputMonitor.controller(message.getChannel(),
                                        message.getControllerNumber(), message.getControllerValue());
            if (message.getChannel() < 16)
            {
                const int target = parameterCC.handleController(message.getControllerNumber());
                if (target >= 0 && target < learnableCount)
                    pendingCCValues[static_cast<std::size_t>(target)].store(message.getControllerValue() / 127.0f);
            }
        }
        else if (message.isChannelPressure())
            midiInputMonitor.pressure(message.getChannel(), message.getChannelPressureValue());
    }
    const bool pressureBreath = parameters.getRawParameterValue("breathSource")->load() >= .5f;
    const auto calibratedBreath = [this](int value)
    {
        const auto normalized = windlab::calibrateBreath(value / 127.0f,
            parameters.getRawParameterValue("breathThreshold")->load(),
            parameters.getRawParameterValue("breathCurve")->load());
        return juce::jlimit(0, 127, static_cast<int>(std::lround(normalized * 127.0f)));
    };
    // Hybrid combines note input and expression from separate MIDI channels.
    // Same-channel choices are intentionally treated as passthrough.
    if (static_cast<int>(parameters.getRawParameterValue("performanceMode")->load()) == 2)
    {
        const int notesCh = 1 + static_cast<int>(parameters.getRawParameterValue("noteMidiChannel")->load());
        const int breathCh = 1 + static_cast<int>(parameters.getRawParameterValue("breathMidiChannel")->load());
        if (notesCh != breathCh)
        {
            juce::MidiBuffer merged;
            const int vibratoCC = midiLearn.binding(windlab::MidiLearnBindings::Vibrato);
            const int breathCC = midiLearn.binding(windlab::MidiLearnBindings::Breath);
            const int expressionCC = midiLearn.binding(windlab::MidiLearnBindings::Expression);
            for (const auto event : midiMessages)
            {
                auto msg = event.getMessage();
                const auto decision = windlab::routeHybrid(
                    msg.getChannel(), msg.isController(),
                    msg.isController() ? msg.getControllerNumber() : -1,
                    notesCh, breathCh, vibratoCC, breathCC, expressionCC,
                    midiLearn.armedTarget() != windlab::MidiLearnBindings::None,
                    msg.isNoteOff(),
                    pressureBreath && msg.isChannelPressure());
                if (decision.action == windlab::HybridRoute::Drop) continue;
                if (decision.action == windlab::HybridRoute::Remap)
                    msg.setChannel(decision.channel);
                merged.addEvent(msg, event.samplePosition);
            }
            midiMessages.swapWith(merged);
        }
    }
    phraseRecorder.beginBlock(buffer.getNumSamples(), getSampleRate(), phrase.effectiveTempo(), phrase.getGrid());
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int channel = message.getChannel();
        const int offset = metadata.samplePosition;
        if (channel == windlab::PhraseEngine::midiChannel) continue;
        if (message.isNoteOn())
            phraseRecorder.noteOn(channel, message.getNoteNumber(), message.getVelocity(), offset);
        else if (message.isNoteOff())
            phraseRecorder.noteOff(channel, message.getNoteNumber(), offset);
        else if (message.isController())
        {
            // Capture the post-mapping expressive function without consuming
            // the pending MIDI Learn assignment in the audio synthesizer.
            const int cc = message.getControllerNumber();
            const int translated =
                cc == midiLearn.binding(windlab::MidiLearnBindings::Vibrato) ? 1 :
                cc == midiLearn.binding(windlab::MidiLearnBindings::Breath) ? 2 :
                cc == midiLearn.binding(windlab::MidiLearnBindings::Expression) ? 11 :
                (cc == 1 || cc == 2 || cc == 11 ? -1 : cc);
            // Record breath as it is heard (after calibration): phrase playback on
            // channel 16 is excluded from calibration and must sound the same.
            if (translated == 2)
                phraseRecorder.controller(channel, 2, calibratedBreath(message.getControllerValue()), offset);
            else if (translated > 0)
                phraseRecorder.controller(channel, translated, message.getControllerValue(), offset);
        }
        else if (message.isChannelPressure() && pressureBreath)
            phraseRecorder.controller(channel, 2, calibratedBreath(message.getChannelPressureValue()), offset);
        else if (message.isPitchWheel())
            phraseRecorder.pitchWheel(channel, message.getPitchWheelValue(), offset);
    }
    phraseRecorder.endBlock();

    // Phrase Editor uses an isolated channel 16, excluded from MIDI capture.
    // Playback and automation remain unchanged and independent of the recorder.
    phrase.process(buffer.getNumSamples(), getSampleRate(),
        [&midiMessages](windlab::PhraseEvent type, int noteOrController, int value, int offset)
        {
            switch (type)
            {
                case windlab::PhraseEvent::NoteOn:
                    midiMessages.addEvent(juce::MidiMessage::noteOn(
                        windlab::PhraseEngine::midiChannel, noteOrController,
                        static_cast<juce::uint8>(value)), offset);
                    break;
                case windlab::PhraseEvent::NoteOff:
                    midiMessages.addEvent(juce::MidiMessage::noteOff(
                        windlab::PhraseEngine::midiChannel, noteOrController), offset);
                    break;
                case windlab::PhraseEvent::Breath:
                case windlab::PhraseEvent::Expression:
                case windlab::PhraseEvent::ModWheel:
                    midiMessages.addEvent(juce::MidiMessage::controllerEvent(
                        windlab::PhraseEngine::midiChannel, noteOrController, value), offset);
                    break;
                case windlab::PhraseEvent::PitchWheel:
                    midiMessages.addEvent(juce::MidiMessage::pitchWheel(
                        windlab::PhraseEngine::midiChannel, value), offset);
                    break;
            }
        }, hostQuarterNotes);
    synthesiser->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    outputGain.setTargetValue(parameters.getRawParameterValue("gain")->load());
    roomAmount.setTargetValue(parameters.getRawParameterValue("space")->load());
    panPosition.setTargetValue(parameters.getRawParameterValue("pan")->load());
    eq.setGains(parameters.getRawParameterValue("bass")->load(),
                parameters.getRawParameterValue("mid")->load(),
                parameters.getRawParameterValue("treble")->load());

    // Two subtle, distinct, feedback-limited stereo early reflections; not a convolution reverb.
    // Buffers are preallocated in prepareToPlay and never resized in processBlock.
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Equalize the dry voice before sending it into the ambient processors.
        const float panValue = panPosition.getNextValue();
        left[i] = eq.process(0, left[i]) * juce::jmin(1.0f, 1.0f - panValue);
        right[i] = eq.process(1, right[i]) * juce::jmin(1.0f, 1.0f + panValue);
        const float dry = .5f * (left[i] + right[i]);
        const float echoL = earlyReflectionL[reflectionPositionL];
        const float echoR = earlyReflectionR[reflectionPositionR];
        earlyReflectionL[reflectionPositionL] = dry * .45f + echoL * .22f;
        earlyReflectionR[reflectionPositionR] = dry * .42f + echoR * .20f;
        if (++reflectionPositionL >= earlyReflectionL.size()) reflectionPositionL = 0;
        if (++reflectionPositionR >= earlyReflectionR.size()) reflectionPositionR = 0;
        const float wet = roomAmount.getNextValue();
        left[i] += echoL * wet;
        right[i] += echoR * wet;
    }

    // JUCE's stereo algorithmic reverb is pre-initialized in prepareToPlay.
    // Parameters move smoothly on the audio thread, without allocating buffers.
    const int samples = buffer.getNumSamples();
    reverbMix.setTargetValue(parameters.getRawParameterValue("reverb")->load());
    reverbSize.setTargetValue(parameters.getRawParameterValue("reverbSize")->load());
    reverbDamping.setTargetValue(parameters.getRawParameterValue("reverbDamp")->load());
    juce::Reverb::Parameters settings;
    const float mix = reverbMix.skip(samples);
    settings.roomSize = .12f + .83f * reverbSize.skip(samples);
    settings.damping = .12f + .82f * reverbDamping.skip(samples);
    settings.wetLevel = .55f * mix;
    settings.dryLevel = .5f; // JUCE FreeVerb doubles dryLevel internally; 0.5 = unity
    settings.width = 1.0f;
    stereoReverb.setParameters(settings);
    stereoReverb.processStereo(left, right, samples);

    // Apply master gain and protect against stacked-note peaks.
    for (int i = 0; i < samples; ++i)
    {
        const float gain = outputGain.getNextValue();
        left[i] = std::tanh(left[i] * gain);
        right[i] = std::tanh(right[i] * gain);
    }
}

juce::AudioProcessorEditor* WindLabAudioProcessor::createEditor()
{
    return new WindLabAudioProcessorEditor(*this);
}

void WindLabAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
    {
        // Store phrase notes inside the existing state blob; older automation IDs
        // remain intact. UI/audio handoff consists only of atomic reads.
        auto* phraseXml = xml->createNewChildElement("WindLabPhraseV1");
        phraseXml->setAttribute("tempo", phrase.getTempo());
        phraseXml->setAttribute("followHostTempo", phrase.followsHostTempo() ? 1 : 0);
        phraseXml->setAttribute("followHostTransport", phrase.followsHostTransport() ? 1 : 0);
        // V2 keeps the old s0..s15 attributes (slot 0 of every step) so older builds
        // still load the primary melody; chords and grid are added alongside.
        phraseXml->setAttribute("grid", static_cast<int>(phrase.getGrid()));
        for (int i = 0; i < windlab::PhraseEngine::maxSteps; ++i)
        {
            phraseXml->setAttribute("s" + juce::String(i),
                juce::String(static_cast<juce::int64>(phrase.packed(i))));
            for (int slot = 1; slot < windlab::PhraseEngine::chordSlots; ++slot)
                if (phrase.chordNote(i, slot).active)
                    phraseXml->setAttribute("c" + juce::String(i) + "_" + juce::String(slot),
                        juce::String(static_cast<juce::int64>(phrase.chordPacked(i, slot))));
        }
        // Separate versioned node: old projects load their previous sound,
        // while MIDI Learn starts with standard CC1/CC2/CC11 defaults.
        auto* learnXml = xml->createNewChildElement("WindLabMidiLearnV1");
        learnXml->setAttribute("ccMap", static_cast<int>(midiLearn.snapshot()));
        // Universal MIDI Learn: stored by parameter ID, robust to future parameters.
        auto* ccXml = xml->createNewChildElement("WindLabCCMapV1");
        for (const auto& [cc, index] : parameterCC.assignments())
            ccXml->setAttribute("cc" + juce::String(cc), parameterIDAt(index));
        copyXmlToBinary(*xml, destData);
    }
}

void WindLabAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (!xml->hasTagName(parameters.state.getType())) return;
        phrase.setPlaying(false); // project recall must never auto-start a phrase
        phrase.clear();
        phrase.setFollowHostTempo(false); // old sessions keep their v0.7 tempo
        midiLearn.reset(); // old sessions preserve default CC assignments
        parameterCC.clearAll();
        if (auto* ccXml = xml->getChildByName("WindLabCCMapV1"))
        {
            for (int i = 0; i < ccXml->getNumAttributes(); ++i)
            {
                const auto name = ccXml->getAttributeName(i);
                const int index = parameterIndexFor(ccXml->getAttributeValue(i));
                if (name.startsWith("cc") && index >= 0)
                    parameterCC.assign(name.substring(2).getIntValue(), index);
            }
            xml->removeChildElement(ccXml, true);
        }
        if (auto* phraseXml = xml->getChildByName("WindLabPhraseV1"))
        {
            phrase.setTempo(phraseXml->getIntAttribute("tempo", 110));
            phrase.setFollowHostTempo(phraseXml->getBoolAttribute("followHostTempo", false));
            phrase.setFollowHostTransport(phraseXml->getBoolAttribute("followHostTransport", false));
            // Projects saved before V2 have no grid attribute: they load as eighths.
            const int gridValue = phraseXml->getIntAttribute("grid", 0);
            phrase.setGrid(static_cast<windlab::PhraseGrid>(std::clamp(gridValue, 0, 3)));
            for (int i = 0; i < windlab::PhraseEngine::maxSteps; ++i)
            {
                const auto name = "s" + juce::String(i);
                if (phraseXml->hasAttribute(name))
                    phrase.setPacked(i, static_cast<std::uint64_t>(
                        phraseXml->getStringAttribute(name).getLargeIntValue()));
                for (int slot = 1; slot < windlab::PhraseEngine::chordSlots; ++slot)
                {
                    const auto chord = "c" + juce::String(i) + "_" + juce::String(slot);
                    if (phraseXml->hasAttribute(chord))
                        phrase.setChordPacked(i, slot, static_cast<std::uint64_t>(
                            phraseXml->getStringAttribute(chord).getLargeIntValue()));
                }
            }
            // Keep phrase metadata out of the APVTS parameter state tree.
            xml->removeChildElement(phraseXml, true);
        }
        if (auto* learnXml = xml->getChildByName("WindLabMidiLearnV1"))
        {
            midiLearn.restore(static_cast<std::uint32_t>(
                learnXml->getIntAttribute("ccMap", static_cast<int>(windlab::MidiLearnBindings::defaults()))));
            xml->removeChildElement(learnXml, true);
        }
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WindLabAudioProcessor();
}
