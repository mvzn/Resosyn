#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ResosynAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    auto makePID = [](const char* id) { return juce::ParameterID (id, 1); };

    using NR = juce::NormalisableRange<float>;

    // Excitation
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makePID ("excitationMode"), "Mode",
        juce::StringArray { "Noise", "Wavetable", "Sampler" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makePID ("noiseColour"), "Noise Colour",
        juce::StringArray { "White", "Pink", "Brown" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("wavetablePosition"), "WT Position",
        NR (0.0f, 1.0f), 0.0f));

    // Sampler loop
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makePID ("samplerLoopEnable"), "Loop Enable", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("samplerLoopStart"), "Loop Start", NR (0.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("samplerLoopEnd"), "Loop End", NR (0.0f, 1.0f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makePID ("samplerLoopType"), "Loop Type",
        juce::StringArray { "Forward", "Ping-Pong" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("samplerLoopCrossfade"), "Loop XFade",
        NR (0.0f, 500.0f), 0.0f));

    // Filterbank
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makePID ("filterType"), "Filter Type",
        juce::StringArray { "Bandpass", "Peak" }, 0));

    {
        NR qRange (0.1f, 200.0f);
        qRange.setSkewForCentre (15.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makePID ("filterQ"), "Filter Q", qRange, 1.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("filterDetune"), "Global Detune",
        NR (-100.0f, 100.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("filterStretch"), "Stretch", NR (0.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("filterSpread"), "Spread", NR (0.0f, 1.0f), 0.0f));

    // 0=neutral (-96 dB), 100=+46 dB; linear mapping applied in buildVoiceParameters
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("peakGainMaster"), "Peak Gain", NR (0.0f, 100.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        makePID ("harmonicCount"), "Harmonics", 1, kNumHarmonics, kNumHarmonics));

    // Envelope
    {
        NR timeRange (1.0f, 5000.0f);
        timeRange.setSkewForCentre (200.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makePID ("envAttack"), "Attack", timeRange, 10.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makePID ("envDecay"), "Decay", timeRange, 100.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("envSustain"), "Sustain", NR (0.0f, 1.0f), 0.8f));

    {
        NR relRange (1.0f, 10000.0f);
        relRange.setSkewForCentre (500.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makePID ("envRelease"), "Release", relRange, 500.0f));
    }

    // Morph
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("timbreMorph"), "Timbre Morph", NR (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("gainMorph"), "Gain Morph", NR (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("noteMorphAmount"), "Note Morph", NR (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makePID ("velocityMorphAmount"), "Vel Morph", NR (0.0f, 1.0f), 0.0f));

    // Master
    {
        NR gainRange (-96.0f, 6.0f);
        gainRange.setSkewForCentre (-6.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makePID ("masterGain"), "Master Gain", gainRange, 0.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterInt> (
        makePID ("polyphony"), "Polyphony", 1, kMaxVoices, kMaxVoices));

    return layout;
}

//==============================================================================
ResosynAudioProcessor::ResosynAudioProcessor()
    : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    snapshotA.fill (1.0f);
    snapshotB.fill (0.0f);

    formatManager.registerBasicFormats();
}

ResosynAudioProcessor::~ResosynAudioProcessor() {}

//==============================================================================
const juce::String ResosynAudioProcessor::getName() const { return JucePlugin_Name; }
bool ResosynAudioProcessor::acceptsMidi()     const { return true; }
bool ResosynAudioProcessor::producesMidi()    const { return false; }
bool ResosynAudioProcessor::isMidiEffect()    const { return false; }
double ResosynAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int  ResosynAudioProcessor::getNumPrograms()               { return 1; }
int  ResosynAudioProcessor::getCurrentProgram()            { return 0; }
void ResosynAudioProcessor::setCurrentProgram (int)        {}
const juce::String ResosynAudioProcessor::getProgramName (int)  { return {}; }
void ResosynAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void ResosynAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voiceManager.prepare (sampleRate, samplesPerBlock);
    smoothedMasterGain.reset (sampleRate, 0.05); // 50 ms smoothing
    smoothedMasterGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            apvts.getRawParameterValue ("masterGain")->load()));
}

void ResosynAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ResosynAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

