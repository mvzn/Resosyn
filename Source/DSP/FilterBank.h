#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <cstring>

// 32-harmonic biquad bandpass bank.
// SoA memory layout required for future AVX SIMD (4 passes of 8-wide).
// Coefficients must be updated every 16–32 samples (subblock cadence).
// Coefficients are linearly interpolated across each subblock to prevent
// zipper noise from parameter changes.
class FilterBank
{
public:
    static constexpr int kSize = 32;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        coeffsInitialized = false;
        flushState();
        std::memset (b0,     0, sizeof (b0));
        std::memset (b2,     0, sizeof (b2));
        std::memset (a1,     0, sizeof (a1));
        std::memset (a2,     0, sizeof (a2));
        std::memset (prevB0, 0, sizeof (prevB0));
        std::memset (prevB2, 0, sizeof (prevB2));
        std::memset (prevA1, 0, sizeof (prevA1));
        std::memset (prevA2, 0, sizeof (prevA2));
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
        // Save current as previous interpolation start point
        std::memcpy (prevB0, b0, sizeof (b0));
        std::memcpy (prevB2, b2, sizeof (b2));
        std::memcpy (prevA1, a1, sizeof (a1));
        std::memcpy (prevA2, a2, sizeof (a2));

        const float twoPiOverSr = 2.0f * juce::MathConstants<float>::pi / (float)sr;
        const float nyquist     = (float)(sr * 0.49);
        const float A           = std::pow (10.0f, peakGainDB / 40.0f);

        for (int k = 0; k < kSize; ++k)
        {
            int n = k + 1;
            float fn = (float)n * fundamental * (1.0f + stretch * (float)(n * n));

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
                float a0inv = 1.0f / (1.0f + alpha / A);
                b0[k] = (1.0f + alpha * A) * a0inv;
                b2[k] = (1.0f - alpha * A) * a0inv;
                a1[k] = -2.0f * cosW0 * a0inv;
                a2[k] = (1.0f - alpha / A) * a0inv;
            }
            else
            {
                float a0inv = 1.0f / (1.0f + alpha);
                b0[k] =  alpha * a0inv;
                b2[k] = -alpha * a0inv;
                a1[k] = -2.0f * cosW0 * a0inv;
                a2[k] = (1.0f - alpha) * a0inv;
            }
        }

        // On first update after flush, snap prev = new so we don't interpolate from zeros.
        if (!coeffsInitialized)
        {
            std::memcpy (prevB0, b0, sizeof (b0));
            std::memcpy (prevB2, b2, sizeof (b2));
            std::memcpy (prevA1, a1, sizeof (a1));
            std::memcpy (prevA2, a2, sizeof (a2));
            coeffsInitialized = true;
        }
    }

    // input[numSamples] → outL/outR accumulated (does not clear outputs first).
    // Coefficients are linearly interpolated from previous to current across the subblock.
    void process (const float* input,
                  float*       outL,
                  float*       outR,
                  int          numSamples,
                  float        spread,
                  const float* gains,
                  int          numStages) noexcept
    {
        const float invN = 1.0f / (float)numSamples;

        for (int k = 0; k < kSize; ++k)
        {
            const float db0 = (b0[k] - prevB0[k]) * invN;
            const float db2 = (b2[k] - prevB2[k]) * invN;
            const float da1 = (a1[k] - prevA1[k]) * invN;
            const float da2 = (a2[k] - prevA2[k]) * invN;

            float b0k = prevB0[k], b2k = prevB2[k], a1k = prevA1[k], a2k = prevA2[k];
            const float gainK = gains[k];

            float pan   = spread * ((k & 1) ? 0.5f : -0.5f);
            float panL  = juce::jlimit (0.0f, 1.0f, 0.5f - pan);
            float panR  = juce::jlimit (0.0f, 1.0f, 0.5f + pan);
            float gainL = gainK * std::sqrt (panL);
            float gainR = gainK * std::sqrt (panR);

            float st1[8], st2[8];
            for (int st = 0; st < numStages; ++st) { st1[st] = s1[st][k]; st2[st] = s2[st][k]; }

            for (int i = 0; i < numSamples; ++i)
            {
                b0k += db0; b2k += db2; a1k += da1; a2k += da2;

                float x = input[i];
                for (int st = 0; st < numStages; ++st)
                {
                    float y = b0k * x + st1[st];
                    st1[st] = st2[st] - a1k * y;
                    st2[st] = b2k * x - a2k * y;
                    x = y;
                }
                outL[i] += x * gainL;
                outR[i] += x * gainR;
            }

            for (int st = 0; st < numStages; ++st) { s1[st][k] = st1[st]; s2[st][k] = st2[st]; }
            // Zero dormant stages so stale state can't inject on order increase.
            for (int st = numStages; st < 8; ++st) { s1[st][k] = s2[st][k] = 0.0f; }
        }
    }

    // Peak EQ process path. Uses b1=a1 property; coefficients interpolated as above.
    void processPeak (const float* input,
                      float*       outL,
                      float*       outR,
                      int          numSamples,
                      float        spread,
                      const float* gains,
                      int          numStages) noexcept
    {
        const float invN = 1.0f / (float)numSamples;

        for (int k = 0; k < kSize; ++k)
        {
            const float db0 = (b0[k] - prevB0[k]) * invN;
            const float db2 = (b2[k] - prevB2[k]) * invN;
            const float da1 = (a1[k] - prevA1[k]) * invN;
            const float da2 = (a2[k] - prevA2[k]) * invN;

            float b0k = prevB0[k], b2k = prevB2[k], a1k = prevA1[k], a2k = prevA2[k];
            const float gainK = gains[k];

            float pan   = spread * ((k & 1) ? 0.5f : -0.5f);
            float panL  = juce::jlimit (0.0f, 1.0f, 0.5f - pan);
            float panR  = juce::jlimit (0.0f, 1.0f, 0.5f + pan);
            float gainL = gainK * std::sqrt (panL);
            float gainR = gainK * std::sqrt (panR);

            float st1[8], st2[8];
            for (int st = 0; st < numStages; ++st) { st1[st] = s1[st][k]; st2[st] = s2[st][k]; }

            for (int i = 0; i < numSamples; ++i)
            {
                b0k += db0; b2k += db2; a1k += da1; a2k += da2;

                float x = input[i];
                for (int st = 0; st < numStages; ++st)
                {
                    float y = b0k * x + st1[st];
                    st1[st] = a1k * (x - y) + st2[st];
                    st2[st] = b2k * x - a2k * y;
                    x = y;
                }
                outL[i] += x * gainL;
                outR[i] += x * gainR;
            }

            for (int st = 0; st < numStages; ++st) { s1[st][k] = st1[st]; s2[st][k] = st2[st]; }
            for (int st = numStages; st < 8; ++st) { s1[st][k] = s2[st][k] = 0.0f; }
        }
    }

    // Zero filter state and reset coefficient interpolation.
    // Call when a voice ends or is stolen to prevent denormal CPU spikes.
    void flushState() noexcept
    {
        std::memset (s1, 0, sizeof (s1));
        std::memset (s2, 0, sizeof (s2));
        coeffsInitialized = false;
    }

private:
    // SoA: group coefficients by field, not by filter (enables AVX)
    float b0[kSize], b2[kSize];
    float a1[kSize], a2[kSize];
    float prevB0[kSize], prevB2[kSize]; // interpolation start points
    float prevA1[kSize], prevA2[kSize];
    float s1[8][kSize], s2[8][kSize];  // per-stage, per-harmonic biquad state

    double sr = 44100.0;
    bool coeffsInitialized = false;
};
