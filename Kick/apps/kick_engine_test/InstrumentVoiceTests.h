// Render production instrument voices without requiring physical audio/MIDI.
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "audio_program/synth/drumProgram.h"
#include "audio_program/synth/drum_voice.h"
namespace
{
    template<typename Synth> struct InstrumentHarness : Synth
    {
        using Synth::Synth;
        void prepare(double rate = 48000) { this->forEachVoice([rate](mt::ISynthVoice& v) { v.prepareToPlay(rate, AUDIO_BLOCK_SIZE); }); }
        int active() { int count = 0; this->forEachVoice([&](mt::ISynthVoice& v) { if(!v.isFree()) ++count; }); return count; }
        juce::AudioBuffer<float> render(int blocks = 1)
        {
            juce::AudioBuffer<float> result(2, AUDIO_BLOCK_SIZE);
            for(int i = 0; i < blocks; ++i)
            {
                result.clear(); this->forEachVoice([&](mt::ISynthVoice& v) { v.process(result); }); this->postProcess(result);
            }
            return result;
        }
        void drain() { this->noteOffAll(); for(int i = 0; i < 300 && active(); ++i) render(); }
        int modeCount(mt::SamplePlayType mode) { int count = 0; this->forEachVoice([&](mt::ISynthVoice& v) { if(!v.isFree() && static_cast<mt::DrumVoice&>(v).getPlayMode() == mode) ++count; }); return count; }
    };

