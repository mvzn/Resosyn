#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <cstring>

// 32-harmonic biquad bandpass bank.
// SoA memory layout required for future AVX SIMD (4 passes of 8-wide).
// Coefficients must be updated every 16–32 samples (subblock cadence).
class FilterBank
{
public:
    static constexpr int kSize = 32;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        flushState();
        std::memset (&b0, 0, sizeof (b0));
        std::memset (&b2, 0, sizeof (b2));
        std::memset (&a1, 0, sizeof (a1));
        std::memset (&a2, 0, sizeof (a2));
    }

    // Called at subblock cadence (~every 32 samples), not per-sample.
    // freqOffsets in semitones, detunes in cents, qMults per-harmonic Q multipliers.
    // filterType: 0=Bandpass, 1=Peak EQ. peakGainDB used only when filterType==1.
    void updateCoefficients (float fundamental,
                             float stretch,
                             float overallQ,
                             const float* freqOffsetsSemitones,
                             const float* detunesCents,
                             const float* qMults,
                             int   filterType  = 0,
                             float peakGainDB  = 0.0f) noexcept
    {
        const float twoPiOverSr = 2.0f * juce::MathConstants<float>::pi / (float)sr;
        const float nyquist     = (float)(sr * 0.49);
        const float A           = std::pow (10.0f, peakGainDB / 40.0f); // peak EQ amplitude

        for (int k = 0; k < kSize; ++k)
        {
            int n = k + 1;
            // Inharmonicity: f_n = n * f0 * (1 + stretch * n^2)
            float fn = (float)n * fundamental * (1.0f + stretch * (float)(n * n));

            // Per-harmonic tuning
            float totalCents = freqOffsetsSemitones[k] * 100.0f + detunesCents[k];
            if (totalCents != 0.0f)
                fn *= std::pow (2.0f, totalCents / 1200.0f);

            fn = juce::jlimit (20.0f, nyquist, fn);

            float w0    = fn * twoPiOverSr;
            float sinW0 = std::sin (w0);
            float cosW0 = std::cos (w0);

            float Q     = juce::jlimit (0.1f, 200.0f, overallQ * qMults[k]);
            float alpha = sinW0 / (2.0f * Q);

            if (filterType == 1)
            {
                // Peaking EQ (Audio EQ Cookbook). b1=a1 property holds.
                float a0inv = 1.0f / (1.0f + alpha / A);
                b0[k] = (1.0f + alpha * A) * a0inv;
                b2[k] = (1.0f - alpha * A) * a0inv;
                a1[k] = -2.0f * cosW0 * a0inv;
                a2[k] = (1.0f - alpha / A) * a0inv;
            }
            else
            {
                // Bandpass (b1=0 property holds)
                float a0inv = 1.0f / (1.0f + alpha);
                b0[k] =  alpha * a0inv;
                b2[k] = -alpha * a0inv;
                a1[k] = -2.0f * cosW0 * a0inv;
                a2[k] = (1.0f - alpha) * a0inv;
            }
        }
    }

    // input[numSamples] → outL/outR accumulated (does not clear outputs first).
    // gains: linear gain per harmonic [kSize].
    // spread: 0=centre, 1=full alternate L/R pan.
    void process (const float* input,
                  float*       outL,
                  float*       outR,
                  int          numSamples,
                  float        spread,
                  const float* gains) noexcept
    {
        for (int k = 0; k < kSize; ++k)
        {
            const float b0k = b0[k], b2k = b2[k], a1k = a1[k], a2k = a2[k];
            float s1k = s1[k], s2k = s2[k];
            const float gainK = gains[k];

            // Alternating L/R panning by harmonic index scaled by spread
            float pan   = spread * ((k & 1) ? 0.5f : -0.5f); // –0.5 to +0.5
            float panL  = juce::jlimit (0.0f, 1.0f, 0.5f - pan);
            float panR  = juce::jlimit (0.0f, 1.0f, 0.5f + pan);
            float gainL = gainK * std::sqrt (panL);
            float gainR = gainK * std::sqrt (panR);

            for (int i = 0; i < numSamples; ++i)
            {
                float x = input[i];
                // Direct Form II Transposed, b1=0 for bandpass
                float y = b0k * x + s1k;
                s1k = s2k - a1k * y;
                s2k = b2k * x - a2k * y;
                outL[i] += y * gainL;
                outR[i] += y * gainR;
            }

            s1[k] = s1k;
            s2[k] = s2k;
        }
    }

    // Peak EQ process path. Uses b1=a1 property: s1 update differs from bandpass.
    // Does not clear outputs first; call instead of process() when filterType==1.
    void processPeak (const float* input,
                      float*       outL,
                      float*       outR,
                      int          numSamples,
                      float        spread,
                      const float* gains) noexcept
    {
        for (int k = 0; k < kSize; ++k)
        {
            const float b0k = b0[k], b2k = b2[k], a1k = a1[k], a2k = a2[k];
            float s1k = s1[k], s2k = s2[k];
            const float gainK = gains[k];

            float pan   = spread * ((k & 1) ? 0.5f : -0.5f);
            float panL  = juce::jlimit (0.0f, 1.0f, 0.5f - pan);
            float panR  = juce::jlimit (0.0f, 1.0f, 0.5f + pan);
            float gainL = gainK * std::sqrt (panL);
            float gainR = gainK * std::sqrt (panR);

            for (int i = 0; i < numSamples; ++i)
            {
                float x = input[i];
                float y = b0k * x + s1k;
                s1k = a1k * (x - y) + s2k;  // b1=a1 for peak EQ
                s2k = b2k * x - a2k * y;
                outL[i] += y * gainL;
                outR[i] += y * gainR;
            }

            s1[k] = s1k;
            s2[k] = s2k;
        }
    }

    // Zero filter state; call when voice ends to prevent denormal CPU spikes.
    void flushState() noexcept
    {
        std::memset (s1, 0, sizeof (s1));
        std::memset (s2, 0, sizeof (s2));
    }

private:
    // SoA: group coefficients by field, not by filter (enables AVX)
    float b0[kSize], b2[kSize]; // b1 = 0 for bandpass
    float a1[kSize], a2[kSize];
    float s1[kSize], s2[kSize]; // per-harmonic biquad state

    double sr = 44100.0;
};
