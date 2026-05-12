#pragma once
#include <JuceHeader.h>
#include "Voice.h"
#include "VoiceParameters.h"

class VoiceManager
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        for (auto& v : voices)
            v.prepare (sampleRate, maxBlockSize);
        ageCounter = 0;
    }

    // Sample-accurate MIDI processing interleaved with audio rendering.
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer&         midiMessages,
                       const VoiceParameters&    params)
    {
        buffer.clear();

        int currentSample = 0;
        const int totalSamples = buffer.getNumSamples();

        for (const auto& midiMsg : midiMessages)
        {
            int samplePos = midiMsg.samplePosition;
            auto msg = midiMsg.getMessage();

            if (samplePos > currentSample)
            {
                renderVoices (buffer, currentSample, samplePos - currentSample, params);
                currentSample = samplePos;
            }

            if      (msg.isNoteOn())  handleNoteOn  (msg.getNoteNumber(), msg.getFloatVelocity(), params.polyphony);
            else if (msg.isNoteOff()) handleNoteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) allNotesOff();
        }

        if (currentSample < totalSamples)
            renderVoices (buffer, currentSample, totalSamples - currentSample, params);
    }

private:
    std::array<Voice, kMaxVoices> voices;
    uint64_t ageCounter = 0;

    void renderVoices (juce::AudioBuffer<float>& buffer,
                       int startSample, int numSamples,
                       const VoiceParameters& params) const
    {
        for (auto& v : const_cast<std::array<Voice, kMaxVoices>&> (voices))
            v.process (buffer, startSample, numSamples, params);
    }

    void handleNoteOn (int note, float velocity, int polyphony)
    {
        // Silence any voice already playing this note
        for (auto& v : voices)
            if (v.isActive() && v.getMidiNote() == note)
                v.noteOff();

        Voice* target = findFreeVoice();
        if (target == nullptr)
            target = stealOldestVoice (polyphony);
        if (target == nullptr)
            return;

        target->noteOn (note, velocity, ageCounter++);
    }

    void handleNoteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.isActive() && v.getMidiNote() == note)
                v.noteOff();
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            v.noteOff();
    }

    Voice* findFreeVoice() noexcept
    {
        for (auto& v : voices)
            if (!v.isActive()) return &v;
        return nullptr;
    }

    // Steal the voice with the lowest (oldest) startAge, limited to polyphony slots.
    Voice* stealOldestVoice (int polyphony) noexcept
    {
        Voice* oldest = nullptr;
        int count = 0;
        for (auto& v : voices)
        {
            if (count >= polyphony) break;
            if (!oldest || v.getStartAge() < oldest->getStartAge())
                oldest = &v;
            ++count;
        }
        return oldest;
    }
};
