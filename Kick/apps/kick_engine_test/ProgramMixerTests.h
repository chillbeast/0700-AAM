// Production-path regressions for program controls and mixer isolation.
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "audio_program/synth/keygroupProgram.h"
#include "../../../AudIO/AudIO/Core/Chain.h"

namespace
{
    void testProgramMixer(Results& r)
    {
        r.section("Program and mixer gap filling");
        {
            struct ReleaseProgram : mt::MTIProgram {
                explicit ReleaseProgram(juce::ValueTree& state) : MTIProgram(state) {}
                mt::SamplePlayType getNoteSamplePlay(int) override { return mt::SamplePlayType::NOTE_OFF; }
                int hits{0}, velocity{0};
                void noteOn(int, int v) override { ++hits; velocity = v; }
            };
            juce::ValueTree state(mt::IDs::PROGRAM); ReleaseProgram program(state);
            juce::Array<juce::MidiMessage> messages;
            messages.add(juce::MidiMessage::noteOn(1, 60, (juce::uint8)103)); program.noteProcess(messages);
            r.check(program.hits == 0, "Sample Play: Note Off mode waits for pad release");
            messages.clear(); messages.add(juce::MidiMessage::noteOff(1, 60)); program.noteProcess(messages); program.noteProcess(messages);
            r.check(program.hits == 1 && program.velocity == 103, "Sample Play: zero-velocity releases use the original hit velocity exactly once");
            messages.clear(); messages.add(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100)); messages.add(juce::MidiMessage::allNotesOff(1)); messages.add(juce::MidiMessage::noteOff(1, 60));
            program.noteProcess(messages);
            r.check(program.hits == 1, "Sample Play: All Notes Off cancels pending release triggers");
        }
        {
            AudIO::Chain chain;
            chain.prepareToPlay(testRate, testBlock);
            AudIO::Buffer<float> buffer(2, true, 2, true);
            auto fill = [&](float left, float right) {
                std::fill_n(buffer.getInputBuffer(0), testBlock, left);
                std::fill_n(buffer.getInputBuffer(1), testBlock, right);
                chain.process(buffer);
            };
            fill(0.25f, -0.125f);
            r.check(std::abs(buffer.getOutputBuffer(0)[0] - 0.25f) < 0.001f, "Limiter: disabled by default and preserves quiet audio");
            chain.setParameter(AudIO::BusOrChain::kParamLimiter, 1.f);
            fill(4.f, -2.f);
            r.check(std::abs(buffer.getOutputBuffer(0)[0] - 0.98f) < 0.0001f && std::abs(buffer.getOutputBuffer(1)[0] + 0.49f) < 0.0001f,
                "Limiter: linked channels catch the first transient and preserve stereo balance");
            fill(0.25f, -0.125f);
            const float early = buffer.getOutputBuffer(0)[0];
            for(int i = 0; i < 500; ++i) fill(0.25f, -0.125f);
            r.check(early < 0.1f && buffer.getOutputBuffer(0)[0] > 0.249f, "Limiter: release recovers smoothly across block boundaries");
            chain.setParameter(AudIO::BusOrChain::kParamGain, 4.f);
            for(int i = 0; i < 20; ++i) fill(1.f, -0.5f);
            r.check(std::abs(buffer.getOutputBuffer(0)[0]) <= 0.98001f, "Limiter: ceiling is applied after bus gain");
            chain.setParameter(AudIO::BusOrChain::kParamBypass, 1.f);
            for(int i = 0; i < 20; ++i) fill(3.f, -1.5f);
            r.check(std::abs(buffer.getOutputBuffer(0)[0]) <= 0.98001f, "Limiter: bypassing bus inserts retains enabled output protection");
            chain.setParameter(AudIO::BusOrChain::kParamLimiter, 0.f);
            for(int i = 0; i < 500; ++i) fill(3.f, -1.5f);
            r.check(std::abs(buffer.getOutputBuffer(0)[0] - 3.f) < 0.001f, "Limiter: disabling restores the unprocessed level");
        }

