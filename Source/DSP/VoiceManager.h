#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "Voice.h"
#include "VoiceParameters.h"

class VoiceManager
{
public:
    // Most-recently-played MIDI note. Updated on noteOn; read by the UI for
    // the filter response display reference pitch. Defaults to A4 (69).
    std::atomic<int> lastPlayedNote { 69 };

    void prepare (double sampleRate, int maxBlockSize)
    {
        for (auto& v : voices)
            v.prepare (sampleRate, maxBlockSize);
        ageCounter = 0;
    }

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
    // First kMaxVoices slots receive new notes; the full pool supports release ghosts.
    std::array<Voice, kTotalVoiceSlots> voices;
    uint64_t ageCounter = 0;

    void renderVoices (juce::AudioBuffer<float>& buffer,
                       int startSample, int numSamples,
                       const VoiceParameters& params) const
    {
        for (auto& v : const_cast<std::array<Voice, kTotalVoiceSlots>&> (voices))
            v.process (buffer, startSample, numSamples, params);
    }

    void handleNoteOn (int note, float velocity, int polyphony)
    {
        lastPlayedNote.store (note, std::memory_order_relaxed);

        // Release any non-ghost voice currently playing this note.
        for (auto& v : voices)
            if (v.isActive() && !v.isInRelease() && v.getMidiNote() == note)
                v.noteOff();

        // If at the polyphony limit, gracefully release the oldest sounding voice.
        // It continues as a ghost in its current slot — no cut, no latency penalty.
        if (countSoundingVoices() >= polyphony)
        {
            Voice* oldest = oldestSoundingVoice();
            if (oldest) oldest->noteOff();
        }

        // Start the new note on a free slot immediately (zero latency).
        Voice* target = findFreeVoice();

        // Fallback: all 16 slots occupied — steal the oldest release ghost.
        // The steal fade in Voice handles this with a short ramp-out.
        if (target == nullptr)
            target = oldestReleaseVoice();

        if (target != nullptr)
            target->noteOn (note, velocity, ageCounter++);
    }

    void handleNoteOff (int note) noexcept
    {
        // Only release the held (non-ghost) voice; ghosts play out uninterrupted.
        for (auto& v : voices)
            if (v.isActive() && !v.isInRelease() && v.getMidiNote() == note)
                v.noteOff();
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            if (v.isActive() && !v.isInRelease())
                v.noteOff();
    }

    // ── Voice pool helpers ────────────────────────────────────────────────────

    int countSoundingVoices() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            if (v.isActive() && !v.isInRelease()) ++n;
        return n;
    }

    Voice* findFreeVoice() noexcept
    {
        for (auto& v : voices)
            if (!v.isActive()) return &v;
        return nullptr;
    }

    // Oldest sounding (non-release) voice — candidate for graceful release.
    Voice* oldestSoundingVoice() noexcept
    {
        Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && !v.isInRelease())
                if (!oldest || v.getStartAge() < oldest->getStartAge())
                    oldest = &v;
        return oldest;
    }

    // Oldest release ghost — last-resort steal target.
    Voice* oldestReleaseVoice() noexcept
    {
        Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && v.isInRelease())
                if (!oldest || v.getStartAge() < oldest->getStartAge())
                    oldest = &v;
        return oldest;
    }
};
