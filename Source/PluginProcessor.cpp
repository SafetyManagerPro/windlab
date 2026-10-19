#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Dsp/WindEngine.h"
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
        WindVoice(juce::AudioProcessorValueTreeState& params, PerformanceSynthesiser& synth)
            : controllerSource(synth),
              instrument(params.getRawParameterValue("instrument")),
              articulation(params.getRawParameterValue("articulation")),
              attack(params.getRawParameterValue("attack")),
              release(params.getRawParameterValue("release")),
              tone(params.getRawParameterValue("tone")),
              air(params.getRawParameterValue("air")) {}

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
            const auto instrumentIndex = juce::jlimit(0, 5, static_cast<int>(instrument->load()));
            const auto articulationIndex = juce::jlimit(0, 2, static_cast<int>(articulation->load()));
            engine.start(midiNoteNumber, velocity,
                         static_cast<windlab::Instrument>(instrumentIndex),
                         attack->load(), release->load(),
                         static_cast<windlab::Articulation>(articulationIndex));
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
            for (int i = 0; i < numSamples; ++i)
            {
                if (!engine.active()) { clearCurrentNote(); break; }
                const auto sample = static_cast<Sample>(engine.render());
                for (int channel = 0; channel < output.getNumChannels(); ++channel)
                    output.addSample(channel, startSample + i, sample);
            }
        }
        windlab::WindEngine engine;
        PerformanceSynthesiser& controllerSource;
        std::atomic<float>* instrument;
        std::atomic<float>* articulation;
        std::atomic<float>* attack;
        std::atomic<float>* release;
        std::atomic<float>* tone;
        std::atomic<float>* air;
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
    layout.add(std::make_unique<Float>(juce::ParameterID {"gain", 1}, "Output",
        juce::NormalisableRange<float>(0.0f, 1.0f), .75f));
    return layout;
}

WindLabAudioProcessor::WindLabAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("WindLabState"), createParameterLayout()),
      synthesiser(std::make_unique<PerformanceSynthesiser>())
{
    auto& synth = *static_cast<PerformanceSynthesiser*>(synthesiser.get());
    for (int i = 0; i < 12; ++i)
        synthesiser->addVoice(new WindVoice(parameters, synth));
    synthesiser->addSound(new WindSound());
    synthesiser->setMinimumRenderingSubdivisionSize(1, false);
}

void WindLabAudioProcessor::prepareToPlay(double sampleRate, int)
{
    synthesiser->setCurrentPlaybackSampleRate(sampleRate);
    synthesiser->allNotesOff(0, false);
    static_cast<PerformanceSynthesiser*>(synthesiser.get())->resetControllers();
    keyboardState.reset();
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
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    synthesiser->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    outputGain.setTargetValue(parameters.getRawParameterValue("gain")->load());
    roomAmount.setTargetValue(parameters.getRawParameterValue("space")->load());

    // Two subtle, distinct, feedback-limited stereo early reflections; not a convolution reverb.
    // Buffers are preallocated in prepareToPlay and never resized in processBlock.
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float dry = .5f * (left[i] + right[i]);
        const float echoL = earlyReflectionL[reflectionPositionL];
        const float echoR = earlyReflectionR[reflectionPositionR];
        earlyReflectionL[reflectionPositionL] = dry * .45f + echoL * .22f;
        earlyReflectionR[reflectionPositionR] = dry * .42f + echoR * .20f;
        if (++reflectionPositionL >= earlyReflectionL.size()) reflectionPositionL = 0;
        if (++reflectionPositionR >= earlyReflectionR.size()) reflectionPositionR = 0;
        const float wet = roomAmount.getNextValue();
        const float g = outputGain.getNextValue();
        // Prevent over-range peaks when all 12 voices stack, with gentle soft clipping.
        left[i] = std::tanh((left[i] + echoL * wet) * g);
        right[i] = std::tanh((right[i] + echoR * wet) * g);
    }
}

juce::AudioProcessorEditor* WindLabAudioProcessor::createEditor()
{
    return new WindLabAudioProcessorEditor(*this);
}

void WindLabAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        juce::copyXmlToBinary(*xml, destData);
}

void WindLabAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = juce::getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WindLabAudioProcessor();
}
