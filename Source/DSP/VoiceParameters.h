#pragma once
#include <JuceHeader.h>

static constexpr int kNumHarmonics    = 32;
static constexpr int kMaxVoices       = 12;
static constexpr int kTotalVoiceSlots = kMaxVoices * 2; // polyphony slots + ghost release slots

struct VoiceParameters
{
    int   excitationMode;       // 0=Noise, 1=Wavetable, 2=Sampler
    int   noiseColour;          // 0=White, 1=Pink, 2=Brown
    float wavetablePosition;    // 0–1

    int   filterType;           // 0=Bandpass, 1=Peak
    int   filterStages;         // 1–8, cascaded biquad stages (order = stages*2)
    bool  phaseAlign;           // compensate per-harmonic group delay (see DEVNOTES.md)
    int   harmonicCount;        // 1–32, harmonics above this are silenced
    int   harmonicStart;        // 1–32, harmonics below this are silenced (low-cut)

    float overallQ;
    float filterDetuneCents;    // –100 to +100
    float filterStretch;        // 0–1
    float filterSpread;         // 0–1
    float peakGainMasterDB;     // –96 to +46 dB, used only in Peak mode

    // 0–1: dry-subtraction compensation. Per-harmonic output becomes
    // (filter_k(input) - compensation*input)*gain_k; dry input added back once after.
    // Tames noise-floor blowup with many active harmonics. Always applied in Peak;
    // applied in Bandpass only if bandpassCompEnabled.
    float compensation;
    bool  bandpassCompEnabled;

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

    const float* snapshotA;              // [kNumHarmonics] linear gains
    const float* snapshotB;              // [kNumHarmonics] linear gains
    const float* perHarmonicFreqSemitones; // [kNumHarmonics] ±24 semitone offset per harmonic
    const float* perHarmonicDetuneCents;   // [kNumHarmonics] ±100 cents per harmonic
    const float* perHarmonicQMult;         // [kNumHarmonics] Q multiplier, default 1.0

    const juce::AudioBuffer<float>* samplerBuffer; // nullptr if no file loaded
    bool  samplerLoopEnable;
    float samplerLoopStart;     // 0–1
    float samplerLoopEnd;       // 0–1
    bool  samplerLoopPingPong;
    float samplerLoopCrossfadeMs;
};
