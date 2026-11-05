#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Dsp/WindEngine.h"
#include "Dsp/VelocityResponse.h"
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

    // Cache CC events in MIDI-event order, including CCs received before note-on.
    // JUCE's base Synthesiser dispatches controllers to currently active voices only.
    class PerformanceSynthesiser final : public juce::Synthesiser
    {
    public:
        explicit PerformanceSynthesiser(windlab::MidiLearnBindings& learned)
            : ccAssignments(learned) {}
        ChannelControls currentlyStartingControls() const noexcept
        {
            return channels[static_cast<std::size_t>(juce::jlimit(1, 16, startingChannel) - 1)];
        }
        void resetControllers()
        {
            channels.fill(ChannelControls{});
            startingChannel = 1;
        }
        void noteOn(int midiChannel, int midiNoteNumber, float velocity) override
        {
            startingChannel = midiChannel;
            juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
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
            juce::Synthesiser::handleController(midiChannel, controllerNumber, controllerValue);
        }
        void handleChannelPressure(int midiChannel, int value) override
        {
            if (juce::isPositiveAndBelow(midiChannel - 1, 16))
                channels[static_cast<std::size_t>(midiChannel - 1)].pressure = value / 127.0f;
            juce::Synthesiser::handleChannelPressure(midiChannel, value);
        }
    private:
        windlab::MidiLearnBindings& ccAssignments;
        std::array<ChannelControls, 16> channels{};
        int startingChannel = 1;
    };

    class WindSound final : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    class WindVoice final : public juce::SynthesiserVoice
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
              humanize(params.getRawParameterValue("humanize")) {}

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
            const auto curvedVelocity = windlab::applyVelocityResponse(
                velocity, juce::jlimit(0, 3, static_cast<int>(velocityResponse->load())));
            const auto instrumentIndex = juce::jlimit(0, 5, static_cast<int>(instrument->load()));
            const auto articulationIndex = juce::jlimit(0, 2, static_cast<int>(articulation->load()));
            engine.start(midiNoteNumber, curvedVelocity,
                         static_cast<windlab::Instrument>(instrumentIndex),
                         attack->load(), release->load(),
                         static_cast<windlab::Articulation>(articulationIndex));
            modeAtNoteOn = juce::jlimit(0, 2, static_cast<int>(sourceMode->load()));
            const auto* sample = modeAtNoteOn == 2 ? nullptr : sampleSource.select(instrumentIndex, midiNoteNumber, curvedVelocity, articulationIndex);
            cursor.start(sample, 440.0 * std::exp2((midiNoteNumber - 69.0) / 12.0), getSampleRate());
        }

        void stopNote(float, bool allowTailOff) override
        {
            if (allowTailOff) engine.noteOff();
            else { engine.reset(); clearCurrentNote(); }
        }

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
                const auto sample = static_cast<Sample>(result);
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                    output.addSample(channel, startSample + i, sample);
            }
        }
        windlab::WindEngine engine;
        PerformanceSynthesiser& controllerSource;
        const windlab::RecordedWinds& sampleSource;
        windlab::RecordedCursor cursor;
        std::atomic<float>* sourceMode;
        std::atomic<float>* velocityResponse;
        int modeAtNoteOn = 0;
        std::atomic<float>* instrument;
        std::atomic<float>* articulation;
        std::atomic<float>* attack;
        std::atomic<float>* release;
        std::atomic<float>* tone;
        std::atomic<float>* air;
        std::atomic<float>* breathLength;
        std::atomic<float>* vibrato;
        std::atomic<float>* humanize;
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout WindLabAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"instrument", 1}, "Instrument",
        juce::StringArray {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn"}, 0));
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
    return layout;
}

WindLabAudioProcessor::WindLabAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("WindLabState"), createParameterLayout()),
      synthesiser(std::make_unique<PerformanceSynthesiser>(midiLearn))
{
    auto& synth = *static_cast<PerformanceSynthesiser*>(synthesiser.get());
    for (int i = 0; i < 12; ++i)
        synthesiser->addVoice(new WindVoice(parameters, synth, sampleBank));
    synthesiser->addSound(new WindSound());
    synthesiser->setMinimumRenderingSubdivisionSize(1, false);
}

void WindLabAudioProcessor::prepareToPlay(double sampleRate, int)
{
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

    if (phrase.followsHostTempo())
    {
        // Tempo is independent of transport start/stop in v0.8.
        // If a standalone player has no host, the last known (or 110) BPM is used.
        if (auto* host = getPlayHead())
            if (const auto position = host->getPosition())
                if (const auto bpm = position->getBpm())
                    phrase.setHostTempo(*bpm);
    }

    // Capture external MIDI and virtual PC/piano notes BEFORE the internal
    // Phrase Editor injects its channel-16 playback. The recording is strictly
    // MIDI (notes + CC curves), not a bounced WAV file. Recording and playing
    // use separate buffers so REC never destroys an existing phrase.
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    phraseRecorder.beginBlock(buffer.getNumSamples(), getSampleRate(), phrase.effectiveTempo());
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
                cc == midiLearn.binding(windlab::MidiLearnBindings::Expression) ? 11 : cc;
            phraseRecorder.controller(channel, translated, message.getControllerValue(), offset);
        }
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
        });
    synthesiser->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    outputGain.setTargetValue(parameters.getRawParameterValue("gain")->load());
    roomAmount.setTargetValue(parameters.getRawParameterValue("space")->load());
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
        left[i] = eq.process(0, left[i]);
        right[i] = eq.process(1, right[i]);
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
        for (int i = 0; i < windlab::PhraseEngine::steps; ++i)
            phraseXml->setAttribute("s" + juce::String(i),
                juce::String(static_cast<juce::int64>(phrase.packed(i))));
        // Separate versioned node: old projects load their previous sound,
        // while MIDI Learn starts with standard CC1/CC2/CC11 defaults.
        auto* learnXml = xml->createNewChildElement("WindLabMidiLearnV1");
        learnXml->setAttribute("ccMap", static_cast<int>(midiLearn.snapshot()));
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
        if (auto* phraseXml = xml->getChildByName("WindLabPhraseV1"))
        {
            phrase.setTempo(phraseXml->getIntAttribute("tempo", 110));
            phrase.setFollowHostTempo(phraseXml->getBoolAttribute("followHostTempo", false));
            for (int i = 0; i < windlab::PhraseEngine::steps; ++i)
            {
                const auto name = "s" + juce::String(i);
                if (phraseXml->hasAttribute(name))
                    phrase.setPacked(i, static_cast<std::uint64_t>(
                        phraseXml->getStringAttribute(name).getLargeIntValue()));
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
