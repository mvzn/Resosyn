#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "HarmonicEditorContent.h"

class ResosynAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ResosynAudioProcessorEditor (ResosynAudioProcessor&);
    ~ResosynAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updatePhaseAlignLatencyLabel();

private:
    ResosynAudioProcessor& audioProcessor;

    // ── Excitation ───────────────────────────────────────────────────────────
    juce::Label    excitationLabel;
    juce::ComboBox modeCombo, noiseColourCombo, loopTypeCombo;
    juce::Slider   wtPosSlider;
    juce::Label    wtPosLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseColourAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   wtPosAttach;

    // ── Filter ───────────────────────────────────────────────────────────────
    juce::Label        filterLabel;
    juce::ToggleButton phaseAlignButton;
    juce::Label        phaseAlignLatencyLabel;
    juce::ComboBox     filterTypeCombo, filterOrderCombo;
    juce::Slider   filterQSlider, filterDetuneSlider, filterStretchSlider, filterSpreadSlider;
    juce::Slider   peakGainSlider;
    juce::Label    filterQLabel,  filterDetuneLabel,  filterStretchLabel,  filterSpreadLabel;
    juce::Label    peakGainLabel;
    juce::Slider   harmonicCountSlider;
    juce::Label    harmonicCountLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   phaseAlignAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttach, filterOrderAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterQAttach, filterDetuneAttach, filterStretchAttach, filterSpreadAttach,
        peakGainAttach, harmonicCountAttach;

    // ── Envelope ─────────────────────────────────────────────────────────────
    juce::Label  envelopeLabel;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel,  decayLabel,  sustainLabel,  releaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attackAttach, decayAttach, sustainAttach, releaseAttach;

    // ── Harmonic editor popup ────────────────────────────────────────────────
    struct HarmonicDocWindow : juce::DocumentWindow {
        std::function<void()> onClose;
        HarmonicDocWindow (const juce::String& n, juce::Colour bg)
            : DocumentWindow (n, bg, DocumentWindow::closeButton) {}
        void closeButtonPressed() override { if (onClose) onClose(); }
    };

    juce::TextButton harmonicsButton;
    std::unique_ptr<HarmonicDocWindow> harmonicWindow;
    HarmonicEditorContent* harmonicContent = nullptr; // owned by harmonicWindow

    // ── Morph ────────────────────────────────────────────────────────────────
    juce::Label  morphLabel;
    juce::Slider timbreMorphSlider, gainMorphSlider, noteMorphSlider, velMorphSlider;
    juce::Label  timbreMorphLabel,  gainMorphLabel,  noteMorphLabel,  velMorphLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        timbreMorphAttach, gainMorphAttach, noteMorphAttach, velMorphAttach;

    // ── Master ───────────────────────────────────────────────────────────────
    juce::Label  masterLabel;
    juce::Slider masterGainSlider, polyphonySlider;
    juce::Label  masterGainLabel,  polyphonyLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        masterGainAttach, polyphonyAttach;

    // ── Sampler ──────────────────────────────────────────────────────────────
    juce::Label        samplerLabel;
    juce::ToggleButton samplerLoopEnableButton;
    juce::Slider       samplerLoopStartSlider, samplerLoopEndSlider, samplerLoopCrossfadeSlider;
    juce::Label        samplerLoopStartLabel,  samplerLoopEndLabel,  samplerLoopCrossfadeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loopTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        loopStartAttach, loopEndAttach, loopCrossfadeAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResosynAudioProcessorEditor)
};
