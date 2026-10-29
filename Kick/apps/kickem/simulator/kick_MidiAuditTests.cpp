// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_ui/kick_ui.h>
#include <audio_midi/audio_midi.h>
#include <kick_ui/audio_components/step_sequencer/kick_StepSequencerControl.h>
#include <program_edit_ui/program_edit_ui.h>
#include <program_edit_ui/sources/midi/kick_MidiProgramEdit.h>
#include <program_edit_ui/sources/keygroup/kick_KeyGroupMaster.h>
#include <kick_ui_channel_mixer/kick_ui_channel_mixer.h>
#include <program_edit_ui/sources/common/kick_FilterEnvEdit.h>
#include <program_edit_ui/sources/drum/kick_DrumProgramEnvelopes.h>
#include <program_edit_ui/sources/drum/kick_DrumProgramLayerMix.h>
#include <program_edit_ui/sources/keygroup/kick_PanVelocityKG.h>
#include <program_edit_ui/sources/audio/kick_AudioProgramEditGrid.h>
#include "kick_Simulator.h"
#include "kick_SimulatorInterfaceImpl.h"

namespace sim
{
    namespace
    {
        struct MidiFixture
        {
            struct TempFile
            {
                juce::File file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("kick-midi-audit-" + juce::Uuid().toString() + ".json");
                ~TempFile() { file.deleteFile(); }
            } temp;
            std::vector<int> values;
            MidiLearn learn {temp.file}; // destroyed before callback storage
            MidiFixture()
            {
                learn.addTarget({"pad", "Pad", MidiLearn::Kind::pad, nullptr, [this](int v) { values.push_back(v); }, {}});
                learn.addTarget({"button", "Button", MidiLearn::Kind::button, nullptr, [this](int v) { values.push_back(v); }, {}});
                learn.addTarget({"encoder", "Encoder", MidiLearn::Kind::encoder, nullptr, [this](int v) { values.push_back(v); }, {}});
            }
            bool send(const juce::MidiMessage& message) { return learn.handle("controller", "Controller", message); }
            static juce::MidiMessage on(int n = 36) { return juce::MidiMessage::noteOn(1, n, (juce::uint8)100); }
            static juce::MidiMessage off(int n = 36) { return juce::MidiMessage::noteOff(1, n); }
            static juce::MidiMessage cc(int n, int v) { return juce::MidiMessage::controllerEvent(1, n, v); }
        };
    }

