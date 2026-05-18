#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
static void initRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
}

static void initSectionLabel (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
}

static void initKnobLabel (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (11.0f)));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, juce::Colours::silver);
}

//==============================================================================
ResosynAudioProcessorEditor::ResosynAudioProcessorEditor (ResosynAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      filterResponseDisplay (p)
{
    setSize (780, 678);

    auto& apvts = audioProcessor.apvts;

    // ── Excitation ────────────────────────────────────────────────────────────
    initSectionLabel (excitationLabel, "EXCITATION");
    addAndMakeVisible (excitationLabel);

    modeCombo.addItem ("Noise",     1);
    modeCombo.addItem ("Wavetable", 2);
    modeCombo.addItem ("Sampler",   3);
    addAndMakeVisible (modeCombo);
    modeAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "excitationMode", modeCombo);

    noiseColourCombo.addItem ("White", 1);
    noiseColourCombo.addItem ("Pink",  2);
    noiseColourCombo.addItem ("Brown", 3);
    addAndMakeVisible (noiseColourCombo);
    noiseColourAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "noiseColour", noiseColourCombo);

    initRotary (wtPosSlider);
    addAndMakeVisible (wtPosSlider);
    initKnobLabel (wtPosLabel, "WT Pos");
    addAndMakeVisible (wtPosLabel);
    wtPosAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "wavetablePosition", wtPosSlider);

    // ── Filter ────────────────────────────────────────────────────────────────
    initSectionLabel (filterLabel, "FILTER");
    addAndMakeVisible (filterLabel);

    phaseAlignButton.setButtonText ("Phase Align");
    addAndMakeVisible (phaseAlignButton);
    phaseAlignAttach = std::make_unique<APVTS::ButtonAttachment> (apvts, "phaseAlign", phaseAlignButton);

    phaseAlignLatencyLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    phaseAlignLatencyLabel.setJustificationType (juce::Justification::centredRight);
    phaseAlignLatencyLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (phaseAlignLatencyLabel);

    filterTypeCombo.addItem ("Bandpass", 1);
    filterTypeCombo.addItem ("Peak",     2);
    addAndMakeVisible (filterTypeCombo);
    filterTypeAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "filterType", filterTypeCombo);

    for (int ord : { 2, 4, 6, 8, 10, 12, 14, 16 })
        filterOrderCombo.addItem (juce::String (ord), ord / 2); // itemId must be > 0
    addAndMakeVisible (filterOrderCombo);
    filterOrderAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "filterOrder", filterOrderCombo);

    initRotary (filterQSlider);       addAndMakeVisible (filterQSlider);
    initRotary (filterDetuneSlider);  addAndMakeVisible (filterDetuneSlider);
    initRotary (peakGainSlider);      addAndMakeVisible (peakGainSlider);
    initRotary (filterStretchSlider); addAndMakeVisible (filterStretchSlider);
    initRotary (filterSpreadSlider);  addAndMakeVisible (filterSpreadSlider);

    initKnobLabel (filterQLabel,       "Q");        addAndMakeVisible (filterQLabel);
    initKnobLabel (filterDetuneLabel,  "Detune");   addAndMakeVisible (filterDetuneLabel);
    initKnobLabel (peakGainLabel,      "Pk Gain");  addAndMakeVisible (peakGainLabel);
    initKnobLabel (filterStretchLabel, "Stretch");  addAndMakeVisible (filterStretchLabel);
    initKnobLabel (filterSpreadLabel,  "Spread");   addAndMakeVisible (filterSpreadLabel);

    initRotary (harmonicCountSlider); addAndMakeVisible (harmonicCountSlider);
    harmonicCountSlider.setRange (1, kNumHarmonics, 1);
    initKnobLabel (harmonicCountLabel, "Harmonics"); addAndMakeVisible (harmonicCountLabel);

    filterQAttach        = std::make_unique<APVTS::SliderAttachment> (apvts, "filterQ",        filterQSlider);
    filterDetuneAttach   = std::make_unique<APVTS::SliderAttachment> (apvts, "filterDetune",   filterDetuneSlider);
    peakGainAttach       = std::make_unique<APVTS::SliderAttachment> (apvts, "peakGainMaster", peakGainSlider);
    filterStretchAttach  = std::make_unique<APVTS::SliderAttachment> (apvts, "filterStretch",  filterStretchSlider);
    filterSpreadAttach   = std::make_unique<APVTS::SliderAttachment> (apvts, "filterSpread",   filterSpreadSlider);
    harmonicCountAttach  = std::make_unique<APVTS::SliderAttachment> (apvts, "harmonicCount",  harmonicCountSlider);

    harmonicsButton.setButtonText ("Edit Harmonics");
    harmonicsButton.onClick = [this] {
        if (harmonicWindow != nullptr)
        {
            harmonicWindow->toFront (true);
            return;
        }

        auto* content = new HarmonicEditorContent (audioProcessor);
        harmonicContent = content;

        harmonicWindow = std::make_unique<HarmonicDocWindow> (
            "Harmonic Editor", juce::Colour (0xff1e1e2e));
        harmonicWindow->setContentOwned (content, true);
        harmonicWindow->setSize (760, 540);
        harmonicWindow->setResizable (false, false);
        harmonicWindow->setUsingNativeTitleBar (true);
        harmonicWindow->onClose = [this] {
            harmonicWindow.reset();
            harmonicContent = nullptr;
        };
        harmonicWindow->setVisible (true);
    };
    addAndMakeVisible (harmonicsButton);

    // ── Envelope ──────────────────────────────────────────────────────────────
    initSectionLabel (envelopeLabel, "ENVELOPE");
    addAndMakeVisible (envelopeLabel);

    initRotary (attackSlider);  addAndMakeVisible (attackSlider);
    initRotary (decaySlider);   addAndMakeVisible (decaySlider);
    initRotary (sustainSlider); addAndMakeVisible (sustainSlider);
    initRotary (releaseSlider); addAndMakeVisible (releaseSlider);

    initKnobLabel (attackLabel,  "Attack");  addAndMakeVisible (attackLabel);
    initKnobLabel (decayLabel,   "Decay");   addAndMakeVisible (decayLabel);
    initKnobLabel (sustainLabel, "Sustain"); addAndMakeVisible (sustainLabel);
    initKnobLabel (releaseLabel, "Release"); addAndMakeVisible (releaseLabel);

    attackAttach  = std::make_unique<APVTS::SliderAttachment> (apvts, "envAttack",  attackSlider);
    decayAttach   = std::make_unique<APVTS::SliderAttachment> (apvts, "envDecay",   decaySlider);
    sustainAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "envSustain", sustainSlider);
    releaseAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "envRelease", releaseSlider);

    // ── Release fade (post-envelope cooldown — A/B test controls) ────────────
    initSectionLabel (releaseFadeLabel, "RELEASE FADE");
    addAndMakeVisible (releaseFadeLabel);

    releaseFadeModeCombo.addItem ("Off (instant)", 1);
    releaseFadeModeCombo.addItem ("Gain Ramp",     2);
    releaseFadeModeCombo.addItem ("State Decay",   3);
    releaseFadeModeCombo.addItem ("Pole Shrink",   4);
    releaseFadeModeCombo.addItem ("Coef Ramp",     5);
    addAndMakeVisible (releaseFadeModeCombo);
    releaseFadeModeAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "releaseFadeMode", releaseFadeModeCombo);

    releaseFadeWrapButton.setButtonText ("Wrap Gain");
    addAndMakeVisible (releaseFadeWrapButton);
    releaseFadeWrapAttach = std::make_unique<APVTS::ButtonAttachment> (apvts, "releaseFadeWrap", releaseFadeWrapButton);

    releaseFadeMsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    releaseFadeMsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    releaseFadeMsSlider.setTextValueSuffix (" ms");
    addAndMakeVisible (releaseFadeMsSlider);
    initKnobLabel (releaseFadeMsLabel, "Fade");
    releaseFadeMsLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (releaseFadeMsLabel);
    releaseFadeMsAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "releaseFadeMs", releaseFadeMsSlider);

    // ── Morph ─────────────────────────────────────────────────────────────────
    initSectionLabel (morphLabel, "MORPH");
    addAndMakeVisible (morphLabel);

    initRotary (timbreMorphSlider); addAndMakeVisible (timbreMorphSlider);
    initRotary (gainMorphSlider);   addAndMakeVisible (gainMorphSlider);
    initRotary (noteMorphSlider);   addAndMakeVisible (noteMorphSlider);
    initRotary (velMorphSlider);    addAndMakeVisible (velMorphSlider);

    initKnobLabel (timbreMorphLabel, "Timbre"); addAndMakeVisible (timbreMorphLabel);
    initKnobLabel (gainMorphLabel,   "Gain");   addAndMakeVisible (gainMorphLabel);
    initKnobLabel (noteMorphLabel,   "Note");   addAndMakeVisible (noteMorphLabel);
    initKnobLabel (velMorphLabel,    "Vel");    addAndMakeVisible (velMorphLabel);

    timbreMorphAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "timbreMorph",         timbreMorphSlider);
    gainMorphAttach   = std::make_unique<APVTS::SliderAttachment> (apvts, "gainMorph",           gainMorphSlider);
    noteMorphAttach   = std::make_unique<APVTS::SliderAttachment> (apvts, "noteMorphAmount",     noteMorphSlider);
    velMorphAttach    = std::make_unique<APVTS::SliderAttachment> (apvts, "velocityMorphAmount", velMorphSlider);

    // ── Master ────────────────────────────────────────────────────────────────
    initSectionLabel (masterLabel, "MASTER");
    addAndMakeVisible (masterLabel);

    masterGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (masterGainSlider);

    polyphonySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    polyphonySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 20);
    polyphonySlider.setRange (1, kMaxVoices, 1);
    addAndMakeVisible (polyphonySlider);

    auto initRowLabel = [](juce::Label& l, const juce::String& text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (11.0f)));
        l.setJustificationType (juce::Justification::centredRight);
        l.setColour (juce::Label::textColourId, juce::Colours::silver);
    };
    initRowLabel (masterGainLabel, "Gain");   addAndMakeVisible (masterGainLabel);
    initRowLabel (polyphonyLabel,  "Voices"); addAndMakeVisible (polyphonyLabel);

    masterGainAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "masterGain", masterGainSlider);
    polyphonyAttach  = std::make_unique<APVTS::SliderAttachment> (apvts, "polyphony",  polyphonySlider);

    // ── Analyze buttons (main window) ────────────────────────────────────────
    auto makeAnalyzeBtn = [this] (juce::TextButton& btn,
                                  const char* label,
                                  ResosynAudioProcessor::SnapshotTarget target)
    {
        btn.setButtonText (label);
        btn.onClick = [this, target] {
            mainAnalyzeChooser = std::make_unique<juce::FileChooser> (
                "Open audio file for analysis",
                juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                audioProcessor.audioFormatWildcard());

            mainAnalyzeChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, target] (const juce::FileChooser& fc) {
                    if (!fc.getResult().existsAsFile()) return;
                    audioProcessor.analyzeFile (fc.getResult(), target);
                    if (harmonicContent != nullptr)
                        harmonicContent->refreshFromProcessor();
                });
        };
        addAndMakeVisible (btn);
    };

    using ST = ResosynAudioProcessor::SnapshotTarget;
    makeAnalyzeBtn (analyzeBtnA, "Analyze → A", ST::A);
    makeAnalyzeBtn (analyzeBtnB, "Analyze → B", ST::B);

    // ── Sampler ───────────────────────────────────────────────────────────────
    initSectionLabel (samplerLabel, "SAMPLER");
    addAndMakeVisible (samplerLabel);

    samplerLoopEnableButton.setButtonText ("Loop");
    addAndMakeVisible (samplerLoopEnableButton);
    loopEnableAttach = std::make_unique<APVTS::ButtonAttachment> (apvts, "samplerLoopEnable", samplerLoopEnableButton);

    loopTypeCombo.addItem ("Forward",   1);
    loopTypeCombo.addItem ("Ping-Pong", 2);
    addAndMakeVisible (loopTypeCombo);
    loopTypeAttach = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "samplerLoopType", loopTypeCombo);

    initRotary (samplerLoopStartSlider);    addAndMakeVisible (samplerLoopStartSlider);
    initRotary (samplerLoopEndSlider);      addAndMakeVisible (samplerLoopEndSlider);
    initRotary (samplerLoopCrossfadeSlider);addAndMakeVisible (samplerLoopCrossfadeSlider);

    initKnobLabel (samplerLoopStartLabel,    "Start"); addAndMakeVisible (samplerLoopStartLabel);
    initKnobLabel (samplerLoopEndLabel,      "End");   addAndMakeVisible (samplerLoopEndLabel);
    initKnobLabel (samplerLoopCrossfadeLabel,"XFade"); addAndMakeVisible (samplerLoopCrossfadeLabel);

    loopStartAttach     = std::make_unique<APVTS::SliderAttachment> (apvts, "samplerLoopStart",     samplerLoopStartSlider);
    loopEndAttach       = std::make_unique<APVTS::SliderAttachment> (apvts, "samplerLoopEnd",       samplerLoopEndSlider);
    loopCrossfadeAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "samplerLoopCrossfade", samplerLoopCrossfadeSlider);

    // ── Filter response display ────────────────────────────────────────────
    addAndMakeVisible (filterResponseDisplay);

    // ── Harmonic low-cut slider ───────────────────────────────────────────
    harmonicStartSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    harmonicStartSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 22);
    harmonicStartSlider.setRange (1, kNumHarmonics, 1);
    addAndMakeVisible (harmonicStartSlider);
    harmonicStartAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "harmonicStart", harmonicStartSlider);

    initRowLabel (harmonicStartLabel, "Low Cut");
    addAndMakeVisible (harmonicStartLabel);

    // ── Compensation slider + BP toggle ────────────────────────────────────
    compensationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    compensationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    compensationSlider.setRange (0.0, 1.0, 0.0);
    addAndMakeVisible (compensationSlider);
    compensationAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "compensation", compensationSlider);

    initRowLabel (compensationLabel, "Comp");
    addAndMakeVisible (compensationLabel);

    bandpassCompButton.setButtonText ("BP Comp");
    addAndMakeVisible (bandpassCompButton);
    bandpassCompAttach = std::make_unique<APVTS::ButtonAttachment> (apvts, "bandpassComp", bandpassCompButton);

    updatePhaseAlignLatencyLabel();
    startTimerHz (5);
}

