// Exercise actual audio-region consumers and capture commits with deterministic input.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace mt
{
    struct AudioWorkflowAuditAccess
    {
        static AudioProgram& renderer(MTAudioProgram& program) { return *program.prog; }
        static juce::File pendingFile(MTAudioProgram& program) { return program.takeFile; }
    };
}
namespace
{
    void testAudioWorkflow(Results& r)
    {
        r.section("Audio tracks: routing, timeline and recording");
        auto& audio = mt::getAudioInterface();
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KickAudio-" + juce::Uuid().toString());
        auto project = scratch.getChildFile("Project"); project.createDirectory();
        r.check(audio.loadProject(project), "Audio: open isolated project");
        pump(1500); // Device manager creation is deliberately deferred at startup.
        auto& devices = audio.getAudioDeviceManager();
        const bool hadDevice = devices.getCurrentAudioDevice() != nullptr;
        devices.closeAudioDevice(); // Synthetic capture must not race a physical input callback.
        const auto file = project.getChildFile("ramp.wav");
        juce::WavAudioFormat format; auto stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 1000, 2, 24, {}, 0));
        juce::AudioBuffer<float> ramp(2, 1000);
        for(int i = 0; i < 1000; ++i) { ramp.setSample(0, i, .1f + i * .0005f); ramp.setSample(1, i, -.1f - i * .00025f); }
        const bool written = writer && writer->writeFromAudioSampleBuffer(ramp, 0, 1000); writer.reset();
        r.check(written && audio.addAsset(file.getFullPathName(), mt::AssetTypes::Audio) == mt::AssetAdded, "Audio: register complete synthetic stereo asset");
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        mt::ASound::Ptr source = new mt::ASound(file.getFullPathName(), formats, 1, -1);
        const auto& samples = source->getData();
        mt::AudioTimeline::Region region; region.source = source; region.tick = 10; region.length = 100; region.begin = 100; region.end = 900;
        mt::AudioTimeline timeline; auto clock = std::make_shared<mt::AudioPlaybackClock>(); timeline.attachClock(clock);
        auto render = [&](double tick, int count = 10, double rate = 1000, bool reverse = false, bool muted = false, float gain = 1, float pan = 0) {
            region.reverse = reverse; timeline.setRegions({region});
            clock->update(tick, 100, {0, 200}, false, muted, gain, pan, true);
            juce::AudioBuffer<float> out(2, count); out.clear(); timeline.render(out, rate); return out;
        };
        auto out = render(0);
        r.check(out.getMagnitude(0, out.getNumSamples()) == 0, "Audio: region is silent before its timeline start");
        out = render(10);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 100)) < 1.e-5f && std::abs(out.getSample(1, 0) - samples.getSample(1, 100)) < 1.e-5f, "Audio: region start uses source trim and retains stereo channels");
        out = render(20);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 200)) < 1.e-5f, "Audio: seeking into a region starts at its correct source offset");
        out = render(10, 10, 2000);
        r.check(std::abs(out.getSample(0, 1) - (samples.getSample(0, 100) + samples.getSample(0, 101)) * .5f) < 1.e-5f, "Audio: source/device sample rates interpolate independently");
        out = render(10, 10, 1000, true);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 899)) < 1.e-5f, "Audio: reverse reads from the exclusive trim end");
        out = render(10, 10, 1000, false, true);
        r.check(out.getMagnitude(0, out.getNumSamples()) == 0, "Audio: track mute suppresses timeline playback");
        out = render(10, 10, 1000, false, false, .5f, -1);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 100) * .5f) < 1.e-5f && out.getMagnitude(1, 0, out.getNumSamples()) == 0, "Audio: track level and pan are audible");
        region.gain = .25f; out = render(10);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 100) * .25f) < 1.e-5f, "Audio: per-region level reaches the renderer");
        region.gain = 1; region.pitch = 2; out = render(10);
        r.check(std::abs(out.getSample(0, 5) - samples.getSample(0, 110)) < 1.e-5f, "Audio: region tuning changes source advancement");
        region.pitch = 1; region.fadeIn = .5f; out = render(10, 10);
        r.check(out.getSample(0, 0) == 0 && out.getSample(0, 9) > out.getSample(0, 1), "Audio: fade-in progresses through the region");
        region.fadeIn = 0; timeline.setRegions({region}); clock->update(10, 100, {0, 200}, false, false, 1, 0, true);
        out.clear(); timeline.render(out, 1000, 2);
        r.check(std::abs(out.getSample(0, 5) - samples.getSample(0, 110)) < 1.e-5f, "Audio: program tuning combines with per-region tuning");
        region.fadeIn = 0; region.fadeOut = .5f; region.end = 1000; out = render(105, 10);
        r.check(out.getMagnitude(0, out.getNumSamples()) == 0, "Audio: exhausted source remains silent during a longer region");
        region.fadeOut = 0; region.tick = 0; region.length = 10; region.begin = 0; region.end = 100;
        timeline.setRegions({region, region}); clock->update(0, 100, {0, 10}, true, false, 1, 0, true);
        out.setSize(2, 150); out.clear(); timeline.render(out, 1000);
        r.check(std::abs(out.getSample(0, 0) - 2 * samples.getSample(0, 0)) < 1.e-5f && std::abs(out.getSample(0, 100) - out.getSample(0, 0)) < .001f, "Audio: overlapping regions sum and loop boundaries restart the timeline");
        auto curves = std::make_shared<mt::AudioAutomation>();
        curves->lanes[0] = {{0, 50}}; curves->lanes[1] = {{2, 100}}; curves->lanes[2] = {{6, 100}};
        timeline.setAutomation(curves); clock->update(0, 100, {0, 10}, true, false, 1, 0, true);
        out.setSize(2, 100); out.clear(); timeline.render(out, 1000);
        r.check(std::abs(out.getSample(0, 0) - samples.getSample(0, 0)) < 1.e-5f, "Audio automation: volume lane scales the summed regions");
        r.check(out.getSample(0, 21) == 0 && out.getSample(1, 21) != 0, "Audio automation: pan points take effect at their timeline ticks");
        r.check(out.getSample(0, 61) == 0 && out.getSample(1, 61) == 0, "Audio automation: mute points gate both channels");
        timeline.setAutomation(std::make_shared<const mt::AudioAutomation>());
        clock->stop(); out.clear(); timeline.render(out, 1000);
        r.check(out.getMagnitude(0, out.getNumSamples()) == 0, "Audio: stopped or deleted track clocks cannot keep sounding");
        float inputL[8]{.1f,.2f,.3f,.4f,.5f,.6f,.7f,.8f}, inputR[8]{-.1f,-.2f,-.3f,-.4f,-.5f,-.6f,-.7f,-.8f};
        r.check(!timeline.prepareCapture(0, 1000, clock) && !timeline.prepareCapture(INT_MAX, 1000, clock), "Audio: invalid and excessive takes are rejected before allocation");
        r.check(timeline.prepareCapture(12, 1000, clock), "Audio: preallocate bounded stereo take");
        timeline.record(inputL, inputR, 8, 1000); // Count-in: clock is inactive.
        clock->update(0, 100, {0, 10}, false, false, 1, 0, true);
        timeline.record(inputL, inputR, 8, 1000); timeline.record(inputL, inputR, 8, 1000);
        juce::AudioBuffer<float> recorded; int count = 0; double rate = 0;
        r.check(timeline.captureDone() && timeline.finishCapture(recorded, count, rate) && count == 12 && rate == 1000 && recorded.getSample(1, 11) == inputR[3], "Audio: count-in is excluded and capture stops exactly at capacity");
        timeline.prepareCapture(12, 1000, clock); timeline.record(inputL, inputR, 8, 2000);
        r.check(timeline.captureDone() && !timeline.finishCapture(recorded, count, rate), "Audio: device-rate changes fail a take instead of saving mis-timed audio");
        timeline.prepareCapture(12, 1000, clock); timeline.record(nullptr, inputR, 8, 1000);
        r.check(!timeline.finishCapture(recorded, count, rate), "Audio: missing input is reported as a failed take");

        const int sequenceIndex = audio.createSequence("Audio test"), trackIndex = audio.createTrack(sequenceIndex, "Recorded track");
        const int programIndex = audio.createProgram(mt::MTProgramType::Audio, "Audio fixture");
        audio.setProgramToTrack(programIndex, sequenceIndex, trackIndex); audio.setSelectedSequenceAndTrack(sequenceIndex, trackIndex);
        auto* program = dynamic_cast<mt::MTAudioProgram*>(audio.getProgram(programIndex));
        auto* track = audio.getManager()->getTrack(sequenceIndex, trackIndex);
        r.check(program && track && track->getAudioPlaybackClock(), "Audio: assigning an audio program attaches a track playback clock");
        if(program && track)
        {
            auto& backend = mt::AudioWorkflowAuditAccess::renderer(*program);
            pump(100); // Graph prepare and input routing finish before capture setup.
            auto& undo = audio.getUndoManager();
            auto captureClock = program->createPlaybackClock();
            r.check(!program->prepareRecording(captureClock, 0, 1, 100, false) && program->getRecordingError().contains("Arm"), "Audio: unarmed track refuses recording with a useful reason");
            program->armed = true;
            r.check(!program->prepareRecording(captureClock, 0, 1, 100, false) && program->getRecordingError().contains("input"), "Audio: unset input refuses recording");
            const auto& inputs = audio.getInputBusNames();
            r.check(!inputs.empty(), "Audio: selectable input buses exist");
            if(!inputs.empty())
            {
                program->input = juce::String(inputs.front());
                program->input = juce::String(inputs.front());
                r.check(backend.inputConnected.load(), "Audio: duplicate input notifications retain the selected graph connection");
                undo.beginNewTransaction("Audio take");
                const bool prepared = program->prepareRecording(captureClock, 100, 110, 100, false);
                r.check(prepared, "Audio: selected input and ARM prepare a real program take", program->getRecordingError());
                if(prepared)
                {
                    // Deterministic input through the same capture backend used by the graph.
                    captureClock->update(100, 100, {0, 200}, false, false, 1, 0, true);
                    backend.timeline.record(inputL, inputR, 8, backend.getSampleRate());
                    const bool saved = program->finishRecording(); undo.beginNewTransaction();
                    const auto clips = program->getAudioSampleList();
                    auto clip = clips.getChild(0); const auto take = juce::File(clip.getProperty(mt::IDs::source).toString());
                    r.check(saved && clips.getNumChildren() == 1 && take.existsAsFile() && (int)clip.getProperty(mt::IDs::length) >= 1, "Audio: stopping a take writes WAV and inserts a timeline region");
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(take));
                    juce::AudioBuffer<float> roundtrip(2, 8); roundtrip.clear(); if(reader) reader->read(&roundtrip, 0, 8, 0, true, true);
                    r.check(reader && reader->lengthInSamples == 8 && reader->sampleRate == backend.getSampleRate() && std::abs(roundtrip.getSample(1, 7) - inputR[7]) < 1.e-5f, "Audio: recorded WAV preserves frame count, sample rate and stereo input");
                    undo.undo(); pump(10);
                    r.check(clips.getNumChildren() == 0 && take.existsAsFile(), "Audio: undo removes the region while retaining the recoverable take file");
                    undo.redo(); pump(10);
                    r.check(clips.getNumChildren() == 1, "Audio: redo restores the recorded region");
                }
                const int before = program->getAudioSampleList().getNumChildren();
                captureClock->stop();
                program->prepareRecording(captureClock, 0, 10, 100, true);
                r.check(program->finishRecording() && program->getAudioSampleList().getNumChildren() == before, "Audio: cancelling count-in preserves all regions and creates no empty take");
                captureClock->update(0, 100, {0, 200}, false, false, 1, 0, true);
                program->prepareRecording(captureClock, 0, 10, 100, true);
                backend.timeline.record(inputL, inputR, 8, backend.getSampleRate());
                mt::AudioWorkflowAuditAccess::pendingFile(*program).createDirectory();
                r.check(!program->finishRecording() && program->getAudioSampleList().getNumChildren() == before && program->getRecordingError().contains("save") && mt::AudioWorkflowAuditAccess::pendingFile(*program).isDirectory(), "Audio: failed WAV replacement preserves existing regions and the conflicting directory");
                program->prepareRecording(captureClock, 0, 10, 100, true); backend.timeline.record(inputL, inputR, 8, backend.getSampleRate());
                mt::AudioWorkflowAuditAccess::pendingFile(*program).replaceWithText("unrelated file");
                r.check(!program->finishRecording() && program->getAudioSampleList().getNumChildren() == before && mt::AudioWorkflowAuditAccess::pendingFile(*program).loadFileAsString() == "unrelated file", "Audio: a recording commit never replaces an unexpected existing file");
                program->prepareRecording(captureClock, 0, 10, 100, true); program->input = "None";
                r.check(!program->finishRecording() && program->getAudioSampleList().getNumChildren() == before && !backend.inputConnected.load(), "Audio: changing or disabling the input invalidates a pending take safely");
                program->input = juce::String(inputs.front());

                captureClock->stop(); program->monitor = true;
                AudIO::Buffer<float> block(2, true, 2, true);
                backend.initialize((uint32_t)backend.getSampleRate(), block.getBlockSize());
                std::fill_n(block.getInputBuffer(0), block.getBlockSize(), .2f); std::fill_n(block.getInputBuffer(1), block.getBlockSize(), -.4f);
                juce::AudioBuffer<float> monitored(2, (int)block.getBlockSize()); monitored.clear();
                backend.processInput(block); backend.preProcess(monitored); backend.postProcess(monitored);
                const float normalMonitor = monitored.getSample(0, 0);
                r.check(normalMonitor > 0 && monitored.getSample(1, 0) < 0, "Audio MONITOR: selected stereo input reaches the program output");
                {
                    juce::ValueTree soloState(mt::IDs::PROGRAM); soloState.setProperty(mt::IDs::solo, true, nullptr);
                    mt::MTIProgram competitor(soloState);
                    r.check(!competitor.isMutedDueToSolo() && program->isMutedDueToSolo(), "Program solo: restored membership belongs to the actual program, independently of its index");
                    monitored.clear(); backend.processInput(block); backend.preProcess(monitored); backend.postProcess(monitored);
                    r.check(monitored.getMagnitude(0, monitored.getNumSamples()) == 0, "Program solo: another program's solo immediately silences sustained audio and monitoring");
                    program->solo = true; const int index = program->getIndex(); program->setIndex(100);
                    r.check(!program->isMutedDueToSolo() && !competitor.isMutedDueToSolo(), "Program solo: additive solos survive program reindexing");
                    program->setIndex(index); program->solo = false;
                }
                monitored.clear(); backend.processInput(block); backend.preProcess(monitored); backend.postProcess(monitored);
                r.check(!program->isMutedDueToSolo() && monitored.getSample(0, 0) > 0, "Program solo: deleting the last solo restores ongoing audio");
                program->getState().setProperty(mt::IDs::gain, .5f, nullptr);
                monitored.clear(); backend.processInput(block); backend.preProcess(monitored); backend.postProcess(monitored);
                program->getState().setProperty(mt::IDs::gain, 1.f, nullptr);
                r.check(std::abs(monitored.getSample(0, 0) / normalMonitor - .5f) < .001f, "Audio MONITOR: program gain applies once to monitored input");
                program->monitor = false;
                monitored.clear(); backend.processInput(block); backend.preProcess(monitored); backend.postProcess(monitored);
                r.check(monitored.getMagnitude(0, monitored.getNumSamples()) == 0, "Audio MONITOR: turning monitoring off silences the input without altering capture");

                const int replaceIndex = audio.createProgram(mt::MTProgramType::Audio, "Replace regions");
                auto* replacement = dynamic_cast<mt::MTAudioProgram*>(audio.getProgram(replaceIndex));
                replacement->input = juce::String(inputs.front()); replacement->armed = true; replacement->addSample(file.getFullPathName(), 0, 100, 0);
                auto replacementClock = replacement->createPlaybackClock(); replacementClock->update(25, 100, {0, 200}, false, false, 1, 0, true);
                auto& replaceBackend = mt::AudioWorkflowAuditAccess::renderer(*replacement);
                const int captureFrames = (int)(replaceBackend.getSampleRate() / 2);
                std::vector<float> recordedLeft((size_t)captureFrames, .3f), recordedRight((size_t)captureFrames, -.2f);
                undo.beginNewTransaction("Replace an audio range");
                const bool replacePrepared = replacement->prepareRecording(replacementClock, 25, 75, 100, true);
                replaceBackend.timeline.record(recordedLeft.data(), recordedRight.data(), captureFrames, replaceBackend.getSampleRate());
                const bool replaced = replacement->finishRecording(); undo.beginNewTransaction();
                auto oldSource = audio.getAssetManager().getAssetSound(file.getFullPathName());
                r.check(replacePrepared && replaced && replacement->getAudioSampleList().getNumChildren() == 3
                    && (int)replacement->getClipTree(0).getProperty(mt::IDs::length) == 25 && (int)replacement->getClipTree(75).getProperty(mt::IDs::start) == oldSource->getLength() * 3 / 4,
                    "Audio REC: replacement trims overlapping regions and preserves audio outside the recorded range");
                undo.undo(); r.check(replacement->getAudioSampleList().getNumChildren() == 1 && (int)replacement->getClipTree(0).getProperty(mt::IDs::length) == 100, "Audio REC: one undo restores the original region and removes the replacement");
                undo.redo(); r.check(replacement->getAudioSampleList().getNumChildren() == 3, "Audio REC: redo restores the take and both preserved edges");

                const int editsIndex = audio.createProgram(mt::MTProgramType::Audio, "Region edits");
                auto* edits = dynamic_cast<mt::MTAudioProgram*>(audio.getProgram(editsIndex));
                edits->addSample(file.getFullPathName(), 10, 100, 0); edits->setSelectAudioSample(10, true);
                edits->setSelectedAudioProperty(mt::IDs::reverse, true); edits->setSelectedAudioProperty("fadeIn", 25);
                auto clipboard = edits->copySelectedAudio();
                r.check(clipboard.getNumChildren() == 1 && (int)clipboard.getChild(0).getProperty(mt::IDs::startTime) == 0, "Audio: clipboard snapshots normalize selected region positions");
                const bool pasted = edits->pasteAudio(clipboard, 120, 300); undo.beginNewTransaction();
                r.check(pasted && edits->getAudioSampleList().getNumChildren() == 2 && (bool)edits->getClipTree(120).getProperty(mt::IDs::reverse), "Audio: paste preserves region processing and inserts at the requested cursor");
                undo.undo(); r.check(edits->getAudioSampleList().getNumChildren() == 1, "Audio: paste is one undoable edit");
                undo.redo(); r.check(edits->getAudioSampleList().getNumChildren() == 2, "Audio: redo restores pasted audio");
                r.check(!edits->pasteAudio(clipboard, -1, 300) && !edits->pasteAudio(clipboard, 250, 300) && edits->getAudioSampleList().getNumChildren() == 2, "Audio: invalid or out-of-range paste leaves all regions unchanged");
                edits->selectAllSamples(false); edits->setSelectAudioSample(120, true);
                const bool split = edits->splitSelectedAudio(170, 100); undo.beginNewTransaction();
                auto left = edits->getClipTree(120), right = edits->getClipTree(170);
                const auto cached = audio.getAssetManager().getAssetSound(file.getFullPathName());
                r.check(split && (int)left.getProperty(mt::IDs::length) == 50 && (int)right.getProperty(mt::IDs::length) == 50
                    && (int)left.getProperty(mt::IDs::start) == cached->getLength() / 2 && (int)right.getProperty(mt::IDs::end) == cached->getLength() / 2,
                    "Audio: splitting a reversed region preserves its sample boundaries");
                r.check((int)right.getProperty("fadeOffsetTicks") == 50 && (int)right.getProperty("fadeLengthTicks") == 100, "Audio: splitting retains the original fade timing");
                undo.undo(); r.check(edits->getAudioSampleList().getNumChildren() == 2 && edits->getClipTree(120).isValid(), "Audio: split undo restores the unsplit region");
                undo.redo(); edits->selectAllSamples(true);
                r.check(edits->getSelectedAudioSampleList().size() == 3 && !edits->getAudioSample(220), "Audio: select-all includes every region and region end bounds are exclusive");
                edits->addSample(file.getFullPathName(), 10, 75, 0);
                r.check((int)edits->getClipTree(10).getProperty(mt::IDs::length) == 75 && edits->getDrawingSource() == file.getFullPathName(), "Audio: reusing a drawing source updates its region length");
                const int children = edits->getState().getNumChildren();
                r.check(!edits->getParamTree("Volume").isValid() && edits->getState().getNumChildren() == children, "Audio automation: reading an absent lane does not mutate project state");
                edits->setValueForParam("Volume", 10, 45); edits->setValueForParam("PAN", 20, 100); edits->setValueForParam("Mute", 30, 100);
                for(int i = 1; i <= 4; ++i) edits->setValueForParam("Send" + juce::String(i), 10, i * 20);
                pump(20); auto& editBackend = mt::AudioWorkflowAuditAccess::renderer(*edits);
                auto liveCurves = editBackend.timeline.getAutomation();
                r.check(liveCurves->value(0, 9, 100) == 100 && liveCurves->value(0, 10, 100) == 45 && liveCurves->value(1, 20, 50) == 100, "Audio automation: saved points publish as step-held playback values");
                edits->applySendAutomation(10); bool sends = true;
                for(int i = 0; i < 4; ++i) sends &= std::abs(editBackend.getGainForSends(i) - (i + 1) * .2f) < .001f;
                r.check(sends, "Audio automation: all four send lanes reach actual bus input gains");
                edits->playbackStopped(); sends = true; auto defaults = edits->getSendGains();
                for(int i = 0; i < 4; ++i) sends &= std::abs(editBackend.getGainForSends(i) - defaults[(size_t)i]) < .001f;
                r.check(sends, "Audio automation: stopping restores manual send levels");
                edits->clearAutomation("Volume"); undo.beginNewTransaction(); pump(10);
                r.check(edits->getParamTree("Volume").getNumProperties() == 0 && editBackend.timeline.getAutomation()->value(0, 10, 100) == 100, "Audio automation: Clear Lane removes both saved and audible automation");
                undo.undo(); pump(10);
                r.check(editBackend.timeline.getAutomation()->value(0, 10, 100) == 45, "Audio automation: Clear Lane supports undo");
                undo.beginNewTransaction("Erase automation point"); edits->eraseAutomationPoint("PAN", 20); undo.beginNewTransaction(); pump(10);
                r.check(editBackend.timeline.getAutomation()->value(1, 20, 50) == 50, "Audio automation: erase removes the selected point");
                undo.undo(); pump(10);
                r.check(editBackend.timeline.getAutomation()->value(1, 20, 50) == 100, "Audio automation: erased points restore on undo");

                const bool wasExternal = mt::getSettings().midiControl.externalClock();
                const int wasCountIn = mt::getSettings().metronome.countIn();
                audio.setExternalMidiClock(true); mt::getSettings().metronome.countIn = 1;
                const int apiIndex = audio.createProgram(mt::MTProgramType::Audio, "Transport take");
                const int apiTrackIndex = audio.createTrack(sequenceIndex, "Audio API");
                audio.setProgramToTrack(apiIndex, sequenceIndex, apiTrackIndex); audio.setSelectedSequenceAndTrack(sequenceIndex, apiTrackIndex);
                auto* apiProgram = dynamic_cast<mt::MTAudioProgram*>(audio.getProgram(apiIndex));
                auto* apiTrack = audio.getManager()->getTrack(sequenceIndex, apiTrackIndex);
                auto* sequence = audio.getManager()->getSequence(sequenceIndex);
                apiTrack->setUsingSeqLength(true); sequence->setLength(1); audio.setTimeSignature(sequenceIndex, 4, 4);
                apiProgram->input = juce::String(inputs.front()); apiProgram->armed = true;
                const bool started = audio.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub);
                r.check(started && audio.isCountingIn() && !apiTrack->getAudioPlaybackClock()->active.load(), "Audio REC: panel recording API arms a take and waits through count-in");
                auto& apiBackend = mt::AudioWorkflowAuditAccess::renderer(*apiProgram);
                apiBackend.timeline.record(inputL, inputR, 8, apiBackend.getSampleRate());
                sequence->onTick(sequence->getTicksPerBar());
                r.check(apiTrack->getAudioPlaybackClock()->active.load(), "Audio REC: transport downbeat activates the audio track's clock");
                apiBackend.timeline.record(inputL, inputR, 8, apiBackend.getSampleRate());
                audio.setSequenceStop();
                sequence->onTick(sequence->getTicksPerBar() + 1);
                r.check(!apiTrack->getAudioPlaybackClock()->active.load(), "Audio STOP: a late queued tick cannot restart a stopped audio track");
                auto apiClip = apiProgram->getAudioSampleList().getChild(0);
                std::unique_ptr<juce::AudioFormatReader> apiReader(formats.createReaderFor(juce::File(apiClip.getProperty(mt::IDs::source).toString())));
                r.check(apiReader && apiReader->lengthInSamples == 8 && (int)apiClip.getProperty(mt::IDs::startTime) == 0 && !audio.isSequenceRecording(), "Audio REC: STOP commits only post-count-in frames through the normal recording API");
                mt::getSettings().metronome.countIn = 0;
                const int midiTrackIndex = audio.createTrack(sequenceIndex, "ARM and monitor");
                auto* midiTrack = audio.getManager()->getTrack(sequenceIndex, midiTrackIndex); midiTrack->setUsingSeqLength(true);
                audio.setSelectedSequenceAndTrack(sequenceIndex, midiTrackIndex);
                midiTrack->armed = false;
                r.check(!audio.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Replace) && audio.getLastRecordingError().contains("Arm"), "Track ARM: disarmed note track refuses REC without erasing notes");
                juce::ValueTree state(mt::IDs::PROGRAM); AuditProgram noteCapture(state, nullptr); mt::TrackAuditAccess::route(*midiTrack, &noteCapture);
                midiTrack->monitor = true; midiTrack->processIncomingMidiMessage(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
                noteCapture.received.clear(); midiTrack->monitor = false;
                const bool released = std::any_of(noteCapture.received.begin(), noteCapture.received.end(), [](const auto& m) { return m.isNoteOff() || m.isAllNotesOff(); });
                noteCapture.received.clear(); midiTrack->processIncomingMidiMessage(juce::MidiMessage::noteOn(1, 61, (juce::uint8)100));
                r.check(released && noteCapture.received.isEmpty(), "Track MONITOR: disabling releases held notes and suppresses new live notes");
                midiTrack->monitor = true; midiTrack->armed = true;
                r.check(audio.startSequenceRecording(mt::MTAudioInterface::RecordingMode::Overdub), "Track ARM: rearming permits the existing note-recording workflow");
                audio.setSequenceStop(); mt::TrackAuditAccess::route(*midiTrack, nullptr);
                audio.setExternalMidiClock(wasExternal); mt::getSettings().metronome.countIn = wasCountIn;
                program->getState().setProperty("audioMonitor", true, &undo); undo.beginNewTransaction();
                r.check(backend.monitor.load() && backend.inputConnected.load(), "Audio: saved monitor and input state reach the audio consumer");
                r.check(audio.saveProject(), "Audio: save a project with recorded regions and routing");
                auto reload = project;
                r.check(audio.loadProject(reload), "Audio: reload the recorded project");
                program = dynamic_cast<mt::MTAudioProgram*>(audio.getProgram(programIndex));
                r.check(program && program->armed.get() && program->monitor.get() && program->input.get() == juce::String(inputs.front()) && program->getAudioSampleList().getNumChildren() == before, "Audio: recorded regions, ARM, MONITOR and input survive real reload");
            }
        }
        audio.closeProject(); scratch.deleteRecursively(); if(hadDevice) devices.restartLastAudioDevice();
    }
}
