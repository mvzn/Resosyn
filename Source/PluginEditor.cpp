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
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (780, 500);

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

    initRotary (masterGainSlider); addAndMakeVisible (masterGainSlider);
    initRotary (polyphonySlider);  addAndMakeVisible (polyphonySlider);
    polyphonySlider.setRange (1, kMaxVoices, 1);

    initKnobLabel (masterGainLabel, "Gain");   addAndMakeVisible (masterGainLabel);
    initKnobLabel (polyphonyLabel,  "Voices"); addAndMakeVisible (polyphonyLabel);

    masterGainAttach = std::make_unique<APVTS::SliderAttachment> (apvts, "masterGain", masterGainSlider);
    polyphonyAttach  = std::make_unique<APVTS::SliderAttachment> (apvts, "polyphony",  polyphonySlider);

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
}

ResosynAudioProcessorEditor::~ResosynAudioProcessorEditor() {}

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
    filterLabel.setBounds     (col1 + 6,        row0 + 4,  W - 12, 18);
    filterTypeCombo.setBounds (col1 + 6,        row0 + 26, 116,    20);
    filterOrderCombo.setBounds(col1 + 6 + 120,  row0 + 26, 120,    20);
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
    placeKnob (masterGainSlider, masterGainLabel, col1 + 10, row1 + 26);
    placeKnob (polyphonySlider,  polyphonyLabel,  col1 + 90, row1 + 26);

    // ── Sampler ───────────────────────────────────────────────────────────────
    samplerLabel.setBounds            (col2 + 6,  row1 + 4,  W - 12, 18);
    samplerLoopEnableButton.setBounds (col2 + 6,  row1 + 26, 70, 22);
    loopTypeCombo.setBounds           (col2 + 84, row1 + 26, W - 90, 22);
    placeKnob (samplerLoopStartSlider,     samplerLoopStartLabel,     col2 + 10,  row1 + 60);
    placeKnob (samplerLoopEndSlider,       samplerLoopEndLabel,       col2 + 90,  row1 + 60);
    placeKnob (samplerLoopCrossfadeSlider, samplerLoopCrossfadeLabel, col2 + 168, row1 + 60);
}
