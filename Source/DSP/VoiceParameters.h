#pragma once
#include <JuceHeader.h>

static constexpr int kNumHarmonics = 32;
static constexpr int kMaxVoices    = 8;

struct VoiceParameters
{
    int   excitationMode;       // 0=Noise, 1=Wavetable, 2=Sampler
    int   noiseColour;          // 0=White, 1=Pink, 2=Brown
    float wavetablePosition;    // 0–1

    float overallQ;
    float filterDetuneCents;    // –100 to +100
    float filterStretch;        // 0–1
    float filterSpread;         // 0–1

    float attackMs;
    float decayMs;
    float sustainLevel;         // 0–1
    float releaseMs;

    float timbreMorph;          // 0–1
    float gainMorph;            // 0–1
    float noteMorphAmount;      // 0–1
    float velocityMorphAmount;  // 0–1

    float masterGainLinear;
    int   polyphony;            // 1–8

    const float* snapshotA;     // [kNumHarmonics] linear gains
    const float* snapshotB;     // [kNumHarmonics] linear gains

    const juce::AudioBuffer<float>* samplerBuffer; // nullptr if no file loaded
    bool  samplerLoopEnable;
    float samplerLoopStart;     // 0–1
    float samplerLoopEnd;       // 0–1
    bool  samplerLoopPingPong;
    float samplerLoopCrossfadeMs;
};
