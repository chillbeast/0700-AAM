// SPDX-License-Identifier: AGPL-3.0-or-later
namespace mt
{
    int MTAudioInterface::getPadVelocity(int pad, int velocity) const
    {
        if(sixteenLevels) return juce::jlimit(1, 127, juce::roundToInt((pad + 1) * 127.0 / 16.0));
        return fixedPadVelocity ? fixedPadVelocity : juce::jlimit(1, 127, velocity);
    }

    int MTAudioInterface::pressPerformancePad(int pad, int note, int velocity)
    {
        if(!manager) return -1;
        const juce::ScopedLock guard(manager->getSequenceLock());
        if(!manager || !juce::isPositiveAndBelow(pad, 16) || !juce::isPositiveAndBelow(note, 128)) return -1;
        auto* track = manager->getTrack(getSelected());
        if(!track) return -1;
        auto& slot = performancePads[(size_t)pad];
        if(slot.note >= 0 && slot.sounding && manager->getTrack(slot.target) && manager->getTrack(slot.target)->getState() == slot.track)
            addLiveMidiMsg(slot.target, juce::MidiMessage::noteOff(1, slot.note));
        slot = {};
        slot.target = getSelected(); slot.track = track->getState();
        slot.note = sixteenLevels ? sixteenNote : note;
        slot.velocity = getPadVelocity(pad, velocity);
        slot.down = slot.sounding = true;
        if(!sixteenLevels) lastPadNote = note;
        addLiveMidiMsg(slot.target, juce::MidiMessage::noteOn(1, slot.note, (juce::uint8)slot.velocity));
        if(noteRepeat)
        {
            auto* sequence = manager->getSequence(slot.target.sequence);
            auto timing = getTimingCorrect();
            const double step = timing.grid();
            slot.clocked = isSequencePlaying();
            const double now = slot.clocked && sequence ? double(sequence->getElapsedTicks())
                : juce::Time::getMillisecondCounterHiRes() * juce::jmax(1, sequence ? sequence->getBPM() : 120) * defaultPPQN / 60000.0;
            slot.next = timing.nextGridTick(now);
            slot.gateEnd = now + step * 0.5;
            startTimer(1);
        }
        return slot.note;
    }

    void MTAudioInterface::releasePerformancePad(int pad)
    {
        if(!manager) return;
        const juce::ScopedLock guard(manager->getSequenceLock());
        if(!manager || !juce::isPositiveAndBelow(pad, 16)) return;
        auto& slot = performancePads[(size_t)pad];
        slot.down = false;
        if(noteRepeat && repeatLatch) return;
        if(slot.note >= 0 && slot.sounding)
        {
            // Several 16 Levels pads may own the same note. The last release
            // owns note-off; an earlier release must not cut the remaining pad.
            bool otherHeld = false;
            for(const auto& other : performancePads)
                if(&other != &slot && other.down && other.note == slot.note && other.track == slot.track) otherHeld = true;
            if(!otherHeld)
                if(auto* track = manager->getTrack(slot.target); track && track->getState() == slot.track)
                    addLiveMidiMsg(slot.target, juce::MidiMessage::noteOff(1, slot.note));
        }
        slot = {};
    }

    void MTAudioInterface::pressurePerformancePad(int pad, int value)
    {
        if(!manager) return;
        const juce::ScopedLock guard(manager->getSequenceLock());
        if(!manager || !juce::isPositiveAndBelow(pad, 16)) return;
        const auto& slot = performancePads[(size_t)pad];
        if(slot.note >= 0)
            if(auto* track = manager->getTrack(slot.target); track && track->getState() == slot.track)
                addLiveMidiMsg(slot.target, juce::MidiMessage::aftertouchChange(1, slot.note, juce::jlimit(0, 127, value)));
    }

    void MTAudioInterface::releasePerformancePads()
    {
        if(!manager) return;
        const juce::ScopedLock guard(manager->getSequenceLock());
        if(!manager) return;
        for(auto& slot : performancePads)
        {
            if(slot.note >= 0 && slot.sounding)
                if(auto* track = manager->getTrack(slot.target); track && track->getState() == slot.track)
                    addLiveMidiMsg(slot.target, juce::MidiMessage::noteOff(1, slot.note));
            slot = {};
        }
    }

    void MTAudioInterface::setNoteRepeat(bool enabled)
    {
        if(enabled == noteRepeat) return;
        releasePerformancePads();
        noteRepeat = enabled;
        if(!enabled) repeatLatch = false;
    }
    void MTAudioInterface::setRepeatLatch(bool enabled)
    {
        repeatLatch = enabled;
        if(enabled) noteRepeat = true;
        else for(int i = 0; i < 16; ++i) if(!performancePads[(size_t)i].down) releasePerformancePad(i);
    }
    void MTAudioInterface::setSixteenLevels(bool enabled)
    {
        if(enabled == sixteenLevels) return;
        releasePerformancePads();
        sixteenLevels = enabled;
        if(enabled) sixteenNote = lastPadNote;
    }

    void MTAudioInterface::servicePadRepeats()
    {
        if(!noteRepeat) return;
        for(auto& slot : performancePads)
        {
            if(slot.note < 0) continue;
            auto* track = manager->getTrack(slot.target);
            auto* sequence = manager->getSequence(slot.target.sequence);
            if(!track || !sequence || track->getState() != slot.track) { slot = {}; continue; }
            if(sequence->isCountingIn()) continue;
            auto timing = TimingCorrect::read(sequence->getState());
            timing.division = sequence->getTempoSettings().getQuantizationLevel();
            timing.barTicks = sequence->getTicksPerBar();
            const double step = timing.grid();
            const bool clocked = isSequencePlaying();
            const double now = clocked ? double(sequence->getElapsedTicks())
                : juce::Time::getMillisecondCounterHiRes() * juce::jmax(1, sequence->getBPM()) * defaultPPQN / 60000.0;
            if(clocked != slot.clocked)
            {
                // STOP/pause releases repeats, including latched notes.
                if(slot.clocked) { releasePerformancePads(); return; }
                slot.clocked = clocked;
                slot.next = timing.nextGridTick(now);
                slot.gateEnd = now;
            }
            if(slot.sounding && now >= slot.gateEnd)
            {
                addLiveMidiMsg(slot.target, juce::MidiMessage::noteOff(1, slot.note));
                slot.sounding = false;
            }
            if(now >= slot.next)
            {
                // Recover phase after UI stalls without replaying a burst of old hits.
                slot.next = timing.nextGridTick(now);
                slot.gateEnd = now + step * 0.5;
                addLiveMidiMsg(slot.target, juce::MidiMessage::noteOn(1, slot.note, (juce::uint8)slot.velocity));
                slot.sounding = true;
            }
        }
    }
}
