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

        // Reference pitch follows the most-recently-played MIDI note so the curve
        // tracks what you're hearing instead of an arbitrary A4.
        const int   lastNote     = processor.getLastPlayedMidiNote();
        const float refFundNote  = 440.0f * std::pow (2.0f, (float)(lastNote - 69) / 12.0f);

        constexpr float kRefSr   = 44100.0f;
        const float twoPiOverSr  = 2.0f * juce::MathConstants<float>::pi / kRefSr;
        const float nyquist      = kRefSr * 0.49f;
        const float A            = std::pow (10.0f, perStageDB / 40.0f);

        float refFund = refFundNote;
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

        // 1024 log-spaced evaluation points (20 Hz–20 kHz).
        // sin/cos precomputed once; complex H(ω) avoids two trig calls per harmonic per point.
        static constexpr int   kNFreq = 1024;
        static constexpr float kFMin  = 20.0f, kFMax = 20000.0f;
        const float logFMin = std::log (kFMin), logFMax = std::log (kFMax);

        float evalCosW[kNFreq], evalSinW[kNFreq], evalCos2W[kNFreq], evalSin2W[kNFreq];
        for (int i = 0; i < kNFreq; ++i)
        {
            float t   = (float)i / (float)(kNFreq - 1);
            float f   = std::exp (logFMin + t * (logFMax - logFMin));
            float w   = f * twoPiOverSr;
            float cw  = std::cos (w);
            float sw  = std::sin (w);
            evalCosW[i]  = cw;
            evalSinW[i]  = sw;
            evalCos2W[i] = 2.0f * cw * cw - 1.0f;  // cos(2ω) = 2cos²(ω)−1
            evalSin2W[i] = 2.0f * sw * cw;           // sin(2ω) = 2sin(ω)cos(ω)
        }

        // Complex accumulator: sum gain_k * H_k(ω)^stages across harmonics, then take |·|.
        // Constructive/destructive interference between adjacent harmonics is exact here;
        // magnitude-only summation ignores it entirely.
        float resp_re[kNFreq] = {};
        float resp_im[kNFreq] = {};

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            if (gains[k] <= 0.0f) continue;
            const auto& c = coeffs[k];
            const float g = gains[k];

            for (int i = 0; i < kNFreq; ++i)
            {
                const float cw  = evalCosW[i],  sw  = evalSinW[i];
                const float c2w = evalCos2W[i], s2w = evalSin2W[i];

                // Denominator: 1 + a1·z⁻¹ + a2·z⁻²  at z = e^jω
                const float d_re = 1.0f + c.a1 * cw  + c.a2 * c2w;
                const float d_im =       -c.a1 * sw  - c.a2 * s2w;

                // Numerator: bandpass b1=0; peak EQ b1=a1
                float n_re, n_im;
                if (isPeak)
                {
                    n_re = c.b0 + c.a1 * cw  + c.b2 * c2w;
                    n_im =       -c.a1 * sw  - c.b2 * s2w;
                }
                else
                {
                    n_re = c.b0 + c.b2 * c2w;
                    n_im =      - c.b2 * s2w;
                }

                // H = N/D  (complex division)
                const float d2 = d_re * d_re + d_im * d_im;
                float h_re, h_im;
                if (d2 > 1e-20f)
                {
                    const float inv = 1.0f / d2;
                    h_re = (n_re * d_re + n_im * d_im) * inv;
                    h_im = (n_im * d_re - n_re * d_im) * inv;
                }
                else { h_re = h_im = 0.0f; }

                // H^stages  via repeated complex multiplication
                float hr = h_re, hi = h_im;
                for (int st = 1; st < filterStages; ++st)
                {
                    const float r = hr * h_re - hi * h_im;
                    const float m = hr * h_im + hi * h_re;
                    hr = r; hi = m;
                }

                // Peak EQ compensation: effective response = H^stages − compensation·dry.
                // Dry signal is real-valued (constant 1 in freq domain), so subtract from re only.
                hr -= peakComp;

                resp_re[i] += g * hr;
                resp_im[i] += g * hi;
            }
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
            float mag = std::sqrt (resp_re[i] * resp_re[i] + resp_im[i] * resp_im[i]);
            float db  = 20.0f * std::log10 (std::max (mag, 1e-7f));
            float t   = (float)i / (float)(kNFreq - 1);
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

        const auto refName = juce::MidiMessage::getMidiNoteName (lastNote, true, true, 4);
        g.drawText (refName + " ref", (int)b.getRight() - 60, (int)b.getY() + 2, 58, 12,
                    juce::Justification::right);
    }

private:
    ResosynAudioProcessor& processor;
};
