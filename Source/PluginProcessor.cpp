#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Dsp/WindEngine.h"
#include "Dsp/AcousticExpression.h"
#include "Dsp/DynamicLayerMorph.h"
#include "Dsp/LegatoHandoff.h"
#include "Dsp/VelocityResponse.h"
#include "Dsp/HybridMidiRoute.h"
#include "Dsp/BreathCalibration.h"
#include "Dsp/SectionMode.h"
#include "Dsp/RoomImpulse.h"
#if WINDLAB_HAS_RECORDED_ROOMS
  #include <WindLabRoomData.h>
#endif
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
        virtual void yieldToLegato(float seconds) noexcept = 0;
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
              legatoMode(params.getRawParameterValue("legatoMode")),
              unison(params.getRawParameterValue("unison")),
              mpeMode(params.getRawParameterValue("mpeMode")),
              mpeBendRange(params.getRawParameterValue("mpeBendRange")) {}
        int currentlyStartingDoubler() const noexcept { return startingDoubler; }
        int currentlyStartingChannel() const noexcept { return startingChannel; }
        int currentUnison() const noexcept
        {
            return unison != nullptr ? juce::jlimit(1, 3, 1 + static_cast<int>(unison->load())) : 1;
        }
        // Frequency of the note being slurred from, or 0 when the new note starts fresh.
        double currentlyStartingLegatoFrom() const noexcept { return legatoFrom; }
        float currentlyStartingLegatoSeconds() const noexcept { return legatoSeconds; }
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
            legatoSeconds = 0.0f;
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
                legatoSeconds = windlab::LegatoHandoff::durationForSemitones(
                    midiNoteNumber - lastNote[channelIndex]);
                for (auto* voice : voices)
                    if (voice->isVoiceActive() && voice->isPlayingChannel(midiChannel) && voice->isKeyDown())
                        if (auto* yielding = dynamic_cast<LegatoYield*>(voice))
                            yielding->yieldToLegato(legatoSeconds);
            }
            lastNote[channelIndex] = midiNoteNumber;
            lastNoteOnSample[channelIndex] = renderedSamples;
            heldKeys[channelIndex] = juce::jmin(16, heldKeys[channelIndex] + 1);
            startingDoubler = 0;
            juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
            // 0.9.9 unison: extra players on the same note. Synthesiser::noteOn would stop
            // a voice already playing this note, so the doublers are started directly;
            // noteOff still releases all of them together.
            const int players = currentUnison();
            if (players > 1 && sounds.size() > 0)
                for (int k = 1; k < players; ++k)
                {
                    startingDoubler = k;
                    if (auto* voice = findFreeVoice(sounds.getFirst().get(), midiChannel, midiNoteNumber, isNoteStealingEnabled()))
                        startVoice(voice, sounds.getFirst().get(), midiChannel, midiNoteNumber, velocity);
                }
            startingDoubler = 0;
            legatoFrom = 0.0;
            legatoSeconds = 0.0f;
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
            // 0.9.10 MPE: the master channel (1) controls every note of the zone (2-15).
            if (mpeOn() && midiChannel == 1)
                for (int member = 2; member <= 15; ++member)
                    applyController(member, controllerNumber, controllerValue);
            applyController(midiChannel, controllerNumber, controllerValue);
        }
        bool mpeOn() const noexcept { return mpeMode != nullptr && mpeMode->load() >= .5f; }
        double bendRangeFor(int midiChannel) const noexcept
        {
            static constexpr double ranges[4] {2.0, 12.0, 24.0, 48.0};
            if (!mpeOn() || midiChannel < 2 || midiChannel > 15) return 2.0;
            return ranges[juce::jlimit(0, 3, static_cast<int>(mpeBendRange->load()))];
        }
        void applyController(int midiChannel, int controllerNumber, int controllerValue)
        {
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
                // MPE: the pressure of each finger is that note's breath.
                const bool mpePressure = mpeOn() && midiChannel >= 2 && midiChannel <= 15;
                if (midiChannel < 16 && ((breathSource != nullptr && static_cast<int>(breathSource->load()) == 1) || mpePressure))
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
        std::atomic<float>* unison;
        std::atomic<float>* mpeMode;
        std::atomic<float>* mpeBendRange;
        int startingDoubler = 0;
        std::array<ChannelControls, 16> channels{};
        std::array<int, 16> heldKeys{};
        std::array<int, 16> lastNote{};
        std::array<std::int64_t, 16> lastNoteOnSample{};
        std::int64_t renderedSamples = 0;
        double legatoFrom = 0.0;
        float legatoSeconds = 0.0f;
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
              jazzEffect(params.getRawParameterValue("jazzEffect")),
              sectionMode(params.getRawParameterValue("sectionMode")) {}

        bool canPlaySound(juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<WindSound*>(sound) != nullptr;
        }

        void startNote(int midiNoteNumber, float velocity,
                       juce::SynthesiserSound*, int currentPitchWheelPosition) override
        {
            const auto c = controllerSource.currentlyStartingControls();
            engine.setPitchWheel(currentPitchWheelPosition);
            engine.setBendRange(controllerSource.bendRangeFor(controllerSource.currentlyStartingChannel()));
            timbreOffset = 0.0f;
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
            // 0.9.8 section mode: the register decides which player of the section plays.
            const auto section = static_cast<windlab::SectionMode>(juce::jlimit(0, 4, static_cast<int>(sectionMode->load())));
            const auto instrumentIndex = windlab::sectionInstrumentFor(
                section, midiNoteNumber, juce::jlimit(0, 8, static_cast<int>(instrument->load())));
            // 0.9.9 unison: each extra player is a few cents off, a little late and
            // placed beside the first one; together they keep the same loudness.
            const int doubler = controllerSource.currentlyStartingDoubler();
            const int players = controllerSource.currentUnison();
            static constexpr float detuneCents[3] {0.0f, 9.0f, -11.0f};
            static constexpr float spread[3] {0.0f, -.30f, .30f};
            entryRandom = entryRandom * 1664525u + 1013904223u;
            const float jitter = static_cast<float>((entryRandom >> 8) & 0xffffffu) / 16777216.0f;
            engine.setDetune(detuneCents[juce::jlimit(0, 2, doubler)] / 100.0);
            startDelay = doubler == 0 ? 0 : juce::roundToInt(getSampleRate() * (.010 * doubler + .006 * jitter));
            unisonGain = 1.0f / std::sqrt(static_cast<float>(players));
            const float seat = juce::jlimit(-1.0f, 1.0f, windlab::sectionPanFor(section, instrumentIndex)
                                                         + (players > 1 ? spread[juce::jlimit(0, 2, doubler)] : 0.0f));
            seatGainL = juce::jmin(1.0f, 1.0f - seat);
            seatGainR = juce::jmin(1.0f, 1.0f + seat);
            const auto articulationIndex = juce::jlimit(0, 2, static_cast<int>(articulation->load()));
            modeAtNoteOn = juce::jlimit(0, 2, static_cast<int>(sourceMode->load()));
            const auto* sample = modeAtNoteOn == 2 ? nullptr : sampleSource.select(instrumentIndex, midiNoteNumber, curvedVelocity, articulationIndex);
            // Two real velocity layers may share a pitch zone. Never mix unrelated
            // root notes or one-shot staccatos; those keep the old single cursor.
            const auto* companion = sampleSource.selectCompanion(sample);
            dynamicLayerMorph.reset(companion != nullptr && sample->velocityLayer > companion->velocityLayer);
            engine.setRecordedSource(sample != nullptr);
            engine.start(midiNoteNumber, curvedVelocity,
                         static_cast<windlab::Instrument>(instrumentIndex),
                         attack->load(), release->load(),
                         static_cast<windlab::Articulation>(articulationIndex), legatoFrom);
            // Instrument-specific timbre from the genuine recording. A slur
            // enters its sustain body and must not replay a tongue transient.
            acousticExpression.setInstrument(instrumentIndex);
            acousticExpression.reset(c.breathEnabled ? c.breath : 1.0f,
                                     juce::jlimit(0.0f, 1.0f, tone->load() + timbreOffset),
                                     legatoFrom > 0.0 && articulationIndex == 0);
            // A slurred note joins the sustained body of the recording instead of replaying its attack.
            entryRandom = entryRandom * 1664525u + 1013904223u;
            const auto noteFrequency = 440.0 * std::exp2((midiNoteNumber - 69.0) / 12.0);
            const auto entryFraction = static_cast<float>((entryRandom >> 8) & 0xffffffu) / 16777216.0f;
            const bool slurred = legatoFrom > 0.0 && articulationIndex == 0;
            cursor.start(sample, noteFrequency, getSampleRate(), slurred, entryFraction);
            companionCursor.start(companion, noteFrequency, getSampleRate(), slurred, entryFraction,
                                  sample != nullptr ? sample->companionAlignment.companionAdvance : 0.0);
            legatoHandoff.start(slurred && sample != nullptr,
                                controllerSource.currentlyStartingLegatoSeconds());
        }

        void stopNote(float, bool allowTailOff) override
        {
            if (allowTailOff) engine.noteOff();
            else { engine.reset(); clearCurrentNote(); }
        }

        void yieldToLegato(float seconds) noexcept override { engine.fastRelease(seconds); }

        void pitchWheelMoved(int newValue) override { engine.setPitchWheel(newValue); }
        void controllerMoved(int controllerNumber, int newValue) override
        {
            if (controllerNumber == 1) engine.setModWheel(newValue / 127.0f);
            if (controllerNumber == 2) engine.setBreath(newValue / 127.0f, true);
            if (controllerNumber == 11) engine.setExpression(newValue / 127.0f);
            // MPE "timbre" (slide, CC74): brighter or darker for this note only.
            if (controllerNumber == 74 && controllerSource.mpeOn())
                timbreOffset = (static_cast<float>(newValue) - 64.0f) / 64.0f * .45f;
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
            acousticExpression.prepare(newRate);
            dynamicLayerMorph.prepare(newRate);
            legatoHandoff.prepare(newRate);
        }

    private:
        template <typename Sample>
        void render(juce::AudioBuffer<Sample>& output, int startSample, int numSamples)
        {
            const float acousticTone = juce::jlimit(0.0f, 1.0f, tone->load() + timbreOffset);
            engine.setTone(acousticTone);
            engine.setAir(air->load());
            engine.setBreathLength(breathLength->load());
            engine.setVibrato(vibrato->load());
            engine.setHumanize(humanize->load());
            applyExpression();
            for (int i = 0; i < numSamples; ++i)
            {
                if (startDelay > 0) { --startDelay; continue; } // late unison player
                if (!engine.active()) { clearCurrentNote(); break; }
                const float model = engine.render();
                float result = model;
                if (cursor.valid())
                {
                    const auto frequency = engine.currentFrequency();
                    float wave = cursor.read(frequency);
                    if (companionCursor.valid())
                    {
                        // Both sample heads advance continuously, even when one is silent;
                        // a slow crossfade keeps dynamic changes click-free.
                        const float second = companionCursor.read(frequency);
                        const float primaryWeight = dynamicLayerMorph.primaryWeight(
                            engine.currentBreathIntensity(), engine.hasLiveBreath());
                        wave = primaryWeight * wave + (1.0f - primaryWeight) * second;
                    }
                    const float wav = acousticExpression.process(
                        wave, engine.currentBreathIntensity(), acousticTone);
                    const float recorded = wav * engine.currentLevel() * 3.6f;
                    result = modeAtNoteOn == 1 ? (.8f * recorded + .20f * model)
                                               : (recorded + .055f * model);
                }
                result += engine.currentClick();
                const auto sample = static_cast<Sample>(result * unisonGain * legatoHandoff.nextGain());
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                    output.addSample(channel, startSample + i,
                                     sample * static_cast<Sample>(channel == 0 ? seatGainL : seatGainR));
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
        windlab::AcousticExpression acousticExpression;
        windlab::DynamicLayerMorph dynamicLayerMorph;
        windlab::LegatoHandoff legatoHandoff;
        PerformanceSynthesiser& controllerSource;
        const windlab::RecordedWinds& sampleSource;
        windlab::RecordedCursor cursor;
        windlab::RecordedCursor companionCursor;
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
        std::atomic<float>* sectionMode;
        float seatGainL = 1.0f, seatGainR = 1.0f, unisonGain = 1.0f, timbreOffset = 0.0f;
        int startDelay = 0;
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
        juce::StringArray {"MIDI CC (MIDI Learn)", "Aftertouch / Channel Pressure", "Microphone (blow)"}, 0));
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
    // 0.9.8: microphone breath, section mode and convolution rooms.
    layout.add(std::make_unique<Float>(juce::ParameterID {"micSensitivity", 1}, "Microphone Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f), .5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"sectionMode", 1}, "Section Mode",
        juce::StringArray {"Off", "Sax section", "Brass section", "Woodwinds", "Big band"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"reverbType", 1}, "Reverb Type",
        juce::StringArray {"Algorithmic", "Studio", "Jazz club", "Concert hall", "Church", "Custom WAV"}, 0));
    // 0.9.10 MPE (zone: master channel 1, notes on channels 2-15; 16 stays the Phrase Editor).
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"mpeMode", 1}, "MPE", juce::StringArray {"Off", "On (channels 2-15)"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"mpeBendRange", 1}, "MPE Bend Range",
        juce::StringArray {"2 semitones", "12 semitones", "24 semitones", "48 semitones"}, 3));
    // 0.9.9: unison players on every note.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"unison", 1}, "Unison Players",
        juce::StringArray {"1 player", "2 players", "3 players"}, 0));
    return layout;
}