ResosynAudioProcessorEditor::~ResosynAudioProcessorEditor() { stopTimer(); }

//==============================================================================
void ResosynAudioProcessorEditor::timerCallback()
{
    updatePhaseAlignLatencyLabel();
    filterResponseDisplay.repaint();
}

void ResosynAudioProcessorEditor::updatePhaseAlignLatencyLabel()
{
    auto& apvts = audioProcessor.apvts;
    bool  on    = apvts.getRawParameterValue ("phaseAlign")->load() > 0.5f;

    if (!on)
    {
        phaseAlignLatencyLabel.setText ("", juce::dontSendNotification);
        return;
    }

    float Q      = apvts.getRawParameterValue ("filterQ")->load();
    int   stages = (int)apvts.getRawParameterValue ("filterOrder")->load() + 1;

    // group delay at ω₀ for a 2nd-order bandpass = Q/(π·f) seconds per stage
    float latMs = (float)stages * Q * 1000.0f / (juce::MathConstants<float>::pi * 440.0f);

    phaseAlignLatencyLabel.setText (juce::String (latMs, 1) + "ms",
                                    juce::dontSendNotification);
}

//==============================================================================
void ResosynAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));

    const int W = 252, H = 220, pad = 8, top = 8;
    auto drawSection = [&] (int col, int row) {
        juce::Rectangle<int> r (col, row, W, H);
        g.setColour (juce::Colour (0xff2a2a3e));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff44446a));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    };

    const int row0 = top, row1 = top + H + pad;
    const int col0 = pad, col1 = pad + W + pad, col2 = pad + 2 * (W + pad);

    drawSection (col0, row0); drawSection (col1, row0); drawSection (col2, row0);
    drawSection (col0, row1); drawSection (col1, row1); drawSection (col2, row1);

    // Bottom filter-response panel (full width)
    const int dispY = row1 + H + pad;
    juce::Rectangle<int> dispPanel (pad, dispY, getWidth() - 2 * pad, 206);
    g.setColour (juce::Colour (0xff2a2a3e));
    g.fillRoundedRectangle (dispPanel.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff44446a));
    g.drawRoundedRectangle (dispPanel.toFloat().reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
void ResosynAudioProcessorEditor::resized()
{
    const int W = 252, H = 220, pad = 8, top = 8;
    const int knobW = 65, knobH = 65, labelH = 14;

    const int row0 = top, row1 = top + H + pad;
    const int col0 = pad, col1 = pad + W + pad, col2 = pad + 2 * (W + pad);

    auto placeKnob = [&] (juce::Slider& s, juce::Label& lbl, int x, int y) {
        lbl.setBounds (x, y, knobW, labelH);
        s.setBounds   (x, y + labelH, knobW, knobH);
    };

    // ── Excitation ────────────────────────────────────────────────────────────
    excitationLabel.setBounds   (col0 + 6,  row0 + 4,  W - 12, 18);
    modeCombo.setBounds         (col0 + 6,  row0 + 26, W - 12, 22);
    noiseColourCombo.setBounds  (col0 + 6,  row0 + 54, W - 12, 22);
    placeKnob (wtPosSlider, wtPosLabel, col0 + 90, row0 + 84);

    // ── Filter ────────────────────────────────────────────────────────────────
    filterLabel.setBounds             (col1 + 6,   row0 + 4, 120, 18);
    phaseAlignButton.setBounds        (col1 + 130, row0 + 4,  72, 18);
    phaseAlignLatencyLabel.setBounds  (col1 + 202, row0 + 4,  42, 18);
    filterTypeCombo.setBounds  (col1 + 6,       row0 + 26, 116, 20);
    filterOrderCombo.setBounds (col1 + 6 + 120, row0 + 26, 120, 20);
    placeKnob (filterQSlider,       filterQLabel,       col1 + 10,  row0 + 52);
    placeKnob (filterDetuneSlider,  filterDetuneLabel,  col1 + 90,  row0 + 52);
    placeKnob (peakGainSlider,      peakGainLabel,      col1 + 170, row0 + 52);
    placeKnob (filterStretchSlider,   filterStretchLabel,   col1 + 10,  row0 + 138);
    placeKnob (filterSpreadSlider,    filterSpreadLabel,    col1 + 90,  row0 + 138);
    placeKnob (harmonicCountSlider,   harmonicCountLabel,   col1 + 170, row0 + 138);

    // ── Envelope ──────────────────────────────────────────────────────────────
    envelopeLabel.setBounds (col2 + 6, row0 + 4, W - 12, 18);
    placeKnob (attackSlider,  attackLabel,  col2 + 10, row0 + 26);
    placeKnob (decaySlider,   decayLabel,   col2 + 90, row0 + 26);
    placeKnob (sustainSlider, sustainLabel, col2 + 10, row0 + 120);
    placeKnob (releaseSlider, releaseLabel, col2 + 90, row0 + 120);

    // ── Morph ─────────────────────────────────────────────────────────────────
    morphLabel.setBounds (col0 + 6, row1 + 4, W - 12, 18);
    placeKnob (timbreMorphSlider, timbreMorphLabel, col0 + 10, row1 + 26);
    placeKnob (gainMorphSlider,   gainMorphLabel,   col0 + 90, row1 + 26);
    placeKnob (noteMorphSlider,   noteMorphLabel,   col0 + 10, row1 + 120);
    placeKnob (velMorphSlider,    velMorphLabel,    col0 + 90, row1 + 120);

    // ── Master ────────────────────────────────────────────────────────────────
    masterLabel.setBounds (col1 + 6, row1 + 4, W - 12, 18);
    {
        const int lblW = 52, slX = col1 + 6 + lblW + 4, slW = W - 12 - lblW - 4;
        masterGainLabel.setBounds (col1 + 6, row1 + 28, lblW, 22);
        masterGainSlider.setBounds (slX, row1 + 28, slW, 22);
        polyphonyLabel.setBounds   (col1 + 6, row1 + 56, lblW, 22);
        polyphonySlider.setBounds  (slX, row1 + 56, slW, 22);
    }
    harmonicsButton.setBounds (col1 + 6,              row1 + 90,  W - 12,      22);
    analyzeBtnA.setBounds     (col1 + 6,              row1 + 118, (W - 16) / 2, 22);
    analyzeBtnB.setBounds     (col1 + 6 + (W - 16) / 2 + 4, row1 + 118, (W - 16) / 2, 22);

    // ── Release fade (sub-section in master column) ────────────────────────
    releaseFadeLabel.setBounds      (col1 + 6,          row1 + 148, W - 12, 16);
    releaseFadeModeCombo.setBounds  (col1 + 6,          row1 + 168, 140,    22);
    releaseFadeWrapButton.setBounds (col1 + 152,        row1 + 168, W - 158, 22);
    releaseFadeMsLabel.setBounds    (col1 + 6,          row1 + 196, 38,     22);
    releaseFadeMsSlider.setBounds   (col1 + 6 + 42,     row1 + 196, W - 12 - 42, 22);

    // ── Filter response display + harmonic low-cut + compensation ───────────
    {
        const int dispY    = row1 + H + pad;     // 464
        const int innerX   = pad + 6;            // 14
        const int innerW   = getWidth() - 2 * pad - 12; // 752
        const int labelW   = 52;
        const int bpBtnW   = 90;

        filterResponseDisplay.setBounds (innerX, dispY + 8, innerW, 130);

        const int row1Y = dispY + 8 + 130 + 8;       // = dispY + 146
        harmonicStartLabel.setBounds  (innerX,             row1Y, labelW, 22);
        harmonicStartSlider.setBounds (innerX + labelW + 4, row1Y, innerW - labelW - 4, 22);

        const int row2Y = row1Y + 28;                // = dispY + 174
        compensationLabel.setBounds   (innerX,                                  row2Y, labelW, 22);
        compensationSlider.setBounds  (innerX + labelW + 4,                     row2Y,
                                       innerW - labelW - 4 - bpBtnW - 6,        22);
        bandpassCompButton.setBounds  (innerX + innerW - bpBtnW,                row2Y, bpBtnW, 22);
    }

    // ── Sampler ───────────────────────────────────────────────────────────────
    samplerLabel.setBounds            (col2 + 6,  row1 + 4,  W - 12, 18);
    samplerLoopEnableButton.setBounds (col2 + 6,  row1 + 26, 70, 22);
    loopTypeCombo.setBounds           (col2 + 84, row1 + 26, W - 90, 22);
    placeKnob (samplerLoopStartSlider,     samplerLoopStartLabel,     col2 + 10,  row1 + 60);
    placeKnob (samplerLoopEndSlider,       samplerLoopEndLabel,       col2 + 90,  row1 + 60);
    placeKnob (samplerLoopCrossfadeSlider, samplerLoopCrossfadeLabel, col2 + 168, row1 + 60);
}
