// End-to-end timing/recording checks use synthetic notes and an externally
// stopped clock so boundaries are deterministic. No physical MIDI is needed.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace
{
    void testTimingRecording(Results& R)
    {
        R.section("Timing, recording and performance controls");
        mt::TimingCorrect timing;
        R.check(timing.grid() == 24 && timing.correct(17) == 24 && timing.correct(48) == 48, "Timing: straight sixteenth grid and exact points");
        timing.swing = 65;
        R.check(timing.correct(24) == 31 && timing.correct(48) == 48, "Timing: swing delays alternate sixteenths within a pair");
        R.check(timing.correct(15) == 0, "Timing: a full window also covers the long side of a swung pair");
        timing.division = mt::MTTimeBase::_16T; timing.swing = 50;
        R.check(timing.grid() == 16 && timing.correct(14) == 16, "Timing: triplet grid uses PPQN rather than rounded display labels");
        timing = {}; timing.strength = 50;
        R.check(timing.correct(16) == 20, "Timing: partial strength moves halfway to the grid");
        timing.window = 10;
        R.check(timing.correct(16) == 16, "Timing: window excludes notes outside the correction radius");
        timing = {}; timing.enabled = false; timing.shift = 10;
        R.check(timing.correct(17) == 17, "Timing: bypass preserves live timing including shift");
        mt::TapTempo taps;
        R.check(taps.tap(1000, 4) == 0 && taps.tap(1500, 4) == 0 && taps.tap(2000, 4) == 0 && std::abs(taps.tap(2500, 4) - 120) < 0.01,
            "Tap tempo: configured four taps produce 120 BPM");
        R.check(taps.tap(2510, 4) == 0 && std::abs(taps.tap(3000, 4) - 120) < 0.01, "Tap tempo: switch bounce does not corrupt the average");
        R.check(taps.tap(6000, 4) == 0 && taps.tap(6500, 4) == 0, "Tap tempo: a long pause starts a new tap series");
        const auto click = mt::getAudioSoundCache().getBuiltinClick();
        bool finiteClick = click && click->isValid() && click->getData().getMagnitude(0, click->getData().getNumSamples()) > 0.01f;
        if(click) for(int i = 0; i < click->getData().getNumSamples(); ++i) finiteClick &= std::isfinite(click->getData().getSample(0, i));
        R.check(finiteClick, "Metronome: generated fallback has audible finite samples without factory content");
        {
            AudIO::Design design(2, 2);
            juce::ValueTree state(mt::IDs::METRONOME);
            mt::Metronome metronome(design, state);
            juce::String missing = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("missing-kick-click-" + juce::Uuid().toString() + ".wav").getFullPathName();
            metronome.setSource(missing); metronome.noteOn(0, 127);
            auto* voice = metronome.isPlayback(0);
            juce::AudioBuffer<float> rendered(2, AUDIO_BLOCK_SIZE); rendered.clear();
            if(voice) { voice->prepareToPlay(48000, AUDIO_BLOCK_SIZE); voice->process(rendered); }
            R.check(voice && rendered.getMagnitude(0, rendered.getNumSamples()) > 0.001f, "Metronome: missing factory sample renders the fallback through a real synth voice");
        }

        auto& ai = mt::getAudioInterface();
        pump(300);
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KickTiming-" + juce::Uuid().toString());
        auto project = scratch.getChildFile("Project"); project.createDirectory();
        ai.loadProject(project);
        const int sequenceIndex = ai.createSequence("Timing");
        const int trackIndex = ai.createTrack(sequenceIndex, "Pads");
        const int otherIndex = ai.createTrack(sequenceIndex, "Other");
        ai.setSelectedSequenceAndTrack(sequenceIndex, trackIndex);
        auto* sequence = ai.getManager()->getSequence(sequenceIndex);
        auto* track = ai.getManager()->getTrack(sequenceIndex, trackIndex);
        auto* other = ai.getManager()->getTrack(sequenceIndex, otherIndex);
        track->setUsingSeqLength(true); other->setUsingSeqLength(true);
        track->setMidiPort("All"); track->setMidiChannel(2);
        sequence->setLength(2); sequence->setLooping(false);
        auto& undo = ai.getUndoManager();
        R.check(ai.setTimeSignature(sequenceIndex, 3, 4) == 0 && sequence->getLengthInTicks() == 576 && track->getLengthInTicks() == 576,
            "Meter: 3/4 updates sequence and track tick lengths");
        sequence->setQuantization(mt::MTTimeBase::_16);
        R.check(sequence->getQuantizationPerBar() == 12, "Meter: 3/4 has twelve sixteenth steps");
        sequence->setQuantization(mt::MTTimeBase::_1);
        R.check(sequence->getTicksPerQuantization() == 288 && sequence->getQuantizationPerBar() == 1, "Meter: BAR division follows the meter instead of a fixed whole note");
        sequence->setQuantization(mt::MTTimeBase::_16);
        R.check(ai.setTimeSignature(sequenceIndex, 7, 8) == 0 && sequence->getTicksPerBar() == 336 && sequence->getLengthInTicks() == 672,
            "Meter: 7/8 uses eighth-note beats");
        sequence->setQuantization(mt::MTTimeBase::_4T);
        R.check(sequence->getQuantizationPerBar() == 6, "Meter: a partial final triplet step is included in 7/8");
        sequence->setQuantization(mt::MTTimeBase::_16);
        R.check(ai.setTimeSignature(sequenceIndex, 0, 4) == -1 && ai.setTimeSignature(sequenceIndex, 4, 3) == -1 && sequence->getNumerator() == 7,
            "Meter: invalid signatures leave the existing meter intact");
        sequence->useMasterBPM(true);
        ai.setMasterBPM(120);
        R.check(sequence->getLengthInTicks() == 672 && std::abs(sequence->getLengthInSeconds() - 3.5) < 0.01,
            "Meter: global BPM retains the sequence's own meter");
        ai.setTimeSignature(sequenceIndex, 4, 4);
        undo.beginNewTransaction(); undo.undo();
        R.check(sequence->getNumerator() == 7 && sequence->getDenominator() == 8 && track->getLengthInTicks() == 672,
            "Meter: undo restores both time signature and derived track lengths");
        undo.redo(); undo.clearUndoHistory();

        track->addOrModifyNote(60, 17, 10, 100); track->addOrModifyNote(61, 43, 10, 90);
        timing = {}; timing.selectedOnly = true;
        track->selectAllInNote(60, true);
        R.check(ai.applyTimingCorrect(ai.getSelected(), timing) == 1 && track->getNotesAtPos(24).size() == 1 && track->getNotesAtPos(43).size() == 1,
            "Timing Correct: selected scope edits the playback index and leaves other notes alone");
        undo.beginNewTransaction(); undo.undo();
        R.check(track->getNotesAtPos(17).size() == 1 && track->getNotesAtPos(24).empty(), "Timing Correct: one undo restores original positions");
        timing = {}; timing.shift = -48;
        ai.applyTimingCorrect(ai.getSelected(), timing);
        R.check(track->getNotesAtPos(0).size() == 2, "Timing Correct: negative shifted starts clamp at zero");
        track->deleteAllNotes();
        track->addOrModifyNote(60, 0, 10, 100); track->addOrModifyNote(60, 96, 10, 100); track->addOrModifyNote(64, 48, 10, 100);
        timing = {}; timing.type = mt::TimingCorrect::Legato;
        ai.applyTimingCorrect(ai.getSelected(), timing);
        R.check(track->getNotesAtPos(0)[0].length == 96, "Timing Correct: legato extends to the next note of the same pitch");
        track->deleteAllNotes(); track->addOrModifyNote(60, 5, 17, 100); track->addOrModifyNote(61, 400, 17, 100);
        timing = {}; timing.type = mt::TimingCorrect::End;
        ai.applyTimingCorrect(ai.getSelected(), timing);
        R.check(track->getNotesAtPos(5)[0].length == 19, "Timing Correct: End quantizes note-off without moving note-on");
        timing.type = mt::TimingCorrect::Length;
        ai.applyTimingCorrect(ai.getSelected(), timing);
        R.check(track->getNotesAtPos(5)[0].length == 24, "Timing Correct: Length quantizes duration independently of onset");
        timing = {}; timing.enabled = false; timing.rangeStart = 384; timing.rangeEnd = 768;
        R.check(ai.applyTimingCorrect(ai.getSelected(), timing) == 1 && track->getNotesAtPos(5).size() == 1 && track->getNotesAtPos(408).size() == 1,
            "Timing Correct: explicit Apply works with recording correction bypassed and respects the bar range");
        timing = {}; timing.swing = 65; ai.setTimingCorrect(timing);
        R.check(ai.getTimingCorrect().swing == 65 && (int)ai.getTimingCorrect().division == 16, "Timing Correct: recording settings are stored in the sequence");
        R.check(ai.saveProject(), "Timing: project saves with meter and correction settings");

        const bool externalBefore = mt::getSettings().midiControl.externalClock();
        const int countBefore = mt::getSettings().metronome.countIn();
        ai.setExternalMidiClock(true); // clock remains still until the test supplies ticks
        mt::getSettings().metronome.countIn = 0;
        juce::ValueTree captureState(mt::IDs::PROGRAM);
        AuditProgram capture(captureState, nullptr);
        mt::TrackAuditAccess::route(*track, &capture);
        sequence->setLooping(true); sequence->setLoopPoints(0, 1);
        track->deleteAllNotes(); track->addOrModifyNote(60, 0, 12, 100); track->addOrModifyNote(61, 400, 12, 100);
        other->addOrModifyNote(72, 0, 12, 100);
        undo.clearUndoHistory();
        R.check(ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Replace) && track->getNumNotes() == 1
                && track->getNotesAtPos(400).size() == 1 && other->getNumNotes() == 1,
            "REC: replaces the selected track's loop range once, preserving other tracks and outside notes");
        sequence->onTick(10);
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOn(1, 36, (juce::uint8)77));
        sequence->onTick(40);
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 36));
        ai.setSequencePause();
        R.check(track->getNumNotes() == 2 && track->getNotesAtPos(0).size() == 1 && track->getNotesAtPos(0)[0].velocity == 77,
            "REC: captures velocity and quantized onset into the replaced range");
        undo.undo();
        R.check(track->getNumNotes() == 2 && track->getNotesAtPos(0)[0].number == 60 && track->getNotesAtPos(400).size() == 1,
            "REC: a single undo restores the replaced notes and removes the take");

        mt::getSettings().metronome.countIn = 1;
        int countClicks = 0, normalClicks = 0;
        sigslot::scoped_connection countConnection = mt::signals::onCountIn.connect([&](bool) { ++countClicks; });
        sigslot::scoped_connection clickConnection = mt::signals::onMetronome.connect([&](bool) { ++normalClicks; });
        R.check(ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub) && ai.isCountingIn() && ai.getCountInBeatsRemaining() == 4,
            "Count-in: one bar arms four beats before recording");
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOn(1, 38, (juce::uint8)90));
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 38));
        sequence->onTick(383);
        R.check(ai.isCountingIn() && countClicks == 4 && normalClicks == 0 && track->getNumNotes() == 2,
            "Count-in: emits all four clicks, suppressing sequence playback and note capture");
        sequence->onTick(384);
        R.check(!ai.isCountingIn() && sequence->getElapsedTicks() == 0 && normalClicks == 1,
            "Count-in: recording starts exactly on the bar boundary, including zero elapsed ticks");
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOn(1, 38, (juce::uint8)90));
        sequence->onTick(408);
        ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 38));
        ai.setSequencePause();
        R.check(track->getNumNotes() == 3, "OVERDUB: adds the new take without erasing existing notes");
        const int beforeCancel = track->getNumNotes();
        ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Replace);
        ai.setSequencePause();
        R.check(!ai.isCountingIn() && track->getNumNotes() == beforeCancel, "Count-in: cancelling REC before the downbeat preserves existing notes");
        countConnection.disconnect(); clickConnection.disconnect();

        mt::getSettings().metronome.countIn = 0;
        timing = {}; timing.enabled = false; ai.setTimingCorrect(timing);
        ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub);
        sequence->onTick(13);
        mt::MTMidiMessage incoming; incoming.sourceName = "Timing test";
        incoming.message = juce::MidiMessage::noteOn(2, 39, (juce::uint8)88);
        ai.getManager()->onMidiMessage(incoming);
        sequence->onTick(397); // held across a complete loop
        incoming.message = juce::MidiMessage::noteOff(2, 39);
        ai.getManager()->onMidiMessage(incoming);
        ai.setSequencePause();
        const auto held = track->getNotesAtPos(13);
        R.check(held.size() == 1 && held[0].number == 39 && held[0].length == 384, "Recording: external MIDI is captured, bypass preserves timing and loop-long holds retain duration", "notes=" + juce::String(track->getNumNotes()) + " at13=" + juce::String((int)held.size()) + " elapsed=" + juce::String((juce::int64)sequence->getElapsedTicks()) + " input=" + track->getMidiPort() + "/" + juce::String(track->getMidiChannel()));
        ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub);
        sequence->onTick(5); incoming.message = juce::MidiMessage::noteOn(2, 40, (juce::uint8)80); ai.getManager()->onMidiMessage(incoming);
        ai.setSelectedTrack(otherIndex);
        sequence->onTick(25); incoming.message = juce::MidiMessage::noteOff(2, 40); ai.getManager()->onMidiMessage(incoming);
        ai.setSelectedTrack(trackIndex); ai.setSequencePause();
        auto moved = track->getNotesAtPos(5);
        R.check(moved.size() == 1 && moved[0].number == 40 && moved[0].length == 20,
            "Recording: changing selected track while an external key is held preserves its recording destination and release");
        ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub);
        sequence->onTick(2); ai.addLiveMidiNote(ai.getSelected(), 41, 99); sequence->onTick(20); ai.setSequencePause();
        auto stopped = track->getNotesAtPos(2);
        R.check(stopped.size() == 1 && stopped[0].number == 41 && stopped[0].length == 18, "Recording: STOP commits a still-held pad with the elapsed duration");


        capture.received.clear();
        ai.setFixedPadVelocity(64);
        ai.pressPerformancePad(0, 60, 12); ai.releasePerformancePad(0);
        R.check(capture.received.size() == 2 && capture.received[0].getVelocity() == 64 && capture.received[1].isNoteOff(), "HALF LEVEL: live pads send velocity 64 and a matching release");
        capture.received.clear(); ai.setFixedPadVelocity(127);
        ai.pressPerformancePad(0, 60, 12); ai.releasePerformancePad(0);
        R.check(capture.received[0].getVelocity() == 127, "FULL LEVEL: live pads send maximum velocity");
        ai.setFixedPadVelocity(0); ai.setSixteenLevels(true); capture.received.clear();
        ai.pressPerformancePad(0, 48, 20); ai.releasePerformancePad(0);
        ai.pressPerformancePad(15, 52, 20); ai.releasePerformancePad(15);
        R.check(capture.received.size() == 4 && capture.received[0].getNoteNumber() == 60 && capture.received[2].getNoteNumber() == 60
                && capture.received[0].getVelocity() == 8 && capture.received[2].getVelocity() == 127,
            "16 LEVELS: all pads play the reference note at sixteen increasing velocities");
        ai.setSixteenLevels(false); timing = {}; ai.setTimingCorrect(timing);
        ai.setSequencePlay(sequenceIndex, true, true);
        ai.setRepeatLatch(true); capture.received.clear();
        ai.pressPerformancePad(0, 42, 95); ai.releasePerformancePad(0);
        sequence->onTick(24); pump(60);
        juce::Timer::callPendingTimersSynchronously();
        int repeated = 0;
        for(const auto& message : capture.received) if(message.isNoteOn() && message.getNoteNumber() == 42) ++repeated;
        R.check(repeated == 2, "NOTE REPEAT/LATCH: a released pad repeats on the next clock division", "hits=" + juce::String(repeated));
        ai.setRepeatLatch(false); capture.received.clear(); sequence->onTick(48); pump(60);
        R.check(capture.received.isEmpty(), "LATCH: turning latch off releases pads that are no longer held");
        ai.pressPerformancePad(1, 44, 95); ai.setSequencePause(); capture.received.clear(); pump(15);
        R.check(capture.received.isEmpty(), "STOP: releases latched/repeated notes and prevents later timer hits");
        ai.setNoteRepeat(false); ai.setSixteenLevels(false); ai.setFixedPadVelocity(0);
        sequence->setLength(1); sequence->setLooping(false); track->deleteAllNotes();
        R.check(track->getLengthInTicks() == 384, "Timing: shortening a sequence updates tracks that follow its length");
        track->addOrModifyNote(45, 0, 12, 100); capture.received.clear();
        ai.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub);
        sequence->onTick(400); pump(60);
        int starts = 0; for(const auto& message : capture.received) if(message.isNoteOn() && message.getNoteNumber() == 45) ++starts;
        R.check(!ai.isSequencePlaying() && !ai.isSequenceRecording() && starts == 1 && sequence->getElapsedTicks() == 384,
            "Recording: a late tick batch stops a non-looping take at its end without replaying tick zero",
            "playing=" + juce::String((int)ai.isSequencePlaying()) + " recording=" + juce::String((int)ai.isSequenceRecording()) + " starts=" + juce::String(starts) + " elapsed=" + juce::String((juce::int64)sequence->getElapsedTicks()));
        ai.setSequencePlay(sequenceIndex, false, true); sequence->onTick(1);
        R.check(ai.isSequencePlaying() && sequence->getElapsedTicks() == 1, "Transport: PLAY restarts a completed non-looping sequence");
        ai.setSequencePause();
        timing = {}; timing.swing = 65; ai.setTimingCorrect(timing); ai.saveProject();
        mt::TrackAuditAccess::route(*track, nullptr);
        mt::getSettings().metronome.countIn = countBefore;
        ai.setExternalMidiClock(externalBefore);
        ai.closeProject();
        R.check(ai.loadProject(project) && ai.getManager()->getSequence(sequenceIndex)->getNumerator() == 4
                && mt::TimingCorrect::read(ai.getSequenceState(sequenceIndex)).swing == 65,
            "Timing: saved meter and swing survive a real project reload");
        ai.closeProject(); scratch.deleteRecursively();
    }
}
