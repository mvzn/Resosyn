#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Draws the summed magnitude response of the filterbank at A4 (440 Hz) reference pitch.
// Repaint this from the editor's timer callback.
class FilterResponseDisplay : public juce::Component
{
public:
    explicit FilterResponseDisplay (ResosynAudioProcessor& proc) : processor (proc) {}

    void paint (juce::Graphics& g) override
    {
        auto& apvts = processor.apvts;
        auto get    = [&](const char* id) { return apvts.getRawParameterValue (id)->load(); };

        const int   filterType   = (int)get ("filterType");
        const int   filterStages = (int)get ("filterOrder") + 1;
        const float overallQ     = get ("filterQ");
        const float stretch      = get ("filterStretch");
        const float peakGainDB   = -96.0f + 142.0f * (get ("peakGainMaster") / 100.0f);
        const float perStageDB   = peakGainDB / (float)filterStages;
        const float detuneCents  = get ("filterDetune");
        const int   harmCount    = juce::jlimit (1, kNumHarmonics, (int)get ("harmonicCount"));
        const int   harmStart    = juce::jlimit (1, kNumHarmonics, (int)get ("harmonicStart"));
        const float timbreMorph  = get ("timbreMorph");
        const float gainMorph    = get ("gainMorph");
        const float compensation = get ("compensation");
        const bool  isPeak       = (filterType == 1);
        const bool  bpComp       = !isPeak && get ("bandpassComp") > 0.5f && compensation > 0.0f;
        const float peakComp     = isPeak ? compensation : 0.0f;

        // Mirrors Voice::computeBlendedGains
        const float* snapA = processor.snapshotA.data();
        const float* snapB = processor.snapshotB.data();
        float aSum = 0.0f, bSum = 0.0f;
        for (int k = 0; k < kNumHarmonics; ++k) { aSum += snapA[k]; bSum += snapB[k]; }
        float aNorm = (aSum > 0.0f) ? 1.0f / aSum : 0.0f;
        float bNorm = (bSum > 0.0f) ? 1.0f / bSum : 0.0f;
        float levelA = aSum * (float)kNumHarmonics;
        float levelB = bSum * (float)kNumHarmonics;
        float overallLevel = (levelA + gainMorph * (levelB - levelA)) / (float)kNumHarmonics;

        float gains[kNumHarmonics];
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            float shapeA = snapA[k] * aNorm;
            float shapeB = snapB[k] * bNorm;
            gains[k] = (shapeA + timbreMorph * (shapeB - shapeA)) * overallLevel;
        }
        for (int k = 0; k < harmStart - 1; ++k)  gains[k] = 0.0f;
        for (int k = harmCount; k < kNumHarmonics; ++k) gains[k] = 0.0f;

        // Bandpass: total-gain normalization morphs gains toward 1/Σgain.
        if (bpComp)
        {
            float sumGain = 0.0f;
            for (int k = 0; k < kNumHarmonics; ++k) sumGain += gains[k];
            if (sumGain > 1e-6f)
            {
                const float scale = (1.0f - compensation) + compensation / sumGain;
                for (int k = 0; k < kNumHarmonics; ++k) gains[k] *= scale;
            }
        }

        // Compute per-harmonic biquad coefficients at A4 (440 Hz), sr=44100 reference
        constexpr float kRefSr   = 44100.0f;
        constexpr float kRefFund = 440.0f;
        const float twoPiOverSr  = 2.0f * juce::MathConstants<float>::pi / kRefSr;
        const float nyquist      = kRefSr * 0.49f;
        const float A            = std::pow (10.0f, perStageDB / 40.0f);

        float refFund = kRefFund;
        if (detuneCents != 0.0f)
            refFund *= std::pow (2.0f, detuneCents / 1200.0f);

        const float* freqSem  = processor.perHarmonicFreqSemitones.data();
        const float* detC     = processor.perHarmonicDetuneCents.data();
        const float* qMults   = processor.perHarmonicQMult.data();

        struct Coeffs { float b0, b2, a1, a2; };
        Coeffs coeffs[kNumHarmonics];

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            int   n     = k + 1;
            float fn    = (float)n * refFund * (1.0f + stretch * (float)(n * n));
            float cents = freqSem[k] * 100.0f + detC[k];
            if (cents != 0.0f) fn *= std::pow (2.0f, cents / 1200.0f);
            fn = juce::jlimit (20.0f, nyquist, fn);

            float w0    = fn * twoPiOverSr;
            float sinW0 = std::sin (w0), cosW0 = std::cos (w0);
            float Q     = juce::jlimit (0.1f, 200.0f, overallQ * qMults[k]);
            float alpha = sinW0 / (2.0f * Q);

