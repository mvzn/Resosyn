#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Draggable bar chart for editing one snapshot's per-harmonic gains.
// ─────────────────────────────────────────────────────────────────────────────
class GainBarEditor : public juce::Component
{
public:
    std::function<void()> onChange;

    void setData (float* d, int count) { data = d; numBars = count; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.fillAll (juce::Colour (0xff141420));
        if (data == nullptr) return;

        const float barW = b.getWidth() / (float)numBars;
        for (int k = 0; k < numBars; ++k)
        {
            float gain = juce::jlimit (0.0f, 1.0f, data[k]);
            float barH = gain * (b.getHeight() - 14.0f);
            float x    = k * barW;

            juce::Colour fill = (k % 2 == 0) ? juce::Colour (0xff5566dd)
                                              : juce::Colour (0xff3344bb);
            g.setColour (fill);
            g.fillRect (x + 1.0f, b.getHeight() - 14.0f - barH, barW - 2.0f, barH);

            if (k % 4 == 0 || k == numBars - 1)
            {
                g.setColour (juce::Colours::grey);
                g.setFont (juce::FontOptions (9.0f));
                g.drawText (juce::String (k + 1),
                            (int)x, (int)(b.getHeight() - 13), (int)barW, 12,
                            juce::Justification::centred);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { applyMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { applyMouse (e); }

private:
    float* data    = nullptr;
    int    numBars = 0;

    void applyMouse (const juce::MouseEvent& e)
    {
        if (data == nullptr || numBars == 0) return;
        float barW = (float)getWidth() / (float)numBars;
        int   k    = juce::jlimit (0, numBars - 1, (int)((float)e.x / barW));
        float gain = juce::jlimit (0.0f, 1.0f,
                                   1.0f - (float)e.y / (float)(getHeight() - 14));
        data[k] = gain;
        if (onChange) onChange();
        repaint();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Content component shown inside the DocumentWindow popup.
// Window should be sized 760 × 540.
// ─────────────────────────────────────────────────────────────────────────────
class HarmonicEditorContent : public juce::Component
{
public:
    explicit HarmonicEditorContent (ResosynAudioProcessor& proc)
        : processor (proc)
    {
        snapshotABtn.setToggleState (true, juce::dontSendNotification);
        snapshotABtn.setRadioGroupId (1);
        snapshotABtn.setClickingTogglesState (true);
        snapshotABtn.onClick = [this] { editingA = true;  refreshGainBars(); };

        snapshotBBtn.setRadioGroupId (1);
        snapshotBBtn.setClickingTogglesState (true);
        snapshotBBtn.onClick = [this] { editingA = false; refreshGainBars(); };

        analyzeBtn.onClick = [this] { openAnalysisChooser(); };

        for (auto* b : { &snapshotABtn, &snapshotBBtn, &analyzeBtn })
            addAndMakeVisible (b);

        gainBars.onChange = [this] {};
        addAndMakeVisible (gainBars);
        refreshGainBars();

        juce::NormalisableRange<double> qRange (0.1, 50.0);
        qRange.setSkewForCentre (1.0);
        const juce::NormalisableRange<double> detRange  (-100.0, 100.0);
        const juce::NormalisableRange<double> freqRange (-24.0,   24.0);

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            auto setupSlider = [&](juce::Slider& s, const char* tip) {
                s.setSliderStyle (juce::Slider::LinearVertical);
                s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s.setTooltip (tip + juce::String (" H") + juce::String (k + 1));
                addAndMakeVisible (s);
            };

            auto& qs = qSliders[(size_t)k];
            setupSlider (qs, "Q×");
            qs.setNormalisableRange (qRange);
            qs.setValue (processor.perHarmonicQMult[(size_t)k], juce::dontSendNotification);
            qs.onValueChange = [this, k] {
                processor.perHarmonicQMult[(size_t)k] = (float)qSliders[(size_t)k].getValue();
            };

            auto& ds = detSliders[(size_t)k];
            setupSlider (ds, "Det");
            ds.setNormalisableRange (detRange);
            ds.setValue (processor.perHarmonicDetuneCents[(size_t)k], juce::dontSendNotification);
            ds.onValueChange = [this, k] {
                processor.perHarmonicDetuneCents[(size_t)k] = (float)detSliders[(size_t)k].getValue();
            };

            auto& fs = freqSliders[(size_t)k];
            setupSlider (fs, "Freq");
            fs.setNormalisableRange (freqRange);
            fs.setNumDecimalPlacesToDisplay (0);
            fs.setValue (processor.perHarmonicFreqSemitones[(size_t)k], juce::dontSendNotification);
            fs.onValueChange = [this, k] {
                processor.perHarmonicFreqSemitones[(size_t)k] = (float)freqSliders[(size_t)k].getValue();
            };
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);

        auto header = b.removeFromTop (28);
        snapshotABtn.setBounds (header.removeFromLeft (36));
        header.removeFromLeft (4);
        snapshotBBtn.setBounds (header.removeFromLeft (36));
        header.removeFromLeft (8);
        analyzeBtn.setBounds (header.removeFromRight (130));

        b.removeFromTop (6);
        gainBars.setBounds (b.removeFromTop (190));
        b.removeFromTop (8);

        const int colW = (b.getWidth() - 48) / kNumHarmonics;

        auto placeRow = [&](std::array<juce::Slider, kNumHarmonics>& sliders, int rowH) {
            auto row = b.removeFromTop (rowH);
            row.removeFromLeft (48);
            for (int k = 0; k < kNumHarmonics; ++k)
                sliders[(size_t)k].setBounds (row.removeFromLeft (colW));
            b.removeFromTop (4);
        };

        placeRow (qSliders,   84);
        placeRow (detSliders, 84);
        placeRow (freqSliders, 84);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e1e2e));

        // Row labels aligned to the left margin of each slider row
        auto b = getLocalBounds().reduced (8);
        b.removeFromTop (28 + 6 + 190 + 8);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (juce::Colours::silver);

        for (const char* lbl : { "Q×", "Det", "Freq" })
        {
            auto row = b.removeFromTop (84);
            g.drawText (lbl, row.removeFromLeft (46), juce::Justification::centredRight);
            b.removeFromTop (4);
        }
    }

    // Refresh all controls from processor arrays (call after analysis or external edits).
    void refreshFromProcessor()
    {
        refreshGainBars();
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            qSliders  [(size_t)k].setValue (processor.perHarmonicQMult          [(size_t)k], juce::dontSendNotification);
            detSliders[(size_t)k].setValue (processor.perHarmonicDetuneCents    [(size_t)k], juce::dontSendNotification);
            freqSliders[(size_t)k].setValue(processor.perHarmonicFreqSemitones  [(size_t)k], juce::dontSendNotification);
        }
    }

private:
    ResosynAudioProcessor& processor;
    bool                   editingA = true;

    juce::TextButton snapshotABtn { "A" };
    juce::TextButton snapshotBBtn { "B" };
    juce::TextButton analyzeBtn   { "Analyze File → Snapshot A" };

    GainBarEditor gainBars;
    std::array<juce::Slider, kNumHarmonics> qSliders;
    std::array<juce::Slider, kNumHarmonics> detSliders;
    std::array<juce::Slider, kNumHarmonics> freqSliders;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void refreshGainBars()
    {
        float* gains = editingA ? processor.snapshotA.data()
                                : processor.snapshotB.data();
        gainBars.setData (gains, kNumHarmonics);
        gainBars.repaint();
    }

    void openAnalysisChooser()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Open audio file for analysis",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            processor.audioFormatWildcard());

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc) {
                if (!fc.getResult().existsAsFile()) return;
                processor.analyzeFile (fc.getResult());
                refreshFromProcessor();
            });
    }
};
