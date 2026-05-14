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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makePID ("filterOrder"), "Filter Order",
        juce::StringArray { "2", "4", "6", "8", "10", "12", "14", "16" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makePID ("phaseAlign"), "Phase Align", false));

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
        NR gainRange (-96.0f, 24.0f);
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
    perHarmonicFreqSemitones.fill (0.0f);
    perHarmonicDetuneCents.fill (0.0f);
    perHarmonicQMult.fill (1.0f);

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
    p.filterStages         = (int)get ("filterOrder") + 1; // choice index 0–7 → stages 1–8
    p.phaseAlign           = get ("phaseAlign") > 0.5f;    // stub — see DEVNOTES.md
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

    p.masterGainLinear     = 1.0f; // master gain applied post-render in processBlock via smoothedMasterGain
    p.polyphony            = juce::jlimit (1, kMaxVoices, (int)get ("polyphony"));

    p.snapshotA = snapshotA.data();
    p.snapshotB = snapshotB.data();
    p.perHarmonicFreqSemitones = perHarmonicFreqSemitones.data();
    p.perHarmonicDetuneCents   = perHarmonicDetuneCents.data();
    p.perHarmonicQMult         = perHarmonicQMult.data();

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

    smoothedMasterGain.setTargetValue (
        juce::Decibels::decibelsToGain (
            apvts.getRawParameterValue ("masterGain")->load()));

    voiceManager.processBlock (buffer, midiMessages, buildVoiceParameters());

    // Apply smoothed master gain post-render for click-free level changes.
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float g = smoothedMasterGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
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

    auto snapA  = xml->createNewChildElement ("SnapshotA");
    auto snapB  = xml->createNewChildElement ("SnapshotB");
    auto phFreq = xml->createNewChildElement ("PerHarmonicFreq");
    auto phDet  = xml->createNewChildElement ("PerHarmonicDet");
    auto phQ    = xml->createNewChildElement ("PerHarmonicQ");
    for (int k = 0; k < kNumHarmonics; ++k)
    {
        juce::String h = "h" + juce::String (k);
        snapA ->setAttribute (h, (double)snapshotA [(size_t)k]);
        snapB ->setAttribute (h, (double)snapshotB [(size_t)k]);
        phFreq->setAttribute (h, (double)perHarmonicFreqSemitones[(size_t)k]);
        phDet ->setAttribute (h, (double)perHarmonicDetuneCents  [(size_t)k]);
        phQ   ->setAttribute (h, (double)perHarmonicQMult        [(size_t)k]);
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

    if (auto* phFreq = xml->getChildByName ("PerHarmonicFreq"))
        for (int k = 0; k < kNumHarmonics; ++k)
            perHarmonicFreqSemitones[(size_t)k] = (float)phFreq->getDoubleAttribute ("h" + juce::String (k), 0.0);

    if (auto* phDet = xml->getChildByName ("PerHarmonicDet"))
        for (int k = 0; k < kNumHarmonics; ++k)
            perHarmonicDetuneCents[(size_t)k] = (float)phDet->getDoubleAttribute ("h" + juce::String (k), 0.0);

    if (auto* phQ = xml->getChildByName ("PerHarmonicQ"))
        for (int k = 0; k < kNumHarmonics; ++k)
            perHarmonicQMult[(size_t)k] = (float)phQ->getDoubleAttribute ("h" + juce::String (k), 1.0);

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
void ResosynAudioProcessor::analyzeFile (const juce::File& file, SnapshotTarget target)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return;

    const double fileSr  = reader->sampleRate;
    const int    fftOrder = 15;                 // 32768 bins
    const int    fftSize  = 1 << fftOrder;

    // Read up to fftSize samples, starting at 20% of the file to skip attack transient.
    const int fileLen      = (int)reader->lengthInSamples;
    const int startSample  = (int)(fileLen * 0.20);
    const int samplesToRead = std::min (fftSize, fileLen - startSample);
    if (samplesToRead < 512) return;

    juce::AudioBuffer<float> tmp ((int)reader->numChannels, samplesToRead);
    reader->read (&tmp, 0, samplesToRead, startSample, true, true);

    // Mix down to mono
    std::vector<float> mono (samplesToRead, 0.0f);
    for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
        for (int i = 0; i < samplesToRead; ++i)
            mono[(size_t)i] += tmp.getSample (ch, i) / (float)tmp.getNumChannels();

    // Build FFT buffer (size 2*fftSize): real samples + Hann window in first half, zeros in second.
    std::vector<float> fftData (fftSize * 2, 0.0f);
    for (int i = 0; i < samplesToRead; ++i)
    {
        float hann = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                               * (float)i / (float)(samplesToRead - 1)));
        fftData[(size_t)i] = mono[(size_t)i] * hann;
    }

    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    // fftData[0..fftSize/2] now holds magnitudes

    const int halfSize = fftSize / 2;
    const auto mag = [&](int k) -> float {
        return (k >= 0 && k < halfSize) ? fftData[(size_t)k] : 0.0f;
    };

    // HPS (Harmonic Product Spectrum) to find f0 in 80–1500 Hz.
    int minBin = std::max (1, (int)(80.0  * fftSize / fileSr));
    int maxBin =             (int)(1500.0 * fftSize / fileSr);
    maxBin = std::min (maxBin, halfSize / 5 - 1); // HPS needs room for 5x product

    std::vector<float> hps ((size_t)maxBin, 0.0f);
    for (int k = minBin; k < maxBin; ++k)
    {
        hps[(size_t)k] = mag (k);
        for (int d = 2; d <= 5; ++d)
            hps[(size_t)k] *= mag (k * d);
    }

    int f0Bin = minBin;
    for (int k = minBin + 1; k < maxBin; ++k)
        if (hps[(size_t)k] > hps[(size_t)f0Bin]) f0Bin = k;

    // Sub-bin parabolic interpolation for precise f0.
    float f0BinF = (float)f0Bin;
    if (f0Bin > minBin && f0Bin < maxBin - 1)
    {
        float a = hps[(size_t)(f0Bin - 1)];
        float b = hps[(size_t)f0Bin];
        float c = hps[(size_t)(f0Bin + 1)];
        if (2.0f * b > a + c)
            f0BinF += 0.5f * (a - c) / (a - 2.0f * b + c);
    }
    const float f0 = f0BinF * (float)fileSr / (float)fftSize;
    if (f0 < 20.0f) return; // implausible

    // Locate each harmonic: search ±2 semitones around n*f0, parabolic interpolation.
    float harmonicAmps [kNumHarmonics] = {};
    float harmonicFreqs[kNumHarmonics] = {};
    float maxAmp = 0.0f;

    for (int k = 0; k < kNumHarmonics; ++k)
    {
        const float fn = (float)(k + 1) * f0;
        if (fn > fileSr * 0.47f) break;

        const float semRange = std::pow (2.0f, 2.0f / 12.0f);
        int bLow  = std::max (1, (int)(fn / semRange * fftSize / fileSr));
        int bHigh = std::min (halfSize - 2, (int)(fn * semRange * fftSize / fileSr));

        int peakBin = bLow;
        for (int b = bLow + 1; b <= bHigh; ++b)
            if (mag (b) > mag (peakBin)) peakBin = b;

        // Sub-bin parabolic interpolation
        float peakBinF = (float)peakBin;
        {
            float a = mag (peakBin - 1), bv = mag (peakBin), c = mag (peakBin + 1);
            if (2.0f * bv > a + c)
                peakBinF += 0.5f * (a - c) / (a - 2.0f * bv + c);
        }

        harmonicAmps [(size_t)k] = mag (peakBin);
        harmonicFreqs[(size_t)k] = peakBinF * (float)fileSr / (float)fftSize;
        maxAmp = std::max (maxAmp, harmonicAmps[(size_t)k]);
    }

    if (maxAmp <= 0.0f) return;

    for (int k = 0; k < kNumHarmonics; ++k)
    {
        const float fn = (float)(k + 1) * f0;
        float amp = harmonicAmps[(size_t)k] / maxAmp;
        if (target == SnapshotTarget::A    || target == SnapshotTarget::Both) snapshotA[(size_t)k] = amp;
        if (target == SnapshotTarget::B    || target == SnapshotTarget::Both) snapshotB[(size_t)k] = amp;

        if (harmonicFreqs[(size_t)k] > 0.0f && fn > 0.0f)
        {
            const float devSemitones = 12.0f * std::log2 (harmonicFreqs[(size_t)k] / fn);
            const float coarse       = std::round (devSemitones);
            const float fine         = (devSemitones - coarse) * 100.0f;
            perHarmonicFreqSemitones[(size_t)k] = juce::jlimit (-24.0f, 24.0f,  coarse);
            perHarmonicDetuneCents  [(size_t)k] = juce::jlimit (-100.0f, 100.0f, fine);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResosynAudioProcessor();
}