    void SimulatorBoard::startMidiAuditTest()
    {
        auto failed = std::make_shared<int>(0);
        auto check = [failed](bool ok, juce::String text) {
            if(!ok) ++*failed;
            SimulatorInterfaceImpl::log("SELFTEST " + juce::String(ok ? "PASS " : "FAIL ") + text);
        };
        const auto mode = juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {});
        if(mode.startsWith("project-startup"))
        {
            const auto expected = juce::SystemStats::getEnvironmentVariable("KICK_EXPECT_PROJECT_POLICY", "Selector");
            auto attempts = std::make_shared<int>(0);
            if(mode == "project-startup-seed")
                selfTestSteps.push_back([expected, check] {
                    auto& a = mt::getAudioInterface(); auto& s = mt::getSettings();
                    s.projects.tempo = 137; s.projects.bars = 3; s.projects.numSeq = 2; s.projects.numTrackPerSeq = 2;
                    s.projects.numerator = "7"; s.projects.denominator = "8";
                    s.projects.isAutoSave = false; s.projects.saveAfter = "2 min";
                    s.projects.namingStyle = "CUSTOM"; s.projects.namePrefix = "Launch";
                    const auto root = getAppDirs(MTProjects); root.createDirectory(); root.getChildFile("Launch").createDirectory();
                    if(expected == "Last Used") {
                        const auto last = root.getChildFile("Last Project");
                        check(a.createProject(last) && a.saveProject(), "Project startup: seed real last-used project");
                        a.closeProject(); s.projects.currentProject = last.getChildFile("Last Project.kickPrj").getFullPathName();
                    }
                    else s.projects.currentProject = root.getChildFile("Unavailable.kickPrj").getFullPathName();
                    s.projects.onLaunch = expected == "Missing" ? "Last Used" : expected;
                    s.projects.isAutoLoad = false;
                    s.save(); check(s.wasLastSaveSuccessful(), "Project startup: persist launch and autosave preferences");
                });
            else
            {
                selfTestSteps.push_back([this, expected, attempts] {
                    const bool needsProject = expected == "New Project" || expected == "Last Used";
                    if((needsProject && (!mt::getAudioInterface().isProjectLoaded() || ui::RecentProject::isProjectLoading) && ++*attempts < 60)
                        || (!needsProject && ++*attempts < 12)) --selfTestIndex;
                });
                selfTestSteps.push_back([expected, check] {
                    auto& a = mt::getAudioInterface(); auto& s = mt::getSettings();
                    check(!a.getManager()->isAutoSaveEnabled() && a.getManager()->getAutoSaveInterval() == 120000,
                        "Project startup: restore autosave policy without opening Preferences");
                    const bool needsProject = expected == "New Project" || expected == "Last Used";
                    check(a.isProjectLoaded() == needsProject, "Project startup: apply saved " + expected + " policy");
                    if(needsProject && a.isProjectLoaded()) {
                        check(a.getNumSequences() == 2 && a.getNumTracks(0) == 2 && a.getSequenceBPM(0) == 137,
                            "Project startup: loaded project retains creation defaults");
                        if(expected == "New Project") check(getAppDirs(MTProjects).getChildFile("Launch").getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0,
                            "Project startup: new project avoids an existing name");
                    }
                    else check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::RecentProject,
                        "Project startup: selector remains available without a project");
                    check(!ui::RecentProject::isProjectLoading, "Project startup: loading state clears");
                    check(a.closeProject() && !a.isProjectLoaded(), "Project startup: close visible project without stale track controls");
                    s.projects.onLaunch = "Selector"; s.projects.isAutoLoad = false; s.save();
                });
            }
            selfTestSteps.push_back([failed] {
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(120); return;
        }
        if(mode == "audio-workflow")
        {
            struct Fixture {
                mt::MTAudioProgram* program{nullptr}; mt::MTAudioProgram* other{nullptr};
                int programIndex{-1}, otherIndex{-1}, sequence{-1}, track{-1}, noteTrack{-1};
                std::unique_ptr<kick_ui::AudioTrackDisplay> strip;
                std::unique_ptr<kick_ui::TrackMidiMixerDisplay> midi;
                std::unique_ptr<AppTrackDisplay> mainTrack;
                kick_ui::TrackSoloMuteData midiState;
                std::unique_ptr<kick_ui::AudioProgramGrid> grid;
                juce::File scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kick-audio-ui-" + juce::Uuid().toString());
                juce::Component* find(juce::Component& parent, const juce::String& id) {
                    for(auto* child : parent.getChildren()) { if(child->getComponentID() == id) return child; if(auto* found = find(*child, id)) return found; } return nullptr;
                }
                juce::Button* button(juce::Component& parent, const juce::String& id) { return dynamic_cast<juce::Button*>(find(parent, id)); }
                ui::KickNumber* number(const juce::String& id) { return dynamic_cast<ui::KickNumber*>(find(*grid, id)); }
                ui::KickListBox* list(juce::Component& parent, const juce::String& id) { return dynamic_cast<ui::KickListBox*>(find(parent, id)); }
                void seek(int tick) { dynamic_cast<kick_ui::AudioScaleComponent*>(find(*grid, "audio-scale"))->onMouseClick((float)kick_ui::AudioGridData::Instance().getXFromTime(tick)); }
            };
            auto f = std::make_shared<Fixture>();
            auto waitFor = [this](std::function<bool()> ready) { auto attempts = std::make_shared<int>(0); selfTestSteps.push_back([this, ready, attempts] { if(!ready() && ++*attempts < 40) --selfTestIndex; }); };
            selfTestSteps.push_back([f, check] {
                auto& a = mt::getAudioInterface(); auto project = f->scratch.getChildFile("Project"); project.createDirectory();
                check(a.loadProject(project), "Audio UI: open isolated project");
                f->sequence = a.createSequence("Audio UI"); f->track = a.createTrack(f->sequence, "Audio"); f->noteTrack = a.createTrack(f->sequence, "Notes");
                f->programIndex = a.createProgram(mt::MTProgramType::Audio, "Recorded audio"); f->otherIndex = a.createProgram(mt::MTProgramType::Audio, "Other audio");
                a.setProgramToTrack(f->programIndex, f->sequence, f->track); a.setSelectedSequenceAndTrack(f->sequence, f->track); a.setSelectedProgram(f->programIndex);
                a.setTrackLength(f->sequence, f->track, 1); a.setTrackLength(f->sequence, f->noteTrack, 1);
                a.getManager()->getSequence(f->sequence)->setQuantization(mt::MTTimeBase::_16);
                f->program = dynamic_cast<mt::MTAudioProgram*>(a.getProgram(f->programIndex)); f->other = dynamic_cast<mt::MTAudioProgram*>(a.getProgram(f->otherIndex));
                auto file = project.getChildFile("tone.wav"); juce::WavAudioFormat format; auto stream = file.createOutputStream();
                std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
                juce::AudioBuffer<float> tone(2, 48000); tone.clear();
                if(writer) writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples()); writer.reset();
                a.addAsset(file.getFullPathName(), mt::AssetTypes::Audio); f->program->addSample(file.getFullPathName(), 10, 100, 0); f->program->setSelectAudioSample(10, true);
                f->strip = std::make_unique<kick_ui::AudioTrackDisplay>(); f->strip->setSize(1024, 112); f->strip->setVisible(true); f->strip->setAudioNumber(0, f->programIndex);
                f->grid = std::make_unique<kick_ui::AudioProgramGrid>(f->program); f->grid->setSize(1024, 458); f->grid->getHeaderComponent().setSize(600, 76); f->grid->setVisible(true);
                kick_ui::AudioGridData::Instance().refresh(true); f->grid->refresh();
                check(f->button(*f->strip, "audio-arm")->getWidth() > 0 && !f->program->armed.get() && !f->program->monitor.get(), "Audio UI: ARM is visible and audio monitoring defaults off");
                f->button(*f->strip, "audio-arm")->triggerClick();
            });
            waitFor([f] { return f->program->armed.get(); });
            selfTestSteps.push_back([f, check] {
                check(f->program->armed.get(), "Audio UI: ARM edits saved program state");
                mt::getAudioInterface().getUndoManager().beginNewTransaction(); mt::getAudioInterface().getUndoManager().undo();
            });
            waitFor([f] { return !f->program->armed.get(); });
            selfTestSteps.push_back([f, check] {
                check(!f->button(*f->strip, "audio-arm")->getToggleState(), "Audio UI: ARM undo refreshes its button");
                f->button(*f->strip, "audio-monitor")->triggerClick();
                f->list(*f->strip, "audio-input")->setSelected(juce::String(mt::getAudioInterface().getInputBusNames().front()), juce::sendNotification);
            });
            waitFor([f] { return f->program->monitor.get() && f->program->input.get() != "None"; });
            selfTestSteps.push_back([f, check] {
                check(f->program->monitor.get(), "Audio UI: MONITOR edits saved monitoring state");
                check(f->program->input.get() == juce::String(mt::getAudioInterface().getInputBusNames().front()), "Audio UI: input selector binds the selected audio program");
                f->strip->setAudioNumber(1, f->otherIndex); f->button(*f->strip, "audio-arm")->triggerClick();
            });
            waitFor([f] { return f->other->armed.get(); });
            selfTestSteps.push_back([f, check] {
                check(f->other->armed.get() && !f->program->armed.get(), "Audio UI: switching strips detaches edits from the previous program");
                f->number("audio-semi")->setValue(7, juce::sendNotification); f->number("audio-fine")->setValue(-12, juce::sendNotification);
                f->number("audio-fade-in")->setValue(20, juce::sendNotification); f->number("audio-fade-out")->setValue(35, juce::sendNotification);
                auto clip = f->program->getClipTree(10);
                check((int)clip.getProperty(mt::IDs::semi) == 7 && (int)clip.getProperty(mt::IDs::fine) == -12, "Audio UI: tuning controls update the selected region");
                check((int)clip.getProperty("fadeIn") == 20 && (int)clip.getProperty("fadeOut") == 35, "Audio UI: fade controls update independent saved ranges");
                f->button(f->grid->getHeaderComponent(), "audio-copy")->triggerClick(); f->seek(180);
            });
            selfTestSteps.push_back([f] { f->button(f->grid->getHeaderComponent(), "audio-paste")->triggerClick(); });
            waitFor([f] { return f->program->getAudioSampleList().getNumChildren() == 2; });
            selfTestSteps.push_back([f, check] {
                check(f->program->getAudioSampleList().getNumChildren() == 2, "Audio UI: Copy and Paste create a region at the edit cursor");
                auto& undo = mt::getAudioInterface().getUndoManager(); undo.beginNewTransaction(); undo.undo();
            });
            waitFor([f] { return f->program->getAudioSampleList().getNumChildren() == 1; });
            selfTestSteps.push_back([f, check] {
                check(f->program->getAudioSampleList().getNumChildren() == 1, "Audio UI: paste undo restores the original region count");
                f->program->selectAllSamples(false); f->program->setSelectAudioSample(10, true); f->seek(60);
                f->button(*f->grid, "audio-split")->triggerClick();
            });
            waitFor([f] { return f->program->getAudioSampleList().getNumChildren() == 2; });
            selfTestSteps.push_back([f, check] {
                check(f->program->getAudioSampleList().getNumChildren() == 2, "Audio UI: Split divides the selected region at the edit cursor");
                f->program->setValueForParam("Volume", 0, 30); f->list(*f->grid, "audio-automation")->setSelected(0, juce::sendNotification);
                f->button(*f->grid, "audio-clear-auto")->triggerClick();
            });
            waitFor([f] { return f->program->getParamTree("Volume").getNumProperties() == 0; });
            selfTestSteps.push_back([f, check] {
                check(f->program->getParamTree("Volume").getNumProperties() == 0, "Audio UI: Clear Auto removes the displayed lane");
                auto& undo = mt::getAudioInterface().getUndoManager(); undo.beginNewTransaction(); undo.undo();
            });
            waitFor([f] { return f->program->getParamTree("Volume").getNumProperties() == 1; });
            selfTestSteps.push_back([f, check] {
                auto& a = mt::getAudioInterface(); check(f->program->getParamTree("Volume").getNumProperties() == 1, "Audio UI: Clear Auto undo restores the lane");
                f->midi = std::make_unique<kick_ui::TrackMidiMixerDisplay>(); f->midi->setData(&f->midiState); f->midi->setSize(1024, 112); f->midi->setVisible(true); f->midi->setNumber(f->noteTrack);
                check(f->button(*f->midi, "track-arm")->getWidth() > 0 && a.getTrackData(f->sequence, f->noteTrack)->armed.get(), "Audio UI: note-track ARM remains enabled by default");
                f->button(*f->midi, "track-arm")->triggerClick(); f->button(*f->midi, "track-monitor")->triggerClick();
            });
            waitFor([f] { auto* track = mt::getAudioInterface().getTrackData(f->sequence, f->noteTrack); return !track->armed.get() && !track->monitor.get(); });
            selfTestSteps.push_back([f, check] {
                auto* track = mt::getAudioInterface().getTrackData(f->sequence, f->noteTrack);
                check(!track->armed.get() && !track->monitor.get(), "Audio UI: note-track ARM and MONITOR update their saved track state");
                f->mainTrack = std::make_unique<AppTrackDisplay>(f->sequence, f->noteTrack); f->mainTrack->setSize(1024, 112);
                check(!f->button(*f->mainTrack, "main-track-arm")->getToggleState() && !f->button(*f->mainTrack, "main-track-monitor")->getToggleState(), "Audio UI: main Track controls reflect saved note-track state");
                f->button(*f->mainTrack, "main-track-arm")->triggerClick(); f->button(*f->mainTrack, "main-track-monitor")->triggerClick();
            });
            waitFor([f] { auto* track = mt::getAudioInterface().getTrackData(f->sequence, f->noteTrack); return track->armed.get() && track->monitor.get(); });
            selfTestSteps.push_back([f, check] {
                auto& a = mt::getAudioInterface(); auto* track = a.getTrackData(f->sequence, f->noteTrack);
                check(track->armed.get() && track->monitor.get(), "Audio UI: main Track ARM and MONITOR edit real note-track state");
                a.getUndoManager().beginNewTransaction(); a.getUndoManager().undo();
            });
            waitFor([f] { return !mt::getAudioInterface().getTrackData(f->sequence, f->noteTrack)->monitor.get(); });
            selfTestSteps.push_back([f, check] {
                check(f->button(*f->mainTrack, "main-track-arm")->getToggleState() && !f->button(*f->mainTrack, "main-track-monitor")->getToggleState(), "Audio UI: main Track undo refreshes only the changed control");
                f->mainTrack->setTrack(f->sequence, f->track);
                check(!f->button(*f->mainTrack, "main-track-arm")->getToggleState() && f->button(*f->mainTrack, "main-track-monitor")->getToggleState(), "Audio UI: main Track controls switch to audio-program recording state");
                f->button(*f->mainTrack, "main-track-arm")->triggerClick(); f->button(*f->mainTrack, "main-track-monitor")->triggerClick();
            });
            waitFor([f] { return f->program->armed.get() && !f->program->monitor.get(); });
            selfTestSteps.push_back([f, check] {
                auto& a = mt::getAudioInterface(); auto* track = a.getTrackData(f->sequence, f->track);
                check(f->program->armed.get() && !f->program->monitor.get() && track->armed.get() && track->monitor.get(), "Audio UI: main Track audio controls edit the program rather than unused note flags");
                f->other->armed = false; f->other->monitor = true; a.setProgramToTrack(f->otherIndex, f->sequence, f->track);
                check(!f->button(*f->mainTrack, "main-track-arm")->getToggleState() && f->button(*f->mainTrack, "main-track-monitor")->getToggleState(), "Audio UI: main Track controls rebind when its program changes");
                f->program->armed = false; f->program->armed = true; f->program->monitor = true; f->program->monitor = false;
                check(!f->button(*f->mainTrack, "main-track-arm")->getToggleState() && f->button(*f->mainTrack, "main-track-monitor")->getToggleState(), "Audio UI: replaced program notifications leave current Track controls intact");
                f->grid.reset(); f->strip.reset(); f->midi.reset(); f->mainTrack.reset();
                kick_ui::sampleNeedRepaint(); kick_ui::AudioGridData::Instance().audioGridChanged();
                mt::getAudioInterface().closeProject(); f->scratch.deleteRecursively();
            });
            selfTestSteps.push_back([failed] { SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures"); juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed); juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
            startTimer(120); return;
        }
        if(mode == "instrument-voices")
        {
            struct Fixture {
                mt::MTKeygroupProgram* keys{nullptr}; mt::MTDrumProgram* drums{nullptr};
                std::vector<int> notes{60}, layerNotes{60}, pads{36};
                std::unique_ptr<kick_ui::KickFilterEnvelope> keyEnv;
                std::unique_ptr<kick_ui::DPEnvelopes> drumEnv;
                std::unique_ptr<kick_ui::KGLayerMix> keyLayers;
                std::unique_ptr<kick_ui::DPLayerMix> drumLayers;
                juce::File scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kick-voice-ui-" + juce::Uuid().toString());
                juce::Component* find(juce::Component& parent, const juce::String& id) {
                    for(auto* child : parent.getChildren()) { if(child->getComponentID() == id) return child; if(auto* match = find(*child, id)) return match; } return nullptr;
                }
                ui::KickRotary* rotary(juce::Component& parent, const juce::String& id) { return dynamic_cast<ui::KickRotary*>(find(parent, id)); }
                juce::Button* button(juce::Component& parent, const juce::String& id) { return dynamic_cast<juce::Button*>(find(parent, id)); }
                ui::KickListBox* list(juce::Component& parent, const juce::String& id) { return dynamic_cast<ui::KickListBox*>(find(parent, id)); }
            };
            auto f = std::make_shared<Fixture>();
            auto waitFor = [this](std::function<bool()> ready) { auto attempts = std::make_shared<int>(0); selfTestSteps.push_back([this, ready, attempts] { if(!ready() && ++*attempts < 40) --selfTestIndex; }); };
            for(int i = 0; i < 6; ++i) selfTestSteps.push_back([] {});
            selfTestSteps.push_back([f, check] {
                auto& audio = mt::getAudioInterface();
                auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
                auto project = f->scratch.getChildFile("Project");
                check(source.copyDirectoryTo(project) && audio.loadProject(project), "Voice UI: load isolated template");
                f->keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(audio.createProgram(mt::MTProgramType::Keygroup, "Envelope Keys")));
                f->drums = dynamic_cast<mt::MTDrumProgram*>(audio.getProgram(audio.createProgram(mt::MTProgramType::Drum, "Envelope Drums")));
                f->keys->getOrCreate(60); f->keys->getOrCreate(61); f->drums->getOrCreate(36);
                f->keyEnv = std::make_unique<kick_ui::KickFilterEnvelope>(f->notes, f->keys); f->keyEnv->setSize(1024, 458); f->keyEnv->setVisible(true);
                check(f->list(*f->keyEnv, "sample-filter-type") && f->keys->getFilterType(60) == -1 && !f->keys->getEnvelopeEnabled(60, mt::AMPLITUDE), "Voice UI: processing defaults remain bypassed");
                f->list(*f->keyEnv, "sample-filter-type")->setSelected(3, juce::sendNotification);
                f->rotary(*f->keyEnv, "sample-filter-cutoff")->setValue(731, juce::sendNotificationSync);
                f->rotary(*f->keyEnv, "sample-filter-resonance")->setValue(53, juce::sendNotificationSync);
                f->button(*f->keyEnv, "sample-env-amp-on")->triggerClick();
            });
            waitFor([f] { return f->keys->getEnvelopeEnabled(60, mt::AMPLITUDE); });
            selfTestSteps.push_back([f, check] {
                check(f->keys->getFilterType(60) == (int)mt::FilterType::LowPass4, "Voice UI: filter type updates the selected key zone");
                check(f->keys->getFilterCutoff(60) == 731 && f->keys->getFilterResonance(60) == 53, "Voice UI: cutoff and resonance edit saved DSP parameters");
                check(f->keys->getEnvelopeEnabled(60, mt::AMPLITUDE), "Voice UI: envelope On enables the selected row");
                f->rotary(*f->keyEnv, "sample-env-amp-attack")->setValue(64, juce::sendNotificationSync);
                check(f->keys->getEnvelopeAttack(60, mt::AMPLITUDE) == 64, "Voice UI: envelope attack reaches the program model");
                f->keys->getUndoManager()->beginNewTransaction("Release slider");
                f->rotary(*f->keyEnv, "sample-env-amp-release")->setValue(32, juce::sendNotificationSync);
                check(f->keys->getEnvelopeRelease(60, mt::AMPLITUDE) == 32, "Voice UI: Release is independent of modulation Depth");
                f->keys->getUndoManager()->undo();
            });
            waitFor([f] { return f->rotary(*f->keyEnv, "sample-env-amp-release")->getValue() == 0; });
            selfTestSteps.push_back([f, check] {
                check(f->keys->getEnvelopeRelease(60, mt::AMPLITUDE) == 0 && f->rotary(*f->keyEnv, "sample-env-amp-release")->getValue() == 0, "Voice UI: project undo refreshes the envelope controls");
                f->button(*f->keyEnv, "sample-lfo-on")->triggerClick();
                f->rotary(*f->keyEnv, "sample-lfo-rate")->setValue(7.2, juce::sendNotificationSync);
                f->rotary(*f->keyEnv, "sample-lfo-amp")->setValue(64, juce::sendNotificationSync);
            });
            waitFor([f] { return f->keys->getLFOEnabled(60); });
            selfTestSteps.push_back([f, check] {
                check(f->keys->getLFOEnabled(60) && std::abs(f->keys->getLFOFrequency(60) - 7.2) < 0.01 && f->keys->getLFOLevel(60) == 64, "Voice UI: LFO enable, rate and destination bind to the selected zone");
                f->list(*f->keyEnv, "sample-zone-select")->setSelected(juce::String("61"), juce::sendNotification);
                check(f->list(*f->keyEnv, "sample-filter-type")->getSelectedIndex() == 0 && !f->button(*f->keyEnv, "sample-env-amp-on")->getToggleState(), "Voice UI: selecting another zone replaces the controls' state");
                f->rotary(*f->keyEnv, "sample-filter-cutoff")->setValue(900, juce::sendNotificationSync);
                check(f->keys->getFilterCutoff(61) == 900 && f->keys->getFilterCutoff(60) == 731, "Voice UI: edits do not leak into the previous key zone");
                f->keyLayers = std::make_unique<kick_ui::KGLayerMix>(f->layerNotes, f->keys); f->keyLayers->setSize(1024, 458); f->keyLayers->setVisible(true);
                f->rotary(*f->keyLayers, "layer-velocity-low-2")->setValue(42, juce::sendNotificationSync);
                f->rotary(*f->keyLayers, "layer-velocity-high-2")->setValue(84, juce::sendNotificationSync);
                check(f->keys->getLayerVelocityStart(60, 2) == 42 && f->keys->getLayerVelocityEnd(60, 2) == 84, "Voice UI: keygroup velocity layer endpoints are editable");
                f->rotary(*f->keyLayers, "layer-velocity-low-2")->setValue(110, juce::sendNotificationSync);
                check(f->keys->getLayerVelocityStart(60, 2) == 110 && f->keys->getLayerVelocityEnd(60, 2) == 110, "Voice UI: crossed velocity endpoints stay ordered");
                f->drumLayers = std::make_unique<kick_ui::DPLayerMix>(f->pads, f->drums); f->drumLayers->setSize(1024, 458); f->drumLayers->setVisible(true);
                f->rotary(*f->drumLayers, "layer-velocity-high-3")->setValue(63, juce::sendNotificationSync);
                check(f->drums->getLayerVelocityEnd(36, 3) == 63, "Voice UI: drum layer endpoints use the same saved range policy");
                f->drumEnv = std::make_unique<kick_ui::DPEnvelopes>(f->pads, f->drums); f->drumEnv->setSize(1024, 458); f->drumEnv->setVisible(true);
                f->list(*f->drumEnv, "sample-filter-type")->setSelected(6, juce::sendNotification);
                f->button(*f->drumEnv, "sample-env-amp-on")->triggerClick();
            });
            waitFor([f] { return f->drums->getEnvelopeEnabled(36, mt::AMPLITUDE); });
            selfTestSteps.push_back([f, check] {
                check(f->drums->getFilterType(36) == (int)mt::FilterType::HighPass1 && f->drums->getEnvelopeEnabled(36, mt::AMPLITUDE), "Voice UI: the drum editor exposes working filter/envelope controls");
                f->drumEnv.reset(); f->drumLayers.reset(); f->keyLayers.reset(); f->keyEnv.reset();
                mt::getAudioInterface().closeProject(); f->scratch.deleteRecursively();
            });
            selfTestSteps.push_back([failed] {
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed); juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(120); return;
        }
        if(mode == "program-mixer")
        {
            struct Fixture
            {
                int midiA{-1}, midiB{-1};
                mt::MTKeygroupProgram* keys{nullptr};
                std::vector<int> selectedKeys{60};
                std::unique_ptr<kick_ui::MidiProgramEdit> editor;
                std::unique_ptr<kick_ui::KeyGroupMaster> keyEditor;
                std::unique_ptr<kick_ui::MasterRouteDisplay> master;
                std::unique_ptr<kick_ui::OutputTrackDisplay> output;
                std::unique_ptr<kick_ui::ReturnsMixerDisplay> returns;
                juce::File scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kick-program-ui-" + juce::Uuid().toString());
                ui::KickSlider* cc(int n) { return dynamic_cast<ui::KickSlider*>(editor->findChildWithID("midi-cc-" + juce::String(n))); }
                juce::Component* control(juce::Component& parent, juce::String id) {
                    for(auto* child : parent.getChildren())
                    {
                        if(child->getComponentID() == id) return child;
                        if(auto* nested = control(*child, id)) return nested;
                    }
                    return nullptr;
                }
                juce::Button* button(juce::Component& parent, juce::String id) { return dynamic_cast<juce::Button*>(control(parent, id)); }
            };
            auto fixture = std::make_shared<Fixture>();
            auto waitFor = [this](std::function<bool()> ready) {
                auto attempts = std::make_shared<int>(0);
                selfTestSteps.push_back([this, ready, attempts] { if(!ready() && ++*attempts < 40) --selfTestIndex; });
            };
            for(int i = 0; i < 6; ++i) selfTestSteps.push_back([] {});
            selfTestSteps.push_back([fixture, check] {
                auto& audio = mt::getAudioInterface();
                auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
                auto project = fixture->scratch.getChildFile("Project");
                check(source.copyDirectoryTo(project) && audio.loadProject(project), "Program UI: load isolated template");
                fixture->midiA = audio.createProgram(mt::MTProgramType::Midi, "CC A");
                fixture->midiB = audio.createProgram(mt::MTProgramType::Midi, "CC B");
                fixture->keys = dynamic_cast<mt::MTKeygroupProgram*>(audio.getProgram(audio.createProgram(mt::MTProgramType::Keygroup, "Keys")));
                fixture->keys->getOrCreate(60);
                auto& a = audio.getProgram(fixture->midiA)->getState();
                a.setProperty("CC0", 11, nullptr); a.setProperty("CC127", 92, nullptr);
                audio.getProgram(fixture->midiB)->getState().setProperty("CC0", 7, nullptr);
                fixture->editor = std::make_unique<kick_ui::MidiProgramEdit>();
                fixture->editor->setProgram(fixture->midiA);
                check(fixture->cc(0) && fixture->cc(127) && fixture->cc(0)->getValue() == 11 && fixture->cc(127)->getValue() == 92,
                    "Program UI: CC banks bind to the program including CC0 and CC127");
                fixture->cc(127)->setValue(93, juce::sendNotificationSync);
                check((int)a.getProperty("CC127") == 93, "Program UI: moving a CC slider changes the actual saved program");
                fixture->editor->setProgram(fixture->midiB);
                check(fixture->cc(0)->getValue() == 7, "Program UI: selecting another program displays its own CC values");
                fixture->cc(0)->setValue(21, juce::sendNotificationSync);
                check((int)a.getProperty("CC0") == 11 && (int)audio.getProgram(fixture->midiB)->getState().getProperty("CC0") == 21,
                    "Program UI: edits cannot leak into the previous program");
                fixture->editor->setProgram(-1);
                fixture->cc(0)->setValue(100, juce::sendNotificationSync);
                check(!fixture->editor->isEnabled() && (int)audio.getProgram(fixture->midiB)->getState().getProperty("CC0") == 21,
                    "Program UI: invalid selection detaches and disables the editor");
                fixture->editor->setProgram(fixture->midiA);
                a.setProperty("CC0", 37, nullptr);
                fixture->keyEditor = std::make_unique<kick_ui::KeyGroupMaster>(fixture->selectedKeys, fixture->keys);
                fixture->keyEditor->setSize(1024, 458);
                fixture->keyEditor->setVisible(true);
                auto* tracking = fixture->button(*fixture->keyEditor, "keygroup-key-track");
                if(tracking) tracking->triggerClick(); else check(false, "Program UI: missing KEY TRACK control");
            });
            waitFor([fixture] { return !fixture->keys->getKeysClip(60)->keyTracking.get() && fixture->cc(0)->getValue() == 37; });
            selfTestSteps.push_back([fixture, check] {
                check(fixture->cc(0)->getValue() == 37, "Program UI: external state/undo updates the attached slider");
                check(!fixture->keys->getKeysClip(60)->keyTracking.get(), "Program UI: KEY TRACK button changes the selected zone");
                fixture->keys->getUndoManager()->undo();
            });
            waitFor([fixture] { return fixture->button(*fixture->keyEditor, "keygroup-key-track")->getToggleState(); });
            selfTestSteps.push_back([fixture, check] {
                check(fixture->keys->getKeysClip(60)->keyTracking.get() && fixture->button(*fixture->keyEditor, "keygroup-key-track")->getToggleState(),
                    "Program UI: KEY TRACK follows project undo");
                auto* play = dynamic_cast<ui::KickToggleOptions*>(fixture->control(*fixture->keyEditor, "keygroup-sample-play"));
                play->setSelected(1, juce::sendNotification);
                check(fixture->keys->getKeysClip(60)->samplePlay.get() == mt::SamplePlayType::NOTE_ON, "Program UI: Note On label selects the held-note mode");
                play->setSelected(2, juce::sendNotification);
                check(fixture->keys->getKeysClip(60)->samplePlay.get() == mt::SamplePlayType::NOTE_OFF, "Program UI: Note Off label selects the release-trigger mode");
                fixture->master = std::make_unique<kick_ui::MasterRouteDisplay>(); fixture->master->setSize(1024, 112); fixture->master->setVisible(true);
                fixture->button(*fixture->master, "master-limiter")->triggerClick();
            });
            waitFor([] { return mt::getAudioInterface().getBusManager().getOutput(0)->limiter.get(); });
            selfTestSteps.push_back([fixture, check] {
                auto& buses = mt::getAudioInterface().getBusManager();
                check(buses.getOutput(0)->limiter.get(), "Mixer UI: master limiter button reaches the output bus");
                fixture->master->knobCommand(DCC::EN_KNOB_16, 1);
                check(buses.getOutput(0)->mute.get(), "Mixer UI: master mute Q-link changes audio state");
                fixture->output = std::make_unique<kick_ui::OutputTrackDisplay>(); fixture->output->setSize(1024, 112); fixture->output->setVisible(true); fixture->output->setOutputNumber(0);
                fixture->output->linkRotated(3, -1);
                check(buses.getOutput(0)->solo.get() && !buses.getOutput(0)->mute.get(), "Mixer UI: output solo Q-link changes audio state");
                fixture->returns = std::make_unique<kick_ui::ReturnsMixerDisplay>(); fixture->returns->setSize(1024, 112); fixture->returns->setVisible(true); fixture->returns->setBusNumber(0);
                fixture->returns->linkRotated(3, -1);
                check(buses.getReturns(0)->solo.get(), "Mixer UI: return solo Q-link changes audio state");
                fixture->returns->linkRotated(3, 1);
                check(!buses.getReturns(0)->solo.get() && buses.getReturns(0)->mute.get(), "Mixer UI: return mute Q-link clears solo and changes audio state");
                fixture->returns.reset(); fixture->output.reset(); fixture->master.reset(); fixture->keyEditor.reset(); fixture->editor.reset();
                mt::getAudioInterface().closeProject(); fixture->scratch.deleteRecursively();
            });
            selfTestSteps.push_back([failed] {
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(120); return;
        }
        if(mode == "timing-controls")
        {
            auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kick-timing-ui-" + juce::Uuid().toString());
            auto click = [this](int command) {
                auto range = ledMap.equal_range(command);
                if(range.first != range.second)
                {
                    auto* button = range.first->second;
                    button->press(true, button->getCode() != command); button->press(false);
                }
            };
            auto waitFor = [this](std::function<bool()> ready) {
                auto attempts = std::make_shared<int>(0);
                selfTestSteps.push_back([this, ready, attempts] {
                    // Navigation, modal callbacks and LED packets are queued.
                    // Wait for delivery, then retain the final state assertion.
                    if(!ready() && ++*attempts < 40) --selfTestIndex;
                });
            };
            auto waitForScreen = [waitFor](kick_ui::Screen wanted) {
                waitFor([wanted] { return kick_ui::KickHome::Instance().getCurrentScreen() == wanted; });
            };
            auto waitForLed = [this, waitFor](int command) {
                waitFor([this, command] {
                    auto* button = findButton(command);
                    return button && button->isLedOn();
                });
            };
            auto sequence = [] { auto& audio = mt::getAudioInterface(); return audio.getManager()->getSequence(); };
            auto track = [] { auto& audio = mt::getAudioInterface(); return audio.getManager()->getTrack(audio.getSelected()); };
            for(int i = 0; i < 4; ++i) selfTestSteps.push_back([] {});
            selfTestSteps.push_back([scratch, check, click, sequence, track] {
                auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
                auto project = scratch.getChildFile("Project");
                auto& audio = mt::getAudioInterface();
                check(source.copyDirectoryTo(project) && audio.loadProject(project), "Timing UI: load isolated template");
                if(!sequence()) audio.setSelectedSequence(audio.createSequence("Timing UI"));
                if(!track()) audio.setSelectedTrack(audio.createTrack(audio.getSelectedSequence(), "Pads"));
                const int program = audio.createProgram(mt::MTProgramType::Drum, "Timing UI");
                audio.setProgramToTrack(program, audio.getSelectedSequence(), audio.getSelectedTrack());
                sequence()->setLength(2); sequence()->setLooping(true); sequence()->setLoopPoints(0, 1);
                track()->setUsingSeqLength(true); track()->deleteAllNotes(); track()->addOrModifyNote(60, 0, 12, 100);
                audio.setExternalMidiClock(true);
                mt::getSettings().metronome.countIn = 1;
            });
            for(int i = 0; i < 3; ++i) selfTestSteps.push_back([] {});
            selfTestSteps.push_back([click] { click(DCC::EN_MAINMODE); });
            for(int i = 0; i < 2; ++i) selfTestSteps.push_back([] {});
            waitForScreen(kick_ui::Screen::HomeListView);
            auto backResult = std::make_shared<int>(0);
            selfTestSteps.push_back([click, backResult] {
                auto* contents = new juce::Component();
                mt::utils::showOptions("BACK cancellation", contents, [contents, backResult](int result) {
                    *backResult = result; delete contents;
                }, {0, 0, 400, 160});
                click(DCC::EN_BACK);
            });
            waitFor([backResult] { return *backResult == UI::CANCEL && juce::Component::getCurrentlyModalComponent() == nullptr; });
            selfTestSteps.push_back([check, backResult] {
                check(*backResult == UI::CANCEL && juce::Component::getCurrentlyModalComponent() == nullptr,
                    "Timing UI: BACK returns the standard Cancel result to a modal dialog");
            });
            selfTestSteps.push_back([check, click] {
                check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::HomeListView, "Timing UI: MAIN opens normally");
                click(DCC::EN_TC);
                check(juce::Component::getCurrentlyModalComponent() != nullptr, "Timing UI: TC opens the real correction dialog");
                if(auto* modal = juce::Component::getCurrentlyModalComponent()) modal->exitModalState(UI::APPLY);
            });
            selfTestSteps.push_back([check, click, track] {
                check(juce::Component::getCurrentlyModalComponent() == nullptr, "Timing UI: Apply closes the correction dialog safely");
                auto& audio = mt::getAudioInterface();
                const bool before = audio.getTimingCorrect().enabled;
                click(DCC::EN_ON_OFF);
                check(audio.getTimingCorrect().enabled != before, "Timing UI: ON/OFF changes recording correction");
                click(DCC::EN_FULL_LEVEL); click(DCC::EN_RECORD);
                check(audio.getFixedPadVelocity() == 127 && audio.isSequenceRecording() && audio.isCountingIn(), "Timing UI: FULL LEVEL and REC start a replace take with count-in");
                check(track()->getNumNotes() == 1, "Timing UI: count-in leaves the original notes intact");
                click(DCC::EN_STOP);
                check(!audio.isSequenceRecording() && !audio.isCountingIn() && track()->getNumNotes() == 1, "Timing UI: STOP cancels the armed take without erasing notes");
                click(DCC::EN_RECORD);
            });
            selfTestSteps.push_back([this, check, sequence, track] {
                sequence()->onTick(384);
                pads.getPad(0).hit(true, 20);
                sequence()->onTick(408);
                pads.getPad(0).hit(false);
                check(track()->getNumNotes() == 1 && track()->getNotesAtPos(0).size() == 1 && track()->getNotesAtPos(0)[0].velocity == 127,
                    "Timing UI: actual pad packets replace and record at full velocity after the downbeat");
            });
            waitForLed(DCC::EN_RECORD);
            selfTestSteps.push_back([this, check, click] {
                check(findButton(DCC::EN_RECORD)->isLedOn(), "Timing UI: REC LED follows the replace take");
                click(DCC::EN_STOP); click(DCC::EN_HALF_LEVEL); click(DCC::EN_OVERDUB);
            });
            selfTestSteps.push_back([this, check, click, sequence, track] {
                sequence()->onTick(384);
                pads.getPad(1).hit(true, 20); sequence()->onTick(408); pads.getPad(1).hit(false);
                check(track()->getNumNotes() == 2 && track()->getNotesAtPos(0).size() == 2, "Timing UI: OVERDUB preserves the earlier take");
                bool half = false;
                for(const auto& note : track()->getNotesAtPos(0)) if(note.number == 1) half = note.velocity == 64;
                check(half, "Timing UI: HALF LEVEL records velocity 64 through the pad route");
            });
            waitForLed(DCC::EN_OVERDUB);
            selfTestSteps.push_back([this, check, click] {
                check(mt::getAudioInterface().getRecordingMode() == mt::MTAudioInterface::RecordingMode::Overdub && findButton(DCC::EN_OVERDUB)->isLedOn(), "Timing UI: OVERDUB has its own recording LED");
                click(DCC::EN_STOP); click(DCC::EN_NOTE_REPEAT); click(DCC::EN_LATCH); click(DCC::EN_16LEVELS);
                auto& audio = mt::getAudioInterface();
                check(audio.isNoteRepeat() && audio.isRepeatLatched() && audio.isSixteenLevels(), "Timing UI: repeat/latch/16 Levels buttons change engine performance state");
                click(DCC::EN_MASTER);
            });
            waitForScreen(kick_ui::Screen::MenuChannelMixer);
            selfTestSteps.push_back([check, click] {
                check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuChannelMixer, "Timing UI: MASTER opens the mixer containing master controls");
                click(DCC::EN_LINK);
            });
            waitForScreen(kick_ui::Screen::MenuKnobEdit);
            selfTestSteps.push_back([check, click] {
                check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuKnobEdit, "Timing UI: LINK opens Q-Link assignments");
                click(DCC::EN_BACK);
            });
            waitForScreen(kick_ui::Screen::MenuChannelMixer);
            selfTestSteps.push_back([check, click] {
                check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuChannelMixer, "Timing UI: BACK returns from Q-Link assignments");
                click(DCC::EN_AUTOMATION);
            });
            waitForScreen(kick_ui::Screen::MenuProgramEdit);
            selfTestSteps.push_back([check, click, sequence, track] {
                check(kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuProgramEdit, "Timing UI: AUTOMATION opens Program Edit with availability feedback");
                auto& audio = mt::getAudioInterface(); audio.setNoteRepeat(false); audio.setSixteenLevels(false); audio.setFixedPadVelocity(0);
                track()->deleteAllNotes(); sequence()->setQuantization(mt::MTTimeBase::_16T);
                click(DCC::EN_STEP_SEQ);
            });
            selfTestSteps.push_back([] {});
            waitForScreen(kick_ui::Screen::HomeSequencerView);
            selfTestSteps.push_back([check, click] {
                check(kick_ui::GridData::Instance().getNumQlPerBar(0) == 24, "Timing UI: triplet division creates twenty-four steps per 4/4 bar");
                click(DCC::EN_BANK_B);
            });
            selfTestSteps.push_back([this, check, track, sequence] {
                pads.getPad(7).hit(true, 100); pads.getPad(7).hit(false);
                check(track()->getNotesAtPos(368).size() == 1, "Timing UI: bank B reaches the last step on a partial triplet page");
                const int before = track()->getNumNotes(); pads.getPad(8).hit(true, 100); pads.getPad(8).hit(false);
                check(track()->getNumNotes() == before, "Timing UI: unused pads on the partial page do not edit another step");
                mt::getAudioInterface().setTimeSignature(mt::getAudioInterface().getSelectedSequence(), 7, 8);
                std::function<void(juce::Component*)> setDivision = [&](juce::Component* component) {
                    if(auto* controls = dynamic_cast<kick_ui::StepSequencerTopControl*>(component))
                        controls->timeDivValueListBox.setSelected(kick_ui::GridData::Instance().getTimeBaseIndex(6), juce::sendNotification);
                    for(auto* child : component->getChildren()) setDivision(child);
                };
                setDivision(&kick_ui::KickHome::Instance());
            });
            selfTestSteps.push_back([this, check, track] {
                pads.getPad(5).hit(true, 100); pads.getPad(5).hit(false);
                const auto last = track()->getNotesAtPos(320);
                check(last.size() == 1 && last[0].length == 16, "Timing UI: the final partial triplet in 7/8 ends at the bar boundary");
                check(track()->getNotesAtPos(368).size() == 1, "Timing UI: editing the partial step preserves notes in the next bar");
                track()->addOrModifyNote(9, 320, 16, 100);
                pads.getPad(5).hit(true, 100); pads.getPad(5).hit(false);
                const auto remaining = track()->getNotesAtPos(320);
                check(remaining.size() == 1 && remaining[0].number == 9, "Timing UI: removing a step preserves other pitches at the same time");
            });
            selfTestSteps.push_back([scratch, failed] {
                auto& audio = mt::getAudioInterface(); audio.setNoteRepeat(false); audio.setSixteenLevels(false); audio.releasePerformancePads(); audio.closeProject();
                scratch.deleteRecursively();
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(120);
            return;
        }
        if(mode.startsWith("startup"))
        {
            // This runs during board construction, before queued screen creation.
            auto& home = kick_ui::KickHome::Instance();
            auto firstBarrier = std::make_shared<bool>(false);
            auto secondBarrier = std::make_shared<bool>(false);
            auto attempts = std::make_shared<std::array<int, 2>>();
            check(home.getScreen(kick_ui::Screen::HomeListView) == nullptr, "M9: exercise the window before lazy screens exist");
            if(mode == "startup-shutdown")
            {
                home.releaseScreens();
                selfTestSteps.push_back([this, check, firstBarrier, attempts] {
                    if(!*firstBarrier && ++(*attempts)[0] < 40) { --selfTestIndex; return; }
                    auto& root = kick_ui::KickHome::Instance();
                    check(root.getScreen(kick_ui::Screen::HomeListView) == nullptr, "M9: shutdown cancels deferred screen creation");
                    root.setSize(900, 550);
                    check(true, "M9: released screen root safely ignores resize");
                });
            }
            else
            {
                auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
                auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kick-startup", "", false).getChildFile("Template 01");
                check(source.copyDirectoryTo(scratch) && mt::getAudioInterface().loadProject(scratch), "M9: project can load before screen initialization");
                mainControl.getJog().turn(1);
                qlinks[0]->turn(1);
                if(auto* main = findButton(DCC::EN_MAINMODE)) { main->press(true); main->press(false); }
                check(true, "M9: early jog, Q-link and MAIN input do not dereference missing screens");
                selfTestSteps.push_back([this, check, firstBarrier, secondBarrier, attempts] {
                    if(!*firstBarrier && ++(*attempts)[0] < 40) { --selfTestIndex; return; }
                    auto& root = kick_ui::KickHome::Instance();
                    check(root.getScreen(kick_ui::Screen::HomeListView) != nullptr && root.getCurrentScreen() == kick_ui::Screen::HomeListView,
                        "M9: startup navigation reaches Main after deferred creation");
                    kick_ui::signals::sigScreenClose(kick_ui::Screen::Temp);
                    juce::MessageManager::callAsync([secondBarrier] { *secondBarrier = true; });
                });
                selfTestSteps.push_back([this, check, secondBarrier, attempts] {
                    if(!*secondBarrier && ++(*attempts)[1] < 40) { --selfTestIndex; return; }
                    auto& root = kick_ui::KickHome::Instance();
                    check(root.getCurrentScreen() == kick_ui::Screen::HomeListView && root.getScreen(kick_ui::Screen::Temp) == nullptr,
                        "M9: missing screen navigation falls back without registering a null screen");
                });
            }
            juce::MessageManager::callAsync([firstBarrier] { *firstBarrier = true; });
            selfTestSteps.push_back([failed] {
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(500);
            return;
        }
        selfTestSteps.push_back([] {});
        selfTestSteps.push_back([check] {
            {
                MidiFixture f;
                f.send(f.on()); // same source was already sounding through tracks
                f.learn.arm("pad"); f.send(f.on()); // learn without a release yet
                f.send(f.on()); // a retrigger now presses the mapped pad
                const bool consumed = f.send(f.off());
                check(!consumed && !f.values.empty() && f.values.back() == 0,
                    "M1: overlapping musical/mapped note-off releases both destinations");
            }
            {
                MidiFixture f;
                f.send(f.on()); f.learn.arm("pad"); f.send(f.on()); f.send(f.on());
                f.learn.remove("pad");
                check(!f.send(f.off()), "M1: unmapping an overlapping note still forwards its musical release");
            }
            {
                MidiFixture f;
                f.learn.arm("button"); f.send(f.cc(64, 127)); f.send(f.cc(64, 0));
                f.learn.setEnabled(false); f.send(f.cc(64, 127));
                f.learn.setEnabled(true);
                check(!f.send(f.cc(64, 0)), "M2: sustain begun while bypassed still receives its release after enabling mappings");
            }
            {
                MidiFixture f;
                f.send(f.cc(64, 127)); f.learn.arm("button");
                check(!f.send(f.cc(64, 63)) && f.learn.mappingCount() == 0 && f.learn.isLearning(),
                    "M2: learning preserves a previously routed pedal release without assigning it");
            }
            {
                MidiFixture f;
                f.learn.arm("button"); f.send(f.cc(64, 63));
                check(f.learn.mappingCount() == 0 && f.learn.isLearning(), "M2: pedal values below 64 cannot teach a button press");
                f.send(f.cc(64, 127)); f.send(f.cc(64, 0)); f.send(f.cc(64, 64)); f.send(f.cc(64, 63));
                check(f.values == std::vector<int>{64, 0}, "M2: MIDI switch CCs use the specified 64/63 on/off boundary");
            }
            {
                MidiFixture f;
                f.learn.setEnabled(false); f.learn.arm("button"); f.learn.setLearning(false);
                check(!f.learn.isEnabled(), "M3: cancelling learn preserves the previous bypass state");
                f.learn.restore();
                check(!f.learn.isEnabled(), "M3: cancelled learn agrees with saved bypass state");
            }
            {
                MidiFixture f;
                f.learn.arm("encoder");
                bool passed = true;
                for(int number = 120; number < 128; ++number) passed &= !f.send(f.cc(number, 127));
                check(passed && f.learn.mappingCount() == 0, "M4: channel-mode commands pass to tracks and cannot become mappings");
                f.temp.file.replaceWithText("{\"version\":1,\"mappings\":[{\"target\":\"encoder\",\"device\":\"controller\",\"channel\":1,\"number\":121,\"cc\":true,\"encoding\":0}]}");
                f.learn.restore();
                check(f.learn.mappingCount() == 0, "M4: saved mappings cannot intercept reserved channel-mode commands");
            }
            {
                MidiFixture f;
                f.learn.arm("button"); f.send(f.cc(64, 127)); f.send(f.cc(64, 0));
                f.learn.arm("pad"); f.send(f.on(48)); f.send(f.off(48));
                f.send(f.cc(64, 127)); f.send(f.on(48));
                const auto before = f.values.size();
                const bool resetPassed = !f.send(f.cc(121, 0));
                const bool pedalReleased = f.values.size() == before + 1 && f.values.back() == 0;
                f.send(f.off(48));
                check(resetPassed && pedalReleased && f.values.size() == before + 2,
                    "M4: Reset All Controllers releases mapped CC switches while retaining held note controls");
            }
            {
                MidiFixture f;
                f.learn.arm("encoder", MidiLearn::Encoding::twosComplement); f.send(f.cc(21, 0)); f.send(f.cc(21, 64));
                check(f.values == std::vector<int>{-16}, "M5: signed seven-bit encoder minimum decodes negative and obeys the step cap");
            }
            {
                MidiFixture f;
                f.learn.arm("pad"); f.send(f.on()); f.send(f.off());
                f.learn.remove("pad"); f.learn.devicesChanged({});
                check(f.send(f.on()) && f.values.empty(), "M6: unmapping before disconnect cannot let late queued notes reach tracks");
            }
            {
                MidiFixture f;
                f.send(f.on()); f.send(f.cc(64, 127)); f.learn.devicesChanged({});
                const bool noteReleased = !f.send(f.off());
                const bool sustainReleased = !f.send(f.cc(64, 0));
                check(noteReleased && sustainReleased && f.send(f.on()) && f.values.empty(),
                    "M6: disconnect forwards owed note/pedal releases while rejecting late presses");
            }
        });
        selfTestSteps.push_back([this, check] {
            std::vector<CPktMgr::HWMsg_t> packets;
            sigslot::scoped_connection capture = Controls::InputEvents::dispatchHardwareEvent.connect([&](CPktMgr::HWMsg_t& m) { packets.push_back(m); });
            auto send = [this](const juce::MidiMessage& m) { return midiLearn->handle("audit-controller", "Audit controller", m); };
            midiLearn->clear(); ShiftLatch::setLatched(false);
            midiLearn->arm("button-" + juce::String(DCC::EN_SHIFT));
            send(MidiFixture::on(61)); send(MidiFixture::off(61)); send(MidiFixture::on(61));
            packets.clear();
            for(int i = 0; i < 2; ++i) { auto* bank = findButton(DCC::EN_BANK_A); bank->press(true); bank->press(false); }
            bool shiftedTwice = packets.size() == 4;
            for(const auto& packet : packets) shiftedTwice &= !packet.data.empty() && packet.data[0] == DCC::EN_BANK_E;
            check(shiftedTwice && ShiftLatch::isLatched(), "M7: a held MIDI Shift remains effective for multiple panel buttons");
            send(MidiFixture::off(61));
            check(!ShiftLatch::isLatched(), "M7: MIDI Shift releases with the controller");
            send(MidiFixture::on(61)); ShiftLatch::setLatched(true); send(MidiFixture::off(61));
            check(ShiftLatch::isLatched(), "M7: releasing MIDI Shift preserves an independently latched mouse Shift");
            auto* bank = findButton(DCC::EN_BANK_A); bank->press(true); bank->press(false);
            check(!ShiftLatch::isLatched(), "M7: mouse Shift remains one-shot after MIDI Shift is released");
            midiLearn->clear();
            auto* wrapper = findParentComponentOfClass<Simulator>();
            midiLearn->setLearning(true);
            packets.clear();
            const bool handled = wrapper && wrapper->getPeer() && wrapper->getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::escapeKey));
            check(handled && !midiLearn->isLearning() && packets.empty(), "Escape cancels learn through actual focused-window key routing");
            midiLearn->setLearning(false);
        });
        selfTestSteps.push_back([check] {
            struct RestoreFilter
            {
                std::function<bool(const mt::MTMidiMessage&)> old = mt::MTMidiDeviceManager::simulatorInputFilter;
                ~RestoreFilter() { mt::MTMidiDeviceManager::simulatorInputFilter = std::move(old); }
            } restore;
            juce::Array<mt::MTMidiMessage> batch;
            std::vector<int> expected;
            for(int i = 0; i < 512; ++i)
            {
                mt::MTMidiMessage m;
                m.sourceIdentifier = "input-" + juce::String(i);
                m.sourceName = "Input";
                m.receivedAtMilliseconds = i;
                m.message = i % 8 == 0 ? juce::MidiMessage::midiClock() : MidiFixture::cc(21, i % 128);
                batch.add(m);
                if(i % 8 == 0 || i % 3 != 0) expected.push_back(i);
            }
            std::vector<int> visited;
            mt::MTMidiDeviceManager::simulatorInputFilter = [&](const mt::MTMidiMessage& m) {
                const int i = (int)m.receivedAtMilliseconds; visited.push_back(i);
                return !m.message.isMidiClock() && i % 3 == 0;
            };
            mt::MTMidiDeviceManager::filterSimulatorMidiMessages(batch);
            bool correct = batch.size() == (int)expected.size() && visited.size() == 512;
            for(int i = 0; i < juce::jmin(batch.size(), (int)expected.size()); ++i)
            {
                correct &= batch.getReference(i).receivedAtMilliseconds == expected[(size_t)i]
                    && batch.getReference(i).sourceIdentifier == "input-" + juce::String(expected[(size_t)i]);
                const auto& message = batch.getReference(i).message;
                correct &= expected[(size_t)i] % 8 == 0 ? message.isMidiClock()
                    : message.isController() && message.getControllerNumber() == 21 && message.getControllerValue() == expected[(size_t)i] % 128;
            }
            for(int i = 0; i < (int)visited.size(); ++i) correct &= visited[(size_t)i] == i;
            check(correct, "M8: mixed MIDI filtering preserves clock, order, identity and timestamps exactly once");
            mt::MTMidiDeviceManager::simulatorInputFilter = [](const mt::MTMidiMessage&) { return true; };
            mt::MTMidiDeviceManager::filterSimulatorMidiMessages(batch);
            check(batch.isEmpty(), "M8: fully consumed MIDI batches leave no events to replay");
            if(juce::SystemStats::getEnvironmentVariable("KICKEM_MIDI_BENCH", {}) == "1")
            {
                juce::Array<mt::MTMidiMessage> source;
                for(int i = 0; i < 4096; ++i)
                {
                    mt::MTMidiMessage m; m.sourceName = "Input"; m.sourceIdentifier = "controller";
                    m.receivedAtMilliseconds = i; m.message = MidiFixture::cc(21, i % 128); source.add(m);
                }
                mt::MTMidiDeviceManager::simulatorInputFilter = [](const mt::MTMidiMessage& m) { return (int)m.receivedAtMilliseconds % 2 == 0; };
                std::vector<double> oldTimes, newTimes;
                for(int run = 0; run < 6; ++run) for(bool legacy : {true, false})
                {
                    auto messages = source;
                    const auto start = juce::Time::getMillisecondCounterHiRes();
                    if(legacy)
                    {
                        for(int i = 0; i < messages.size();)
                            if(mt::MTMidiDeviceManager::simulatorInputFilter(messages.getReference(i))) messages.remove(i); else ++i;
                    }
                    else mt::MTMidiDeviceManager::filterSimulatorMidiMessages(messages);
                    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
                    juce::int64 checksum = 0;
                    for(const auto& message : messages) checksum += (juce::int64)message.receivedAtMilliseconds;
                    if(messages.size() != 2048 || checksum != 4194304)
                    { check(false, "M8: benchmark must retain the expected MIDI payload"); return; }
                    if(run > 0) (legacy ? oldTimes : newTimes).push_back(elapsed);
                }
                std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
                SimulatorInterfaceImpl::log("SELFTEST BENCH 4096-event MIDI batch, half consumed; median legacy " + juce::String(oldTimes[2], 3)
                    + " ms, current " + juce::String(newTimes[2], 3) + " ms");
            }
        });
        selfTestSteps.push_back([failed] {
            SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
            juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        });
        startTimer(200);
    }
}