//==============================================================================
VoiceParameters ResosynAudioProcessor::buildVoiceParameters() const noexcept
{
    auto get = [this](const char* id) {
        return apvts.getRawParameterValue (id)->load();
    };

    smoothedMasterGain; // ensure reference; actual advance happens below
    float gainDb = get ("masterGain");

    VoiceParameters p;
    p.excitationMode       = (int)get ("excitationMode");
    p.noiseColour          = (int)get ("noiseColour");
    p.wavetablePosition    = get ("wavetablePosition");

    p.filterType           = (int)get ("filterType");
    p.harmonicCount        = juce::jlimit (1, kNumHarmonics, (int)apvts.getRawParameterValue ("harmonicCount")->load());
    p.overallQ             = get ("filterQ");
    p.filterDetuneCents    = get ("filterDetune");
    p.filterStretch        = get ("filterStretch");
    p.filterSpread         = get ("filterSpread");
    // 0→-96 dB (neutral/silent), 100→+46 dB
    p.peakGainMasterDB     = -96.0f + 142.0f * (get ("peakGainMaster") / 100.0f);

    p.attackMs             = get ("envAttack");
    p.decayMs              = get ("envDecay");
    p.sustainLevel         = get ("envSustain");
    p.releaseMs            = get ("envRelease");

    p.timbreMorph          = get ("timbreMorph");
    p.gainMorph            = get ("gainMorph");
    p.noteMorphAmount      = get ("noteMorphAmount");
    p.velocityMorphAmount  = get ("velocityMorphAmount");

    p.masterGainLinear     = juce::Decibels::decibelsToGain (gainDb);
    p.polyphony            = juce::jlimit (1, kMaxVoices, (int)get ("polyphony"));

    p.snapshotA = snapshotA.data();
    p.snapshotB = snapshotB.data();

    p.samplerBuffer        = (samplerBuffer.getNumSamples() > 0) ? &samplerBuffer : nullptr;
    p.samplerLoopEnable    = get ("samplerLoopEnable") > 0.5f;
    p.samplerLoopStart     = get ("samplerLoopStart");
    p.samplerLoopEnd       = get ("samplerLoopEnd");
    p.samplerLoopPingPong  = (int)get ("samplerLoopType") == 1;
    p.samplerLoopCrossfadeMs = get ("samplerLoopCrossfade");

    return p;
}

void ResosynAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    voiceManager.processBlock (buffer, midiMessages, buildVoiceParameters());
}

//==============================================================================
bool ResosynAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ResosynAudioProcessor::createEditor()
{
    return new ResosynAudioProcessorEditor (*this);
}

//==============================================================================
void ResosynAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();

    // Append snapshots and sampler path as XML attributes on the root
    auto snapA = xml->createNewChildElement ("SnapshotA");
    auto snapB = xml->createNewChildElement ("SnapshotB");
    for (int k = 0; k < kNumHarmonics; ++k)
    {
        snapA->setAttribute ("h" + juce::String (k), (double)snapshotA[(size_t)k]);
        snapB->setAttribute ("h" + juce::String (k), (double)snapshotB[(size_t)k]);
    }
    xml->setAttribute ("samplerFilePath", samplerFilePath);

    copyXmlToBinary (*xml, destData);
}

void ResosynAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (auto* snapA = xml->getChildByName ("SnapshotA"))
        for (int k = 0; k < kNumHarmonics; ++k)
            snapshotA[(size_t)k] = (float)snapA->getDoubleAttribute ("h" + juce::String (k), 1.0);

    if (auto* snapB = xml->getChildByName ("SnapshotB"))
        for (int k = 0; k < kNumHarmonics; ++k)
            snapshotB[(size_t)k] = (float)snapB->getDoubleAttribute ("h" + juce::String (k), 0.0);

    samplerFilePath = xml->getStringAttribute ("samplerFilePath");
    if (samplerFilePath.isNotEmpty())
        loadSamplerFile (juce::File (samplerFilePath));
}

void ResosynAudioProcessor::loadSamplerFile (const juce::File& file)
{
    if (!file.existsAsFile()) return;
    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));
    if (reader == nullptr) return;

    samplerBuffer.setSize ((int)reader->numChannels,
                           (int)reader->lengthInSamples);
    reader->read (&samplerBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
    samplerFilePath = file.getFullPathName();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResosynAudioProcessor();
}
