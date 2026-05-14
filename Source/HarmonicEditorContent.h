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
            float gain  = juce::jlimit (0.0f, 1.0f, data[k]);
            float barH  = gain * (b.getHeight() - 14.0f); // leave 14px for labels
            float x     = k * barW;
            float top   = b.getHeight() - 14.0f - barH;

            juce::Colour fill = (k % 2 == 0) ? juce::Colour (0xff5566dd)
                                              : juce::Colour (0xff3344bb);
            g.setColour (fill);
            g.fillRect (x + 1.0f, top, barW - 2.0f, barH);

            // Label every 4th harmonic plus the last
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
// ─────────────────────────────────────────────────────────────────────────────
class HarmonicEditorContent : public juce::Component
{
public:
    explicit HarmonicEditorContent (ResosynAudioProcessor& proc)
        : processor (proc)
    {
        // Header buttons
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

        // Gain bars
        gainBars.onChange = [this] { /* data already written; audio thread will pick it up */ };
        addAndMakeVisible (gainBars);
        refreshGainBars();

        // Per-harmonic Q-mult sliders
        juce::NormalisableRange<double> qRange (0.1, 50.0);
        qRange.setSkewForCentre (1.0);

        // Per-harmonic detune sliders
        juce::NormalisableRange<double> detRange (-100.0, 100.0);

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            auto& qs = qSliders[(size_t)k];
            qs.setSliderStyle (juce::Slider::LinearVertical);
            qs.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            qs.setNormalisableRange (qRange);
            qs.setValue (processor.perHarmonicQMult[(size_t)k], juce::dontSendNotification);
            qs.setTooltip ("Harmonic " + juce::String (k + 1) + " Q×");
            qs.onValueChange = [this, k] {
                processor.perHarmonicQMult[(size_t)k] = (float)qSliders[(size_t)k].getValue();
            };
            addAndMakeVisible (qs);

            auto& ds = detSliders[(size_t)k];
            ds.setSliderStyle (juce::Slider::LinearVertical);
            ds.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            ds.setNormalisableRange (detRange);
            ds.setValue (processor.perHarmonicDetuneCents[(size_t)k], juce::dontSendNotification);
            ds.setTooltip ("Harmonic " + juce::String (k + 1) + " detune (cents)");
            ds.onValueChange = [this, k] {
                processor.perHarmonicDetuneCents[(size_t)k] = (float)detSliders[(size_t)k].getValue();
            };
            addAndMakeVisible (ds);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);

        // Header row
        auto header = b.removeFromTop (28);
        snapshotABtn.setBounds (header.removeFromLeft (36));
        header.removeFromLeft (4);
        snapshotBBtn.setBounds (header.removeFromLeft (36));
        header.removeFromLeft (8);
        analyzeBtn.setBounds (header.removeFromRight (110));

        b.removeFromTop (6);

        // Gain bars
        gainBars.setBounds (b.removeFromTop (190));
        b.removeFromTop (8);

        // Row layout: left label column (48px) + 32 slider columns
        const int colW = (b.getWidth() - 48) / kNumHarmonics;

        auto placeRow = [&](std::array<juce::Slider, kNumHarmonics>& sliders, int rowH) {
            auto row = b.removeFromTop (rowH);
            row.removeFromLeft (48); // label space
            for (int k = 0; k < kNumHarmonics; ++k)
                sliders[(size_t)k].setBounds (row.removeFromLeft (colW));
            b.removeFromTop (4);
        };

        placeRow (qSliders,  90);
        placeRow (detSliders, 90);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e1e2e));

        auto b = getLocalBounds().reduced (8);
        b.removeFromTop (34 + 6 + 190 + 8); // skip header + bars

        // Row labels
        auto labelFont = juce::Font (juce::FontOptions (11.0f));
        g.setFont (labelFont);
        g.setColour (juce::Colours::silver);

        auto labelBounds = b.removeFromTop (90);
        g.drawText ("Q×", labelBounds.removeFromLeft (48).withTrimmedBottom (20),
                    juce::Justification::centredRight);

        b.removeFromTop (4);
        auto detBounds = b.removeFromTop (90);
        g.drawText ("Det", detBounds.removeFromLeft (48).withTrimmedBottom (20),
                    juce::Justification::centredRight);
    }

    // Called by the editor after analysis completes so sliders reflect new data.
    void refreshFromProcessor()
    {
        refreshGainBars();
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            qSliders  [(size_t)k].setValue (processor.perHarmonicQMult        [(size_t)k],
                                            juce::dontSendNotification);
            detSliders[(size_t)k].setValue (processor.perHarmonicDetuneCents  [(size_t)k],
                                            juce::dontSendNotification);
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
                auto chosen = fc.getResult();
                if (!chosen.existsAsFile()) return;
                processor.analyzeFile (chosen);
                refreshFromProcessor();
            });
    }
};
