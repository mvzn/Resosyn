#pragma once
#include <JuceHeader.h>
#include "VoiceParameters.h"
#include "FilterBank.h"
#include "NoiseGenerator.h"
#include "WavetableOscillator.h"

class Voice
{
public:
    static constexpr int kStealFadeLen       = 128;
    static constexpr int kPhaseAlignBufSize  = 8192;   // must be power-of-two
    static constexpr int kRingBufMask        = kPhaseAlignBufSize - 1;
    static constexpr int kMaxPreDelaySamples = 4096;

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
        if (active)
        {
            // Voice is being stolen: fade out current note, queue the new one.
            pendingNote     = note;
            pendingVelocity = vel;
            pendingAge      = age;
            hasPendingNote  = true;
            stealFadeRemain = kStealFadeLen;
        }
        else
        {
            startNote (note, vel, age);
        }
    }

    void noteOff() noexcept { envelope.noteOff(); inRelease = true; }

    bool     isActive()    const noexcept { return active; }
    bool     isInRelease() const noexcept { return inRelease; }
    int      getMidiNote() const noexcept { return midiNote; }
    uint64_t getStartAge() const noexcept { return startAge; }

    // Adds voice audio to buffer[startSample..startSample+numSamples].
    void process (juce::AudioBuffer<float>& output,
                  int startSample,
                  int numSamples,
                  const VoiceParameters& p)
    {
        if (!active) return;

        updateEnvelopeParams (p);

        float fundamental = midiNoteToHz (midiNote);
        if (p.filterDetuneCents != 0.0f)
            fundamental *= std::pow (2.0f, p.filterDetuneCents / 1200.0f);

        static const float kZero32[kNumHarmonics] = {};
        static const float kOne32[kNumHarmonics]  = {
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
        };

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

            // Recompute blended gains every subblock so morph automation is responsive.
            float blendedGains[kNumHarmonics];
            float morphPos = computeMorphPos (p);
            computeBlendedGains (p, morphPos, blendedGains);
            for (int k = p.harmonicCount; k < kNumHarmonics; ++k)
                blendedGains[k] = 0.0f;

            // Divide peak gain across stages so total gain equals the target.
            float perStagePeakGainDB = p.peakGainMasterDB / (float)p.filterStages;
            filterBank.updateCoefficients (fundamental, p.filterStretch,
                                           p.overallQ, kZero32, kZero32, kOne32,
                                           p.filterType, perStagePeakGainDB);

            generateExcitation (excBuf, n, p);

            for (int i = 0; i < n; ++i)
                excBuf[i] *= envelope.getNextSample();

            for (int i = 0; i < n; ++i)
                excRingBuf[(ringWritePos + i) & kRingBufMask] = excBuf[i];

            std::memset (tmpL, 0, sizeof (float) * (size_t)n);
            std::memset (tmpR, 0, sizeof (float) * (size_t)n);

            if (p.phaseAlign)
            {
                int preDelays[kNumHarmonics];
                float maxDelayF = (float)p.filterStages * p.overallQ * (float)sr
                                  / (juce::MathConstants<float>::pi * fundamental);
                maxDelayF = std::min (maxDelayF, (float)kMaxPreDelaySamples);

                for (int k = 0; k < kNumHarmonics; ++k)
                {
                    float harmN = (float)(k + 1);
                    preDelays[k] = (int)(maxDelayF * (1.0f - 1.0f / harmN) + 0.5f);
                }

                if (p.filterType == 1)
                    filterBank.processAlignedPeak (excRingBuf, kRingBufMask, ringWritePos,
                                                   preDelays, tmpL, tmpR, n,
                                                   p.filterSpread, blendedGains, p.filterStages);
                else
                    filterBank.processAligned (excRingBuf, kRingBufMask, ringWritePos,
                                               preDelays, tmpL, tmpR, n,
                                               p.filterSpread, blendedGains, p.filterStages);
            }
            else
            {
                if (p.filterType == 1)
                    filterBank.processPeak (excBuf, tmpL, tmpR, n, p.filterSpread, blendedGains, p.filterStages);
                else
                    filterBank.process (excBuf, tmpL, tmpR, n, p.filterSpread, blendedGains, p.filterStages);
            }

            ringWritePos = (ringWritePos + n) & kRingBufMask;

            // Apply steal fade — ramp down over kStealFadeLen samples before firing pending note.
            if (stealFadeRemain > 0)
            {
                for (int i = 0; i < n; ++i)
                {
                    float gain = (float)stealFadeRemain / (float)kStealFadeLen;
                    tmpL[i] *= gain;
                    tmpR[i] *= gain;

                    if (--stealFadeRemain == 0)
                    {
                        if (hasPendingNote)
                            startNote (pendingNote, pendingVelocity, pendingAge);

                        // Zero samples after the transition point this subblock.
                        for (int j = i + 1; j < n; ++j)
                            tmpL[j] = tmpR[j] = 0.0f;
                        break;
                    }
                }
            }

            for (int i = 0; i < n; ++i)
            {
                outL[offset + i] += tmpL[i];
                outR[offset + i] += tmpR[i];
            }

            offset += n;
        }

        if (!envelope.isActive() && stealFadeRemain == 0 && !hasPendingNote)
        {
            active = false;
            filterBank.flushState();
        }
    }

