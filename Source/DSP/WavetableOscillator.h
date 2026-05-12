#pragma once
#include <JuceHeader.h>
#include <cmath>

// Static wavetable bank: Sine, Saw, Square, Triangle, White, Pink, Brown.
// Generated once on first access; shared across all voices.
struct WavetableBank
{
    static constexpr int kTableSize = 2048;
    static constexpr int kNumTables = 7;

    float tables[kNumTables][kTableSize];

    static const WavetableBank& get() noexcept
    {
        static WavetableBank instance;
        return instance;
    }

private:
    WavetableBank() noexcept { build(); }

    void build() noexcept
    {
        const float twoPi = 2.0f * juce::MathConstants<float>::pi;

        // 0: Sine
        for (int i = 0; i < kTableSize; ++i)
            tables[0][i] = std::sin (twoPi * (float)i / kTableSize);

        // 1: Saw (–1 to +1)
        for (int i = 0; i < kTableSize; ++i)
            tables[1][i] = 2.0f * (float)i / kTableSize - 1.0f;

        // 2: Square
        for (int i = 0; i < kTableSize; ++i)
            tables[2][i] = (i < kTableSize / 2) ? 1.0f : -1.0f;

        // 3: Triangle
        for (int i = 0; i < kTableSize; ++i)
        {
            float t = (float)i / kTableSize;
            tables[3][i] = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
        }

        // 4: White noise (fixed seed for reproducible table)
        juce::Random rng (12345);
        for (int i = 0; i < kTableSize; ++i)
            tables[4][i] = rng.nextFloat() * 2.0f - 1.0f;

        // 5: Pink noise (Paul Kellett algorithm over the table)
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        juce::Random pRng (67890);
        for (int i = 0; i < kTableSize; ++i)
        {
            float w = pRng.nextFloat() * 2.0f - 1.0f;
            b0 = 0.99886f*b0 + w*0.0555179f; b1 = 0.99332f*b1 + w*0.0750759f;
            b2 = 0.96900f*b2 + w*0.1538520f; b3 = 0.86650f*b3 + w*0.3104856f;
            b4 = 0.55000f*b4 + w*0.5329522f; b5 = -0.7616f*b5  - w*0.0168980f;
            tables[5][i] = (b0+b1+b2+b3+b4+b5+b6+w*0.5362f) * 0.11f;
            b6 = w * 0.115926f;
        }

        // 6: Brown noise (leaky integrator)
        float brown = 0;
        juce::Random brRng (11111);
        for (int i = 0; i < kTableSize; ++i)
        {
            float w = brRng.nextFloat() * 2.0f - 1.0f;
            brown = (brown + 0.02f * w) / 1.02f;
            tables[6][i] = brown * 3.5f;
        }
    }
};

// Per-voice oscillator that reads from WavetableBank and pitch-tracks via MIDI note.
class WavetableOscillator
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        phase = 0.0f;
        phaseIncrement = 0.0f;
    }

    void setFrequency (float hz) noexcept
    {
        phaseIncrement = hz / (float)sampleRate;
    }

    void setPosition (float pos) noexcept
    {
        position = juce::jlimit (0.0f, 1.0f, pos);
    }

    void reset() noexcept { phase = 0.0f; }

    float getNextSample() noexcept
    {
        const auto& bank = WavetableBank::get();
        const int N = WavetableBank::kTableSize;
        const int T = WavetableBank::kNumTables;

        float tablePos = position * (float)(T - 1);
        int   t0 = (int)tablePos;
        int   t1 = juce::jmin (t0 + 1, T - 1);
        float tableBlend = tablePos - (float)t0;

        float phaseF = phase * (float)N;
        int   p0 = (int)phaseF & (N - 1);
        int   p1 = (p0 + 1) & (N - 1);
        float pFrac = phaseF - (float)(int)phaseF;

        auto lerp = [](float a, float b, float t) { return a + t * (b - a); };

        float s0 = lerp (bank.tables[t0][p0], bank.tables[t0][p1], pFrac);
        float s1 = lerp (bank.tables[t1][p0], bank.tables[t1][p1], pFrac);

        phase += phaseIncrement;
        if (phase >= 1.0f) phase -= 1.0f;

        return lerp (s0, s1, tableBlend);
    }

private:
    double sampleRate    = 44100.0;
    float  phase         = 0.0f;
    float  phaseIncrement= 0.0f;
    float  position      = 0.0f; // 0–1 mapping to table 0–6
};