        auto& audio = mt::getAudioInterface();
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KickProgramMixer-" + juce::Uuid().toString());
        auto project = scratch.getChildFile("Project"); project.createDirectory();
        r.check(audio.loadProject(project), "Programs: load isolated project");
        const int midiIndex = audio.createProgram(mt::MTProgramType::Midi, "CC fixture");
        const int keyIndex = audio.createProgram(mt::MTProgramType::Keygroup, "Keys fixture");
        auto* midi = dynamic_cast<mt::MTMidiProgram*>(audio.getProgram(midiIndex));
        auto* keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(keyIndex));
        r.check(midi && keys, "Programs: create MIDI and keygroup fixtures");
        if(!midi || !keys) { audio.closeProject(); scratch.deleteRecursively(); return; }
        midi->midiCh = 9;
        midi->getState().setProperty("CC74", 83, midi->getUndoManager());
        const auto cc = midi->controllerMessage("CC74");
        r.check(cc && cc->isController() && cc->getControllerNumber() == 74 && cc->getControllerValue() == 83 && cc->getChannel() == 9,
            "MIDI CC: canonical property uses the program channel and saved value");
        r.check(!midi->controllerMessage("CC128") && !midi->controllerMessage("CC-1") && !midi->controllerMessage("CC01") && !midi->controllerMessage("myCC7") && !midi->controllerMessage("CC"),
            "MIDI CC: reject malformed and out-of-range controller properties");
        midi->getState().setProperty("CC127", 999, nullptr);
        r.check(midi->controllerMessage("CC127")->getControllerValue() == 127, "MIDI CC: legacy values clamp to a legal MIDI data byte");

        auto& buses = audio.getBusManager();
        auto* out0 = buses.getOutput(0); auto* out1 = buses.getOutput(1); auto* out2 = buses.getOutput(2);
        auto* ret0 = buses.getReturns(0); auto* ret1 = buses.getReturns(1);
        auto raw = [&](mt::MTIBus* bus) { return bus ? AudIO::DawBusManager::getBus(buses.getBusId(bus->name)) : nullptr; };
        r.check(out0 && out1 && out2 && ret0 && ret1, "Mixer: output and return groups are available");
        if(out0 && out1 && out2 && ret0 && ret1)
        {
            out2->mute = true; out0->solo = true;
            r.check(raw(out0)->isSolo() && !raw(out0)->isEffectivelyMuted() && raw(out1)->isEffectivelyMuted() && !raw(ret1)->isEffectivelyMuted(),
                "Mixer: output solo isolates its group without muting returns upstream");
            out1->solo = true;
            r.check(raw(out0)->isSolo() && raw(out1)->isSolo() && !raw(out1)->isEffectivelyMuted(), "Mixer: multiple output solos can coexist");
            out0->solo = false;
            r.check(raw(out0)->isEffectivelyMuted() && !raw(out1)->isEffectivelyMuted(), "Mixer: clearing one solo retains the remaining solo");
            out1->solo = false;
            r.check(!raw(out0)->isEffectivelyMuted() && raw(out2)->isEffectivelyMuted() && out2->mute.get(), "Mixer: clearing the last solo restores explicit mutes");
            ret0->solo = true;
            r.check(raw(ret1)->isEffectivelyMuted() && !raw(out0)->isEffectivelyMuted(), "Mixer: return solo is independent of output solo");
            ret1->solo = true;
            r.check(!raw(ret1)->isEffectivelyMuted(), "Mixer: multiple return solos can coexist");
            out0->limiter = true;
            if(auto* undo = midi->getUndoManager())
            {
                undo->beginNewTransaction("limiter regression"); out0->limiter = false;
                undo->undo();
                r.check(out0->limiter.get(), "Mixer: limiter toggle participates in project undo");
            }
        }

        const auto wav = project.getChildFile("key-tone.wav");
        {
            juce::WavAudioFormat format;
            auto stream = wav.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), testRate, 2, 24, {}, 0));
            juce::AudioBuffer<float> tone(2, 8192);
            for(int channel = 0; channel < 2; ++channel)
                for(int i = 0; i < tone.getNumSamples(); ++i) tone.setSample(channel, i, 0.5f * std::sin(i * 0.05));
            r.check(writer && writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples()), "Keygroup: write synthetic sample fixture");
        }
        audio.addAsset(wav.getFullPathName());
        audio.addSampleToProgram(keyIndex, 60, wav.getFullPathName());
        auto* zone = keys->getKeysClip(60);
        r.check(zone != nullptr, "Keygroup: sample assignment creates a zone");
        if(zone)
        {
            zone->minNote = 48; zone->maxNote = 84;
            r.check(keys->getZoneForNote(48) == zone && keys->getZoneForNote(84) == zone && !keys->getZoneForNote(47) && !keys->getZoneForNote(85),
                "Keygroup: inclusive zone boundaries reject unmapped notes");
            AudIO::Design design(2, 2);
            mt::KeygroupProgram synth(design, "Key render fixture", keys);
            auto render = [&](int note) {
                juce::AudioBuffer<float> rendered(2, AUDIO_BLOCK_SIZE); rendered.clear();
                synth.noteOffAll();
                // Direct voice rendering must also drain stopped voices, as
                // the normal synth audio callback does between pad presses.
                for(int oldNote = 0; oldNote < 128; ++oldNote)
                    while(auto* stopped = synth.isPlayback(oldNote)) stopped->process(rendered);
                rendered.clear(); synth.noteOn(note, 127);
                auto* voice = synth.isPlayback(note);
                if(voice) { voice->prepareToPlay(testRate, AUDIO_BLOCK_SIZE); voice->process(rendered); }
                return std::make_pair(voice ? voice->getPosition() : 0.0, rendered.getMagnitude(0, rendered.getNumSamples()));
            };
            const auto root = render(60); const auto octave = render(72);
            r.check(root.second > 0.01f && octave.second > 0.01f && std::abs(octave.first - root.first * 2.0) < 0.01,
                "Keygroup: the real note-on path renders samples and tracks octave pitch", "root=" + f3(root.first) + "/" + f3(root.second) + " octave=" + f3(octave.first) + "/" + f3(octave.second));
            zone->keyTracking = false;
            const auto fixed = render(72);
            r.check(std::abs(fixed.first - root.first) < 0.01, "Keygroup: KEY TRACK off keeps sample pitch across mapped notes", "root=" + f3(root.first) + " fixed=" + f3(fixed.first));
            zone->semi = 12;
            const auto tuned = render(72);
            r.check(std::abs(tuned.first - root.first * 2.0) < 0.01, "Keygroup: zone tuning works with KEY TRACK off");
            const auto unmapped = render(100);
            r.check(unmapped.second == 0.0f, "Keygroup: unmapped notes do not borrow a sample");
            zone->samplePlay = mt::SamplePlayType::NOTE_ON;
            r.check(keys->getNoteSamplePlay(72) == mt::SamplePlayType::NOTE_ON, "Keygroup: mapped notes inherit their zone's note-release mode");
        }
        r.check(audio.saveProject() && audio.loadProject(project), "Programs: save and reload controller, keygroup and mixer settings");
        midi = dynamic_cast<mt::MTMidiProgram*>(audio.getProgram(midiIndex));
        keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(keyIndex));
        r.check(midi && midi->getMidiChannel() == 9 && (int)midi->getState().getProperty("CC74") == 83 && keys && keys->getKeysClip(60) && !keys->getKeysClip(60)->keyTracking.get(),
            "Programs: MIDI CC and KEY TRACK survive real project reload");
        r.check(buses.getOutput(0)->limiter.get() && buses.getReturns(0)->solo.get() && buses.getReturns(1)->solo.get() && raw(buses.getReturns(0))->isSolo(),
            "Mixer: saved limiter and additive solo membership restore into the audio layer");
        audio.closeProject(); scratch.deleteRecursively();
    }
}