private:
    FilterBank          filterBank;
    juce::ADSR          envelope;
    NoiseGenerator      noiseGen;
    WavetableOscillator wavetableOsc;

    int      midiNote   = -1;
    float    velocity   = 0.0f;
    uint64_t startAge   = 0;
    bool     active     = false;
    bool     inRelease  = false;
    double   sr         = 44100.0;

    // Sampler playhead
    double samplerPhase       = 0.0;
    bool   samplerPingPongFwd = true;

    // Phase align ring buffer (stores envelope-applied excitation history)
    float excRingBuf[kPhaseAlignBufSize] {};
    int   ringWritePos = 0;

    // Steal fade state
    int      stealFadeRemain  = 0;
    bool     hasPendingNote   = false;
    int      pendingNote      = -1;
    float    pendingVelocity  = 0.0f;
    uint64_t pendingAge       = 0;

    juce::AudioBuffer<float> excitationBuf;
    juce::AudioBuffer<float> tempL, tempR;

    static float midiNoteToHz (int note) noexcept
    {
        return 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
    }

    void startNote (int note, float vel, uint64_t age) noexcept
    {
        midiNote          = note;
        velocity          = vel;
        startAge          = age;
        active            = true;
        inRelease         = false;
        samplerPhase      = 0.0;
        samplerPingPongFwd= true;
        hasPendingNote    = false;
        stealFadeRemain   = 0;
        ringWritePos      = 0;
        std::memset (excRingBuf, 0, sizeof (excRingBuf));

        envelope.reset();
        envelope.noteOn();
        filterBank.flushState();
        wavetableOsc.reset();
        wavetableOsc.setFrequency (midiNoteToHz (note));
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
        float aSum = 0.0f, bSum = 0.0f;
        for (int k = 0; k < kNumHarmonics; ++k)
        {
            aSum += p.snapshotA[k];
            bSum += p.snapshotB[k];
        }
        float aNorm = (aSum > 0.0f) ? 1.0f / aSum : 0.0f;
        float bNorm = (bSum > 0.0f) ? 1.0f / bSum : 0.0f;

        float gainMorphPos = juce::jlimit (0.0f, 1.0f, p.gainMorph);
        float levelA = aSum * (float)kNumHarmonics;
        float levelB = bSum * (float)kNumHarmonics;
        float overallLevel = (levelA + gainMorphPos * (levelB - levelA)) / (float)kNumHarmonics;

        for (int k = 0; k < kNumHarmonics; ++k)
        {
            float shapeA = p.snapshotA[k] * aNorm;
            float shapeB = p.snapshotB[k] * bNorm;
            gains[k] = (shapeA + morphPos * (shapeB - shapeA)) * overallLevel;
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

        int loopStart = juce::jlimit (0, totalSamples - 1, (int)(p.samplerLoopStart * (float)totalSamples));
        int loopEnd   = juce::jlimit (loopStart + 1, totalSamples, (int)(p.samplerLoopEnd * (float)totalSamples));

        for (int i = 0; i < n; ++i)
        {
            // Linear interpolation between adjacent samples
            int   pos0 = juce::jlimit (0, totalSamples - 1, (int)samplerPhase);
            int   pos1 = std::min (pos0 + 1, totalSamples - 1);
            float frac = (float)(samplerPhase - (double)pos0);

            float s0 = sb.getSample (0, pos0);
            float s1 = sb.getSample (0, pos1);
            if (numCh > 1)
            {
                s0 = (s0 + sb.getSample (1, pos0)) * 0.5f;
                s1 = (s1 + sb.getSample (1, pos1)) * 0.5f;
            }
            buf[i] = s0 + frac * (s1 - s0);

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
