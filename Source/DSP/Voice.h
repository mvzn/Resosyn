#pragma once
#include <JuceHeader.h>
#include "VoiceParameters.h"
#include "FilterBank.h"
#include "NoiseGenerator.h"
#include "WavetableOscillator.h"

class Voice
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        filterBank.prepare (sampleRate);
        wavetableOsc.prepare (sampleRate);
        noiseGen.reset();

        envelope.setSampleRate (sampleRate);

        excitationBuf.setSize (1, maxBlockSize);
        tempL.setSize (1, maxBlockSize);
        tempR.setSize (1, maxBlockSize);
    }

    void noteOn (int note, float vel, uint64_t age)
    {
        midiNote    = note;
        velocity    = vel;
        startAge    = age;
        active      = true;
        samplerPhase          = 0.0;
        samplerPingPongFwd    = true;

        envelope.reset();
        envelope.noteOn();
        filterBank.flushState();
        wavetableOsc.reset();
        wavetableOsc.setFrequency (midiNoteToHz (note));
    }

    void noteOff() noexcept { envelope.noteOff(); }

    bool isActive()  const noexcept { return active; }
    int  getMidiNote() const noexcept { return midiNote; }
    uint64_t getStartAge() const noexcept { return startAge; }

    // Adds voice audio to buffer[startSample..startSample+numSamples].
    void process (juce::AudioBuffer<float>& output,
                  int startSample,
                  int numSamples,
                  const VoiceParameters& p)
    {
        if (!active) return;

        updateEnvelopeParams (p);

        float morphPos = computeMorphPos (p);
        float blendedGains[kNumHarmonics];
        computeBlendedGains (p, morphPos, blendedGains);
        for (int k = p.harmonicCount; k < kNumHarmonics; ++k)
            blendedGains[k] = 0.0f;

        static const float kZero32[kNumHarmonics] = {};
        static const float kOne32[kNumHarmonics]  = {
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
        };

        float fundamental = midiNoteToHz (midiNote);
        // Apply global detune
        if (p.filterDetuneCents != 0.0f)
            fundamental *= std::pow (2.0f, p.filterDetuneCents / 1200.0f);

        float* excBuf = excitationBuf.getWritePointer (0);
        float* outL   = output.getWritePointer (0) + startSample;
        float* outR   = output.getWritePointer (1) + startSample;
        float* tmpL   = tempL.getWritePointer (0);
        float* tmpR   = tempR.getWritePointer (0);

        static constexpr int kSubBlock = 32;
        int offset = 0;

        while (offset < numSamples)
        {
            int n = std::min (kSubBlock, numSamples - offset);

            filterBank.updateCoefficients (fundamental, p.filterStretch,
                                           p.overallQ, kZero32, kZero32, kOne32,
                                           p.filterType, p.peakGainMasterDB);

            generateExcitation (excBuf, n, p);

            // Apply envelope per-sample into excitation
            for (int i = 0; i < n; ++i)
                excBuf[i] *= envelope.getNextSample();

            std::memset (tmpL, 0, sizeof (float) * (size_t)n);
            std::memset (tmpR, 0, sizeof (float) * (size_t)n);

            if (p.filterType == 1)
                filterBank.processPeak (excBuf, tmpL, tmpR, n, p.filterSpread, blendedGains);
            else
                filterBank.process (excBuf, tmpL, tmpR, n, p.filterSpread, blendedGains);

            float masterGain = p.masterGainLinear;
            for (int i = 0; i < n; ++i)
            {
                outL[offset + i] += tmpL[i] * masterGain;
                outR[offset + i] += tmpR[i] * masterGain;
            }

            offset += n;
        }

        if (!envelope.isActive())
        {
            active = false;
            filterBank.flushState();
        }
    }

