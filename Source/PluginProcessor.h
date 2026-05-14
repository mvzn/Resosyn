#pragma once
#include <JuceHeader.h>
#include "DSP/VoiceManager.h"
#include "DSP/VoiceParameters.h"

class ResosynAudioProcessor : public juce::AudioProcessor
{
public:
    ResosynAudioProcessor();
    ~ResosynAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool  acceptsMidi()     const override;
    bool  producesMidi()    const override;
    bool  isMidiEffect()    const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Per-harmonic data (not APVTS params; serialised manually in getStateInformation)
    std::array<float, kNumHarmonics> snapshotA;
    std::array<float, kNumHarmonics> snapshotB;
    std::array<float, kNumHarmonics> perHarmonicFreqSemitones; // ±24, default 0
    std::array<float, kNumHarmonics> perHarmonicDetuneCents;   // ±100, default 0
    std::array<float, kNumHarmonics> perHarmonicQMult;         // 0.1–50, default 1.0

    // One-shot analysis: detects f0, per-harmonic gains → snapshotA, inharmonicity → freq/detune arrays.
    void analyzeFile (const juce::File& file);
    juce::String audioFormatWildcard() const { return formatManager.getWildcardForAllFormats(); }

    // Sampler file (path-only storage, V1)
    juce::AudioBuffer<float> samplerBuffer;
    juce::String             samplerFilePath;
    void loadSamplerFile (const juce::File& file);

private:
    VoiceManager voiceManager;
    juce::AudioFormatManager formatManager;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMasterGain;

    VoiceParameters buildVoiceParameters() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResosynAudioProcessor)
};
