#pragma once
#include <JuceHeader.h>

static constexpr int kNumHarmonics = 32;
static constexpr int kMaxVoices    = 8;

struct VoiceParameters
{
    int   excitationMode;       // 0=Noise, 1=Wavetable, 2=Sampler
    int   noiseColour;          // 0=White, 1=Pink, 2=Brown
    float wavetablePosition;    // 0–1

    int   filterType;           // 0=Bandpass, 1=Peak
    int   filterStages;         // 1–8, cascaded biquad stages (order = stages*2)
    bool  phaseAlign;           // compensate per-harmonic group delay (see DEVNOTES.md)
    int   harmonicCount;        // 1–32, harmonics above this are silenced

    float overallQ;
    float filterDetuneCents;    // –100 to +100
    float filterStretch;        // 0–1
    float filterSpread;         // 0–1
    float peakGainMasterDB;     // –96 to +46 dB, used only in Peak mode

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