private:
    // DSP components
    FilterBank         filterBank;
    juce::ADSR         envelope;
    NoiseGenerator     noiseGen;
    WavetableOscillator wavetableOsc;

    // Voice state
    int      midiNote  = -1;
    float    velocity  = 0.0f;
    uint64_t startAge  = 0;
    bool     active    = false;
    double   sr        = 44100.0;

    // Sampler playhead
    double samplerPhase      = 0.0;
    bool   samplerPingPongFwd= true;

    // Pre-allocated scratch buffers
    juce::AudioBuffer<float> excitationBuf;
    juce::AudioBuffer<float> tempL, tempR;

    static float midiNoteToHz (int note) noexcept
    {
        return 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
    }

    void updateEnvelopeParams (const VoiceParameters& p) noexcept
    {
        juce::ADSR::Parameters ep;
        ep.attack  = p.attackMs  * 0.001f;
        ep.decay   = p.decayMs   * 0.001f;
        ep.sustain = p.sustainLevel;
        ep.release = p.releaseMs * 0.001f;
        envelope.setParameters (ep);
    }

    float computeMorphPos (const VoiceParameters& p) const noexcept
    {
        float noteHeight = (float)midiNote / 127.0f;
        float pos = p.timbreMorph
                  + noteHeight * p.noteMorphAmount
                  + velocity   * p.velocityMorphAmount;
        return juce::jlimit (0.0f, 1.0f, pos);
    }

    void computeBlendedGains (const VoiceParameters& p,
                              float morphPos,
                              float* gains) const noexcept
    {
        // Timbre morph: blend relative spectral shapes A and B
        float aSum = 0.0f, bSum = 0.0f;
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            aSum += p.snapshotA[k];
            bSum += p.snapshotB[k];
        }
        float aNorm = (aSum > 0.0f) ? 1.0f / aSum : 0.0f;
        float bNorm = (bSum > 0.0f) ? 1.0f / bSum : 0.0f;

        // Gain morph: blend overall level
        float gainMorphPos = juce::jlimit (0.0f, 1.0f, p.gainMorph);
        float levelA = aSum * (float)kNumHarmonics;
        float levelB = bSum * (float)kNumHarmonics;
        float overallLevel = levelA + gainMorphPos * (levelB - levelA);
        overallLevel /= (float)kNumHarmonics; // normalize to per-harmonic scale

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            float shapeA = p.snapshotA[k] * aNorm;
            float shapeB = p.snapshotB[k] * bNorm;
            float shape  = shapeA + morphPos * (shapeB - shapeA);
            gains[k] = shape * overallLevel;
        }
    }

    void generateExcitation (float* buf, int n, const VoiceParameters& p) noexcept
    {
        switch (p.excitationMode)
        {
            case 1: // Wavetable
                wavetableOsc.setFrequency (midiNoteToHz (midiNote));
                wavetableOsc.setPosition (p.wavetablePosition);
                for (int i = 0; i < n; ++i)
                    buf[i] = wavetableOsc.getNextSample();
                break;

            case 2: // Sampler
                if (p.samplerBuffer != nullptr && p.samplerBuffer->getNumSamples() > 0)
                    generateSamplerExcitation (buf, n, p);
                else
                    std::memset (buf, 0, sizeof (float) * (size_t)n);
                break;

            default: // Noise (0)
                noiseGen.setColour (static_cast<NoiseGenerator::Colour> (p.noiseColour));
                for (int i = 0; i < n; ++i)
                    buf[i] = noiseGen.getNextSample();
                break;
        }
    }

    void generateSamplerExcitation (float* buf, int n, const VoiceParameters& p) noexcept
    {
        const auto& sb = *p.samplerBuffer;
        const int totalSamples = sb.getNumSamples();
        const int numCh        = sb.getNumChannels();

        int loopStart = (int)(p.samplerLoopStart * (float)totalSamples);
        int loopEnd   = (int)(p.samplerLoopEnd   * (float)totalSamples);
        loopStart = juce::jlimit (0, totalSamples - 1, loopStart);
        loopEnd   = juce::jlimit (loopStart + 1, totalSamples, loopEnd);

        for (int i = 0; i < n; ++i)
        {
            int pos = juce::jlimit (0, totalSamples - 1, (int)samplerPhase);
            float sample = sb.getSample (0, pos);
            if (numCh > 1) sample = (sample + sb.getSample (1, pos)) * 0.5f;
            buf[i] = sample;

            if (samplerPingPongFwd)
            {
                samplerPhase += 1.0;
                if (p.samplerLoopEnable && samplerPhase >= (double)loopEnd)
                {
                    if (p.samplerLoopPingPong) { samplerPhase = (double)(loopEnd - 1); samplerPingPongFwd = false; }
                    else                       { samplerPhase = (double)loopStart; }
                }
                else if (!p.samplerLoopEnable && samplerPhase >= (double)totalSamples)
                {
                    samplerPhase = (double)(totalSamples - 1);
                }
            }
            else
            {
                samplerPhase -= 1.0;
                if (samplerPhase <= (double)loopStart) { samplerPhase = (double)loopStart; samplerPingPongFwd = true; }
            }
        }
    }
};