    void testInstrumentVoices(Results& r)
    {
        r.section("Instrument voice completion");
        {
            AudIO::LFO reference, polled;
            reference.setSampleRate(48000); polled.setSampleRate(48000);
            reference.setFrequency(37, true); polled.setFrequency(37, true);
            reference.setEnabled(true); polled.setEnabled(true);
            double error = 0;
            for(int i = 0; i < AUDIO_BLOCK_SIZE * 8; ++i) {
                if(i % AUDIO_BLOCK_SIZE == 0) polled.setFrequency(37);
                error = juce::jmax(error, (double)std::abs(reference() - polled()));
            }
            r.check(error < 1.e-6, "LFO: polling an unchanged rate preserves phase across audio blocks");
            AudIO::LFO held; held.setSampleRate(48000); held.setType(AudIO::LFOType::SAMPLE_HOLD); held.setFrequency(100, true); held.setEnabled(true);
            for(int i = 0; i < 64; ++i) held();
            const float value = held(); bool stable = true;
            for(int i = 0; i < 200; ++i) stable &= held() == value;
            r.check(stable, "LFO: Sample Hold retains a value between hold boundaries instead of generating white noise");
        }
        auto& audio = mt::getAudioInterface();
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KickVoices-" + juce::Uuid().toString());
        auto project = scratch.getChildFile("Project"); project.createDirectory();
        r.check(audio.loadProject(project), "Voices: load isolated project");
        auto makeWave = [&](juce::String name, float level, int waveform) {
            const auto file = project.getChildFile(name + ".wav");
            juce::WavAudioFormat format; auto stream = file.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
            juce::AudioBuffer<float> data(2, 32768);
            for(int ch = 0; ch < 2; ++ch) for(int i = 0; i < data.getNumSamples(); ++i) data.setSample(ch, i, waveform == 2 ? level * std::sin(i * (juce::MathConstants<double>::twoPi / 48.0)) : waveform == 1 && (i & 1) ? -level : level);
            if(writer) writer->writeFromAudioSampleBuffer(data, 0, data.getNumSamples());
            writer.reset(); audio.addAsset(file.getFullPathName()); return file.getFullPathName();
        };
        const auto loud = makeWave("loud", 0.5f, false), quiet = makeWave("quiet", 0.25f, false), bright = makeWave("bright", 0.5f, true);
        const auto mid = makeWave("mid", 0.5f, 2);
        const int keysIndex = audio.createProgram(mt::MTProgramType::Keygroup, "Voice Keys");
        const int drumIndex = audio.createProgram(mt::MTProgramType::Drum, "Voice Drums");
        auto* keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(keysIndex));
        auto* drums = dynamic_cast<mt::MTDrumProgram*>(audio.getProgram(drumIndex));
        r.check(keys && drums, "Voices: create production programs");
        if(!keys || !drums) { audio.closeProject(); scratch.deleteRecursively(); return; }
        audio.addSampleToProgram(keysIndex, 60, loud); auto* zone = keys->getKeysClip(60);
        zone->minNote = 48; zone->maxNote = 84; zone->keyTracking = false;
        audio.addSampleToProgram(drumIndex, 36, loud, 0, 2);
        audio.addSampleToProgram(drumIndex, 36, quiet, 0, 3);
        auto* pad = drums->getDrumClip(36);
        {
            AudIO::Design design(2, 2);
            InstrumentHarness<mt::KeygroupProgram> synth(design, "Keys renderer", keys); synth.prepare();
            auto hit = [&](int note = 60, int velocity = 127, int blocks = 2) { synth.drain(); synth.noteOn(note, velocity); return synth.render(blocks); };
            auto baseline = hit(); const float normal = baseline.getMagnitude(0, baseline.getNumSamples());
            r.check(normal > 0.1f, "Voices: keygroup defaults render with modulation/filter bypassed");
            synth.drain(); synth.prepare(44100); synth.noteOn(60, 127); synth.render();
            r.check(synth.isPlayback(60) && std::abs(synth.isPlayback(60)->getPosition() - AUDIO_BLOCK_SIZE * (48000.0 / 44100.0)) < 0.1,
                "Voices: assigning a sample after prepare respects the device sample rate");
            synth.drain(); synth.prepare();
            keys->getState().setProperty(mt::IDs::gain, 0.5f, nullptr);
            auto half = hit();
            r.check(std::abs(half.getMagnitude(0, half.getNumSamples()) / normal - 0.5f) < 0.02f, "Voices: program gain is applied once");
            keys->getState().setProperty(mt::IDs::gain, 1.f, nullptr);
            keys->setEnvelopeEnabled(60, mt::AMPLITUDE, true); keys->setEnvelopeAttack(60, mt::AMPLITUDE, 127);
            auto attack = hit(60, 127, 1);
            r.check(attack.getMagnitude(0, attack.getNumSamples()) < normal * 0.1f, "Voices: saved amplitude attack changes rendered audio");
            keys->setEnvelopeAttack(60, mt::AMPLITUDE, 0); keys->setEnvelopeRelease(60, mt::AMPLITUDE, 24);
            hit(); synth.noteOff(60); auto released = synth.render();
            r.check(synth.active() > 0 && released.getMagnitude(0, released.getNumSamples()) > 0, "Voices: note release renders the configured envelope tail");
            synth.render(60);
            r.check(synth.active() == 0, "Voices: release tail finishes and returns its voice to the pool");
            keys->setEnvelopeEnabled(60, mt::AMPLITUDE, false);
            keys->setEnvelopeEnabled(60, mt::PITCH, true); keys->setEnvelopeDepth(60, mt::PITCH, 127);
            hit(60, 127, 1); const double pitched = synth.isPlayback(60) ? synth.isPlayback(60)->getPosition() : 0;
            keys->setEnvelopeEnabled(60, mt::PITCH, false); hit(60, 127, 1); const double plain = synth.isPlayback(60)->getPosition();
            r.check(std::abs(pitched - plain * 4) < 1, "Voices: pitch envelope uses a positive root-relative pitch ratio");
            zone->layers[0]->setSource(bright, 0); auto unfiltered = hit();
            keys->setFilterType(60, (int)mt::FilterType::LowPass4); keys->setFilterCutoff(60, 200); auto low = hit(60, 127, 8);
            r.check(low.getMagnitude(0, low.getNumSamples()) < unfiltered.getMagnitude(0, unfiltered.getNumSamples()) * 0.05f,
                "Voices: keygroup filter cutoff changes the actual sample output");
            zone->layers[0]->setSource(loud, 0); keys->setFilterType(60, (int)mt::FilterType::HighPass2); keys->setFilterCutoff(60, 1000);
            auto high = hit(60, 127, 8);
            r.check(high.getMagnitude(0, high.getNumSamples()) < normal * 0.05f, "Voices: filter type selects a high-pass response");
            zone->layers[0]->setSource(mid, 0); keys->setFilterType(60, (int)mt::FilterType::LowPass4); keys->setFilterCutoff(60, 100);
            auto dark = hit(60, 127, 8);
            keys->setEnvelopeDepth(60, mt::FILTER, 127); keys->setEnvelopeEnabled(60, mt::FILTER, true);
            auto opened = synth.render(8);
            r.check(opened.getMagnitude(0, AUDIO_BLOCK_SIZE) > 0.05f && opened.getMagnitude(0, AUDIO_BLOCK_SIZE) > dark.getMagnitude(0, AUDIO_BLOCK_SIZE) * 10,
                "Voices: the filter envelope modulates a held voice's actual cutoff");
            keys->setEnvelopeEnabled(60, mt::FILTER, false);
            zone->layers[0]->setSource(loud, 0);
            keys->setFilterType(60, -1); keys->setLFOFrequency(60, 50); keys->setLFOLevel(60, 127); keys->setLFOEnabled(60, true);
            hit(60, 127, 1); float minimum = 10, maximum = 0;
            for(int block = 0; block < 4; ++block) {
                auto lfo = synth.render();
                for(int i = 0; i < lfo.getNumSamples(); ++i) { const float v = std::abs(lfo.getSample(0, i)); minimum = juce::jmin(minimum, v); maximum = juce::jmax(maximum, v); }
            }
            r.check(maximum - minimum > 0.3f, "Voices: enabling the saved LFO produces audible amplitude modulation", "min=" + f3(minimum) + " max=" + f3(maximum) + " rate=" + f3(keys->getLFOFrequency(60)));
            keys->setLFOLevel(60, 0); keys->setLFOPitch(60, 127); hit(60, 127, 1);
            r.check(synth.isPlayback(60) && synth.isPlayback(60)->getPosition() > plain * 1.01 && synth.isPlayback(60)->getPosition() < plain * 1.2,
                "Voices: LFO pitch modulates playback without zero/negative pitch ratios");
            keys->setLFOPitch(60, 0); keys->setLFOPan(60, 1.0); auto panned = hit(60, 127, 1);
            r.check(rms(panned.getReadPointer(0), AUDIO_BLOCK_SIZE) < rms(panned.getReadPointer(1), AUDIO_BLOCK_SIZE) * 0.9f,
                "Voices: LFO pan modulates the two audio channels independently", "L=" + f3(rms(panned.getReadPointer(0), AUDIO_BLOCK_SIZE)) + " R=" + f3(rms(panned.getReadPointer(1), AUDIO_BLOCK_SIZE)));
            keys->setLFOPan(60, 0); keys->setLFOFilter(60, 127); zone->layers[0]->setSource(mid, 0);
            keys->setFilterType(60, (int)mt::FilterType::LowPass4); keys->setFilterCutoff(60, 600);
            auto opening = hit(60, 127, 1); auto closing = synth.render();
            r.check(rms(closing.getReadPointer(0), AUDIO_BLOCK_SIZE) < rms(opening.getReadPointer(0), AUDIO_BLOCK_SIZE) * 0.8f,
                "Voices: LFO filter destination changes the rendered cutoff over its cycle");
            keys->setLFOFilter(60, 0); keys->setFilterType(60, -1); zone->layers[0]->setSource(loud, 0);
            keys->setLFOEnabled(60, false); auto noLfo = hit(60, 127, 4);
            r.check(noLfo.getMagnitude(0, noLfo.getNumSamples()) > normal * 0.9f, "Voices: LFO bypass restores unmodulated audio");
            zone->polyPhony = 1; synth.drain(); synth.noteOn(60, 90); auto* first = synth.isPlayback(60); synth.render(); synth.noteOn(60, 127);
            r.check(synth.active() == 1 && synth.isPlayback(60) == first && synth.render().getMagnitude(0, AUDIO_BLOCK_SIZE) > 0.01f,
                "Voices: a full per-zone pool retriggers instead of silently dropping the hit");
            synth.drain(); zone->polyPhony = 16; keys->getState().setProperty(mt::IDs::poly, 2, nullptr);
            synth.noteOn(60, 127); synth.noteOn(61, 127); synth.noteOn(62, 127);
            r.check(synth.active() == 2 && !synth.isPlayback(60) && synth.isPlayback(61) && synth.isPlayback(62), "Voices: program polyphony steals the oldest held note deterministically");
            synth.drain(); keys->getState().setProperty(mt::IDs::poly, 16, nullptr);
            audio.addSampleToProgram(keysIndex, 61, quiet); auto* overlap = keys->getKeysClip(61); overlap->minNote = 60; overlap->maxNote = 60;
            zone->samplePlay = mt::SamplePlayType::NOTE_ON; overlap->samplePlay = mt::SamplePlayType::ONE_SHOT;
            synth.processNoteMessage(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127)); synth.render(2);
            r.check(synth.active() == 2, "Voices: overlapping keygroup zones layer rather than discarding later zones");
            synth.processNoteMessage(juce::MidiMessage::noteOff(1, 60)); auto afterRelease = synth.render(2);
            r.check(synth.active() == 1 && synth.modeCount(mt::SamplePlayType::ONE_SHOT) == 1 && afterRelease.getMagnitude(0, AUDIO_BLOCK_SIZE) > 0.01f,
                "Voices: overlapping zones retain their independent One Shot and Note On release behavior");
            synth.drain(); overlap->samplePlay = mt::SamplePlayType::NOTE_OFF;
            synth.processNoteMessage(juce::MidiMessage::noteOn(1, 60, (juce::uint8)105));
            r.check(synth.active() == 1, "Voices: a release-triggered zone waits while another zone sounds");
            synth.processNoteMessage(juce::MidiMessage::noteOff(1, 60)); synth.render();
            r.check(synth.active() == 1 && synth.modeCount(mt::SamplePlayType::NOTE_OFF) == 1, "Voices: zero-velocity release starts only its armed keygroup zone");
            synth.processNoteMessage(juce::MidiMessage::noteOff(1, 60));
            r.check(synth.active() == 1, "Voices: a duplicate release does not retrigger a layered zone");
            synth.drain(); synth.processNoteMessage(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100)); synth.noteOffAll(); synth.render();
            synth.processNoteMessage(juce::MidiMessage::noteOff(1, 60));
            r.check(synth.active() == 0, "Voices: All Notes Off cancels pending layered release triggers");
            synth.drain(); overlap->muteGroup = 1; zone->muteGroup = 1; overlap->samplePlay = mt::SamplePlayType::NOTE_ON;
            synth.noteOn(60, 127); synth.render();
            r.check(synth.active() == 1, "Voices: keygroup mute groups choke the preceding mapped voice");
            zone->muteGroup = 0; overlap->muteGroup = 0;
        }
        {
            AudIO::Design design(2, 2); InstrumentHarness<mt::DrumProgram> synth(design, "Drum renderer", drums); synth.prepare();
            auto hit = [&](int velocity) { synth.drain(); synth.noteOn(36, velocity); return synth.isPlayback(36); };
            pad->layerPlay = mt::LayerPlayType::CYCLE;
            auto* first = hit(127); auto* a = first ? first->getSlice() : nullptr;
            auto* second = hit(127); auto* b = second ? second->getSlice() : nullptr;
            auto* third = hit(127); auto* c = third ? third->getSlice() : nullptr;
            r.check(a == pad->getSourceSlice(2) && b == pad->getSourceSlice(3) && c == a, "Layers: drum cycle wraps through sparse assigned layers");
            pad->layerPlay = mt::LayerPlayType::VELOCITY;
            drums->setLayerVelocityEnd(36, 2, 63); drums->setLayerVelocityStart(36, 3, 64);
            auto* lower = hit(63); a = lower ? lower->getSlice() : nullptr;
            auto* upper = hit(64); b = upper ? upper->getSlice() : nullptr;
            auto* full = hit(127); c = full ? full->getSlice() : nullptr;
            r.check(a == pad->getSourceSlice(2) && b == pad->getSourceSlice(3) && c == b, "Layers: velocity boundaries and velocity 127 select the intended layer");
            drums->setLayerVelocityStart(36, 2, 20); hit(10);
            r.check(synth.active() == 0, "Layers: a velocity-zone gap remains silent instead of falling back to layer zero");
            drums->setLayerVelocityStart(36, 2, 0); pad->layerPlay = mt::LayerPlayType::RANDOM;
            bool valid = true; for(int i = 0; i < 40; ++i) { auto* voice = hit(127); valid &= voice && (voice->getSlice() == pad->getSourceSlice(2) || voice->getSlice() == pad->getSourceSlice(3)); }
            r.check(valid, "Layers: random selection never chooses an unassigned sparse slot");
            pad->layerPlay = mt::LayerPlayType::CYCLE; auto* voice = hit(127); auto baseline = synth.render(3);
            drums->setEnvelopeEnabled(36, mt::AMPLITUDE, true); drums->setEnvelopeAttack(36, mt::AMPLITUDE, 127);
            hit(127); auto attacked = synth.render();
            r.check(voice && attacked.getMagnitude(0, AUDIO_BLOCK_SIZE) < baseline.getMagnitude(0, AUDIO_BLOCK_SIZE) * 0.1f,
                "Voices: drum envelope controls use the same working parameter tree");
            drums->setEnvelopeEnabled(36, mt::AMPLITUDE, false); drums->setFilterType(36, (int)mt::FilterType::HighPass2); drums->setFilterCutoff(36, 1000);
            hit(127); auto filtered = synth.render(8);
            r.check(filtered.getMagnitude(0, AUDIO_BLOCK_SIZE) < baseline.getMagnitude(0, AUDIO_BLOCK_SIZE) * 0.05f,
                "Voices: dedicated drum filter parameters reach the voice DSP");
        }
        drums->setLayerVelocityStart(36, 2, 110);
        r.check(drums->getLayerVelocityStart(36, 2) == 110 && drums->getLayerVelocityEnd(36, 2) == 110, "Layers: moving low above high keeps an ordered zone");
        drums->setLayerVelocityEnd(36, 2, -10);
        r.check(drums->getLayerVelocityStart(36, 2) == 0 && drums->getLayerVelocityEnd(36, 2) == 0, "Layers: range edits clamp and keep both endpoints legal");
        auto* undo = drums->getUndoManager(); undo->beginNewTransaction("Layer range"); drums->setLayerVelocityEnd(36, 2, 127); undo->undo();
        r.check(drums->getLayerVelocityEnd(36, 2) == 0, "Layers: velocity-zone edits participate in project undo");
        keys->setFilterType(60, (int)mt::FilterType::LowPass4); keys->setFilterCutoff(60, 731); keys->setEnvelopeEnabled(60, mt::PITCH, true);
        keys->setEnvelopeRelease(60, mt::PITCH, 46); keys->setLFOEnabled(60, true); keys->setLFOFrequency(60, 7.2);
        r.check(audio.saveProject() && audio.loadProject(project), "Voices: save and reload filter, envelope, LFO and layer state");
        keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(keysIndex)); drums = dynamic_cast<mt::MTDrumProgram*>(audio.getProgram(drumIndex));
        r.check(keys && keys->getFilterType(60) == (int)mt::FilterType::LowPass4 && keys->getFilterCutoff(60) == 731 && keys->getEnvelopeEnabled(60, mt::PITCH)
            && keys->getEnvelopeRelease(60, mt::PITCH) == 46 && keys->getLFOEnabled(60) && std::abs(keys->getLFOFrequency(60) - 7.2) < 0.01,
            "Voices: processing settings survive real project reload");
        r.check(drums && drums->getLayerVelocityStart(36, 3) == 64 && drums->getLayerVelocityEnd(36, 2) == 0, "Layers: saved velocity ranges survive real project reload");
        audio.closeProject(); scratch.deleteRecursively();
    }
}
