#pragma once
#include <JuceHeader.h>

class NoiseGenerator
{
public:
    enum class Colour { White, Pink, Brown };

    void setColour (Colour c) noexcept { colour = c; }

    float getNextSample() noexcept
    {
        float white = rng.nextFloat() * 2.0f - 1.0f;
        switch (colour)
        {
            case Colour::Pink:  return generatePink (white);
            case Colour::Brown: return generateBrown (white);
            default:            return white * 0.1f;
        }
    }

    void reset() noexcept
    {
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
        brownLevel = 0.0f;
    }

private:
    float generatePink (float white) noexcept
    {
        // Paul Kellett's 6-pole pink noise approximation
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        return pink * 0.11f;
    }

    float generateBrown (float white) noexcept
    {
        brownLevel = (brownLevel + 0.02f * white) / 1.02f;
        return brownLevel * 3.5f;
    }

    Colour colour = Colour::White;
    juce::Random rng;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float brownLevel = 0;
};