WindLabAudioProcessor::WindLabAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Breath Mic", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("WindLabState"), createParameterLayout()),
      synthesiser(std::make_unique<PerformanceSynthesiser>(
          midiLearn, parameters.getRawParameterValue("articulation"), parameters))
{
    auto& synth = *static_cast<PerformanceSynthesiser*>(synthesiser.get());
    for (int i = 0; i < 16; ++i) // 4-note chords x up to 3 unison players, plus tails
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

namespace
{
    // Embedded OpenAIR response for a WindLab room, or nullptr (generated room).
    const char* recordedRoomResource(int type)
    {
#if WINDLAB_HAS_RECORDED_ROOMS
        static constexpr const char* names[5] {nullptr, "room_studio_wav", "room_club_wav", "room_hall_wav", "room_church_wav"};
        if (type >= 1 && type <= 4)
        {
            int bytes = 0;
            if (WindLabRoomData::getNamedResource(names[type], bytes) != nullptr && bytes > 0)
                return names[type];
        }
#else
        juce::ignoreUnused(type);
#endif
        return nullptr;
    }
}

juce::String WindLabAudioProcessor::roomDescription(int type) const
{
    if (type <= 0) return "riverbero algoritmico";
    if (type == customRoomType)
    {
        const auto file = getCustomRoomFile();
        return file.existsAsFile() ? "file: " + file.getFileName() : juce::String("nessun file: premi CARICA SALA WAV");
    }
    return recordedRoomResource(type) != nullptr ? juce::String("registrata (OpenAIR, CC BY 4.0)")
                                                 : juce::String("generata da WindLab");
}

void WindLabAudioProcessor::setCustomRoomFile(const juce::File& file)
{
    {
        const juce::ScopedLock lock(customRoomLock);
        customRoomFile = file;
    }
    ++customRoomRevision;
}

juce::File WindLabAudioProcessor::getCustomRoomFile() const
{
    const juce::ScopedLock lock(customRoomLock);
    return customRoomFile;
}

void WindLabAudioProcessor::updateRoomImpulse()
{
    // Message thread only. A key packs type, size and damping (in 5% steps) so the
    // impulse is rebuilt only when the sound really changes, and only once the knobs
    // have been still for ~200 ms (no rebuilds while dragging).
    const double rate = preparedSampleRate.load();
    if (rate <= 0.0) return;
    const int type = juce::jlimit(0, customRoomType, static_cast<int>(parameters.getRawParameterValue("reverbType")->load()));
    const int size = juce::roundToInt(parameters.getRawParameterValue("reverbSize")->load() * 20.0f);
    const int damp = juce::roundToInt(parameters.getRawParameterValue("reverbDamp")->load() * 20.0f);
    const char* recorded = recordedRoomResource(type);
    // Recorded and custom rooms do not depend on SIZE / DAMPING (they are real rooms).
    const int key = type == 0 ? 0
                  : type == customRoomType ? 500000 + customRoomRevision
                  : recorded != nullptr ? type * 10000 + 9999
                  : type * 10000 + size * 100 + damp;
    if (key != roomPendingKey) { roomPendingKey = key; roomStableTicks = 0; return; }
    if (key == roomLoadedKey.load() || ++roomStableTicks < 6) return;
    if (type == 0) { roomLoadedKey.store(0); return; }
    if (type == customRoomType)
    {
        const auto file = getCustomRoomFile();
        if (!file.existsAsFile()) { roomLoadedKey.store(-1); roomPendingKey = -2; return; } // algorithmic until a file is chosen
        convolution.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes,
                                        0, juce::dsp::Convolution::Normalise::yes);
        roomLoadedKey.store(key);
        return;
    }
#if WINDLAB_HAS_RECORDED_ROOMS
    if (recorded != nullptr)
    {
        int bytes = 0;
        if (const auto* data = WindLabRoomData::getNamedResource(recorded, bytes))
        {
            convolution.loadImpulseResponse(data, static_cast<std::size_t>(bytes), juce::dsp::Convolution::Stereo::yes,
                                            juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
            roomLoadedKey.store(key);
            return;
        }
    }
#endif
    std::vector<float> l, r;
    windlab::generateRoomImpulse(static_cast<windlab::RoomType>(type), rate,
                                 static_cast<float>(size) / 20.0f, static_cast<float>(damp) / 20.0f, l, r);
    if (l.empty()) return;
    juce::AudioBuffer<float> impulse(2, static_cast<int>(l.size()));
    impulse.copyFrom(0, 0, l.data(), static_cast<int>(l.size()));
    impulse.copyFrom(1, 0, r.data(), static_cast<int>(r.size()));
    convolution.loadImpulseResponse(std::move(impulse), rate, juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    roomLoadedKey.store(key);
}

void WindLabAudioProcessor::timerCallback()
{
    updateRoomImpulse();
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
                                            "breathThreshold", "breathCurve", "breathSource", "velocityResponse", "mpeMode", "mpeBendRange",
                                            "micSensitivity"};
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
                                     {"vibrato", .05f}, {"reverbType", 3}}},
            // 0.9.8: sections and convolution rooms (sectionMode 1 sax, 2 brass, 3 woodwinds, 4 big band;
            // reverbType 1 studio, 2 jazz club, 3 concert hall, 4 church).
            {"Sezione sax - club jazz", {{"instrument", 7}, {"sectionMode", 1}, {"reverbType", 2}, {"reverb", .28f},
                                         {"vibrato", .22f}, {"legatoMode", 2}, {"humanize", .55f}}},
            {"Sezione ottoni - fanfara", {{"instrument", 0}, {"sectionMode", 2}, {"reverbType", 3}, {"reverb", .30f},
                                          {"tone", .70f}, {"legatoMode", 2}}},
            {"Big band - sala", {{"instrument", 0}, {"sectionMode", 4}, {"reverbType", 3}, {"reverb", .26f},
                                 {"tone", .66f}, {"legatoMode", 2}, {"humanize", .5f}, {"unison", 1}}},
            {"Flauti - unisono a tre", {{"instrument", 3}, {"unison", 2}, {"reverbType", 3}, {"reverb", .34f},
                                            {"vibrato", .35f}, {"air", .4f}}},
            {"Legni - cameristico", {{"instrument", 3}, {"sectionMode", 3}, {"reverbType", 1}, {"reverb", .30f},
                                     {"vibrato", .3f}, {"legatoMode", 2}}},
            {"Flauto in chiesa", {{"instrument", 3}, {"reverbType", 4}, {"reverb", .42f}, {"air", .45f}, {"vibrato", .45f}}},
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

void WindLabAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    maxBlockSize = juce::jmax(64, samplesPerBlock);
    micFollower.prepare(sampleRate);
    lastMicBreathCC = -1;
    convolution.reset();
    convolution.prepare({sampleRate, static_cast<juce::uint32>(maxBlockSize), 2});
    wetBuffer.setSize(2, maxBlockSize, false, true, true);
    preparedSampleRate.store(sampleRate);
    roomLoadedKey.store(-1); // regenerate the room for this sample rate
    updateRoomImpulse();
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
    // Output stereo; the optional breath microphone may be mono, stereo or switched off.
    const auto input = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (input.isDisabled() || input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void WindLabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    // 0.9.8: read the breath microphone (if any) before the buffer becomes the output.
    float micBreath = 0.0f;
    if (getTotalNumInputChannels() > 0 && buffer.getNumSamples() > 0)
    {
        micFollower.setSensitivity(parameters.getRawParameterValue("micSensitivity")->load());
        micBreath = micFollower.process(buffer.getReadPointer(0), buffer.getNumSamples());
    }
    micBreathLevel.store(micBreath);
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
    // Notes from the on-screen instrument and the microphone breath are added after the
    // diagnostic monitor and the universal MIDI Learn, so they are never 'learned' by mistake.
    if (virtualInputReady.load() && buffer.getNumSamples() > 0)
        virtualInstrumentInput.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    // Microphone breath becomes the breath CC (the one chosen in MIDI LEARN) on the
    // note channel, so it is calibrated, routed and recorded like a breath controller.
    if (static_cast<int>(parameters.getRawParameterValue("breathSource")->load()) == breathFromMicrophone)
    {
        const int value = juce::jlimit(0, 127, juce::roundToInt(micBreath * 127.0f));
        if (value != lastMicBreathCC)
        {
            const bool hybrid = static_cast<int>(parameters.getRawParameterValue("performanceMode")->load()) == 2;
            const int channel = hybrid ? 1 + static_cast<int>(parameters.getRawParameterValue("noteMidiChannel")->load()) : 1;
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(
                channel, midiLearn.binding(windlab::MidiLearnBindings::Breath), value), 0);
            lastMicBreathCC = value;
        }
    }
    else
        lastMicBreathCC = -1;
    const bool pressureBreath = static_cast<int>(parameters.getRawParameterValue("breathSource")->load()) == 1;
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
    phraseRecorder.beginBlock(buffer.getNumSamples(), getSampleRate(), phrase.effectiveTempo(), phrase.getGrid(), phrase.getBars());
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
    for (const auto event : midiMessages)
    {
        const auto message = event.getMessage();
        if (message.isNoteOn())
            displayNote.store(message.getNoteNumber(), std::memory_order_relaxed);
        else if (message.isNoteOff() && message.getNoteNumber() == displayNote.load(std::memory_order_relaxed))
            displayNote.store(-1, std::memory_order_relaxed);
    }
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
    const int roomType = juce::jlimit(0, customRoomType, static_cast<int>(parameters.getRawParameterValue("reverbType")->load()));
    if (roomType == 0 || roomLoadedKey.load() < 0)
    {
        stereoReverb.setParameters(settings);
        stereoReverb.processStereo(left, right, samples);
    }
    else
    {
        // 0.9.8 convolution room: the dry signal stays at unity, the room is added on top.
        // Processed in chunks no longer than the size prepared in prepareToPlay.
        const float wetGain = .62f * mix;
        for (int start = 0; start < samples; start += maxBlockSize)
        {
            const int count = juce::jmin(maxBlockSize, samples - start);
            wetBuffer.copyFrom(0, 0, left + start, count);
            wetBuffer.copyFrom(1, 0, right + start, count);
            auto block = juce::dsp::AudioBlock<float>(wetBuffer).getSubBlock(0, static_cast<std::size_t>(count));
            convolution.process(juce::dsp::ProcessContextReplacing<float>(block));
            juce::FloatVectorOperations::addWithMultiply(left + start, wetBuffer.getReadPointer(0), wetGain, count);
            juce::FloatVectorOperations::addWithMultiply(right + start, wetBuffer.getReadPointer(1), wetGain, count);
        }
    }

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
        phraseXml->setAttribute("bars", phrase.getBars());
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
        if (getCustomRoomFile() != juce::File())
            xml->createNewChildElement("WindLabRoomV1")->setAttribute("customFile", getCustomRoomFile().getFullPathName());
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
        phrase.setBars(2);
        phrase.setFollowHostTempo(false); // old sessions keep their v0.7 tempo
        midiLearn.reset(); // old sessions preserve default CC assignments
        parameterCC.clearAll();
        if (auto* roomXml = xml->getChildByName("WindLabRoomV1"))
        {
            const auto path = roomXml->getStringAttribute("customFile");
            if (juce::File::isAbsolutePath(path)) setCustomRoomFile(juce::File(path));
            xml->removeChildElement(roomXml, true);
        }
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
            phrase.setBars(phraseXml->getIntAttribute("bars", 2)); // older projects: 2 bars
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