            if (filterType == 1)
            {
                float a0inv = 1.0f / (1.0f + alpha / A);
                coeffs[k]   = { (1.0f + alpha * A) * a0inv, (1.0f - alpha * A) * a0inv,
                                -2.0f * cosW0 * a0inv,       (1.0f - alpha / A) * a0inv };
            }
            else
            {
                float a0inv = 1.0f / (1.0f + alpha);
                coeffs[k]   = { alpha * a0inv, -alpha * a0inv,
                                -2.0f * cosW0 * a0inv, (1.0f - alpha) * a0inv };
            }
        }

        // Evaluate summed magnitude response at 256 log-spaced points (20 Hz–20 kHz)
        static constexpr int   kNFreq   = 256;
        static constexpr float kFMin    = 20.0f, kFMax = 20000.0f;
        const float logFMin = std::log (kFMin), logFMax = std::log (kFMax);

        float evalCosW[kNFreq], evalCos2W[kNFreq];
        for (int i = 0; i < kNFreq; ++i)
        {
            float t   = (float)i / (float)(kNFreq - 1);
            float f   = std::exp (logFMin + t * (logFMax - logFMin));
            float w   = f * twoPiOverSr;
            float cw  = std::cos (w);
            evalCosW[i]  = cw;
            evalCos2W[i] = 2.0f * cw * cw - 1.0f; // cos(2ω) = 2cos²(ω)-1
        }

        float resp[kNFreq] = {};
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            if (gains[k] <= 0.0f) continue;
            const auto& c     = coeffs[k];
            float b0sq        = c.b0 * c.b0;
            float b2sq        = c.b2 * c.b2;
            float b0b2        = c.b0 * c.b2;
            float a1sq        = c.a1 * c.a1;
            float a2sq        = c.a2 * c.a2;
            float a1_1pa2     = c.a1 * (1.0f + c.a2);
            float a1_b0pb2    = c.a1 * (c.b0 + c.b2); // peak EQ numerator cross term

            for (int i = 0; i < kNFreq; ++i)
            {
                float cosW  = evalCosW[i];
                float cos2W = evalCos2W[i];

                float num   = (filterType == 1)
                    ? b0sq + a1sq + b2sq + 2.0f * a1_b0pb2 * cosW + 2.0f * b0b2 * cos2W
                    : b0sq + b2sq                                  + 2.0f * b0b2 * cos2W;

                float den   = 1.0f + a1sq + a2sq + 2.0f * a1_1pa2 * cosW + 2.0f * c.a2 * cos2W;
                float h2    = (den > 1e-10f) ? num / den : 0.0f;
                float h     = std::sqrt (std::max (0.0f, h2));

                float hN = h;
                for (int st = 1; st < filterStages; ++st) hN *= h;

                resp[i] += gains[k] * hN;
            }
        }

        // Peak EQ: dry-subtraction adds a constant magnitude offset of comp*(1 - Σgain).
        // (Approximation using summed magnitudes rather than complex sum.)
        if (peakComp > 0.0f)
        {
            float sumGain = 0.0f;
            for (int k = 0; k < kNumHarmonics; ++k) sumGain += gains[k];
            const float dryOffset = peakComp * (1.0f - sumGain);
            for (int i = 0; i < kNFreq; ++i)
                resp[i] = std::max (0.0f, resp[i] + dryOffset);
        }

        // ── Draw ─────────────────────────────────────────────────────────────────
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.fillAll (juce::Colour (0xff141420));

        const float dbMin = -60.0f, dbMax = 24.0f, dbRange = dbMax - dbMin;

        auto dbToY = [&](float db) noexcept {
            return b.getY() + (1.0f - (juce::jlimit (dbMin, dbMax, db) - dbMin) / dbRange) * b.getHeight();
        };
        auto freqToX = [&](float f) noexcept {
            return b.getX() + ((std::log (f) - logFMin) / (logFMax - logFMin)) * b.getWidth();
        };

        // Grid — frequency verticals
        g.setColour (juce::Colour (0xff252540));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            g.drawVerticalLine ((int)freqToX (f), b.getY(), b.getBottom());

        // Grid — dB horizontals (0 dB brighter)
        for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f, 12.0f })
        {
            g.setColour (db == 0.0f ? juce::Colour (0xff404060) : juce::Colour (0xff252540));
            g.drawHorizontalLine ((int)dbToY (db), b.getX(), b.getRight());
        }

        // Response fill + stroke
        juce::Path fill, stroke;
        for (int i = 0; i < kNFreq; ++i)
        {
            float db = 20.0f * std::log10 (std::max (resp[i], 1e-7f));
            float t  = (float)i / (float)(kNFreq - 1);
            float x  = b.getX() + t * b.getWidth();
            float y  = dbToY (db);

            if (i == 0) { fill.startNewSubPath (x, b.getBottom()); fill.lineTo (x, y); stroke.startNewSubPath (x, y); }
            else         { fill.lineTo (x, y); stroke.lineTo (x, y); }
        }
        fill.lineTo (b.getRight(), b.getBottom());
        fill.closeSubPath();

        g.setColour (juce::Colour (0x226688ff));
        g.fillPath (fill);
        g.setColour (juce::Colour (0xff6688ff));
        g.strokePath (stroke, juce::PathStrokeType (1.5f));

        // Labels
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (juce::Colours::grey.withAlpha (0.7f));
        const float fLabels[]     = { 100.0f, 1000.0f, 5000.0f, 10000.0f };
        const char* fLabelStrs[]  = { "100",  "1k",    "5k",    "10k"    };
        for (int i = 0; i < 4; ++i)
            g.drawText (fLabelStrs[i], (int)freqToX (fLabels[i]) + 2, (int)b.getY() + 2, 28, 12,
                        juce::Justification::left);

        g.drawText ("A4 ref", (int)b.getRight() - 38, (int)b.getY() + 2, 36, 12,
                    juce::Justification::right);
    }

private:
    ResosynAudioProcessor& processor;
};
