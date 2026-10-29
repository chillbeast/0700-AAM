// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_ui/kick_ui.h>
#include <audio_midi/audio_midi.h>
#include "kick_Simulator.h"
#include "kick_SimulatorInterfaceImpl.h"

namespace sim
{
    void SimulatorBoard::setupMidiLearn(const std::vector<SimButton*>& buttons)
    {
        midiLearn = std::make_unique<MidiLearn>(getAppDirs(MTPrefs).getChildFile("simulator-midi.json"));
        auto add = [this](juce::String id, juce::String name, MidiLearn::Kind kind, MidiMappable& control,
                          std::function<void(int)> action, std::function<void(int)> pressure = {}, bool primary = true) {
            midiLearn->addTarget({id, name, kind, &control, std::move(action), std::move(pressure)});
            if(primary) control.onMidiMappingGesture = [this, id](const juce::MouseEvent& e) { return midiLearn->gesture(id, e); };
        };
        for(auto* button : buttons)
        {
            const auto id = "button-" + juce::String(button->getCode());
            const auto name = button->getTooltip().upToFirstOccurrenceOf(" / Shift:", false, false);
            add(id, name, MidiLearn::Kind::button, *button, [button](int value) {
                if(button->getCode() == DCC::EN_SHIFT) ShiftLatch::setHeld(value > 0);
                else button->press(value > 0);
            });
            if(button->getShiftCode() != DCC::EN_INVALID_ID)
            {
                const int code = button->getShiftCode();
                add("button-" + juce::String(code), button->getTooltip().fromFirstOccurrenceOf(" / Shift: ", false, false),
                    MidiLearn::Kind::button, *button, [code](int value) { hw::send(hw::buttonMessage(code, value ? DCC::BTN_PRESSED : DCC::BTN_RELEASED)); }, {}, false);
            }
        }
        for(int i = 0; i < 16; ++i)
            add("pad-" + juce::String(i), "Pad " + juce::String(i + 1), MidiLearn::Kind::pad, pads.getPad(i),
                [this, i](int value) { pads.getPad(i).hit(value > 0, value); }, [this, i](int value) { pads.getPad(i).press(value); });
        for(int i = 0; i < 4; ++i)
            add("qlink-" + juce::String(i), "Q-link P" + juce::String(i + 1), MidiLearn::Kind::encoder, *qlinks[i],
                [this, i](int steps) { qlinks[i]->turn(steps); });
        add("jog", "Jog turn", MidiLearn::Kind::encoder, mainControl.getJog(), [this](int steps) { mainControl.getJog().turn(steps); });
        add("jog-enter", "Jog Enter", MidiLearn::Kind::button, mainControl.getJog(), [this](int value) { mainControl.getJog().pushCentre(value > 0); }, {}, false);
        add("touch-strip", "Touch strip", MidiLearn::Kind::slider, slider, [this](int value) { slider.setTouch(value); });
        midiLearn->restore();
        struct Overlay : juce::Component
        {
            MidiLearn& learn;
            explicit Overlay(MidiLearn& m) : learn(m) { setInterceptsMouseClicks(false, false); setAlwaysOnTop(true); }
            void paint(juce::Graphics& g) override { learn.paintTargets(g, *getParentComponent()); }
        };
        midiOverlay = std::make_unique<Overlay>(*midiLearn);
        addAndMakeVisible(*midiOverlay);
        midiLearn->onChange = [this] { midiOverlay->repaint(); if(onMidiChanged) onMidiChanged(); };
        mt::MTMidiDeviceManager::simulatorInputFilter = [this](const mt::MTMidiMessage& m) {
            return midiLearn->handle(m.sourceIdentifier, m.sourceName, m.message);
        };
        midiDevicesConnection = mt::signals::midiInputsChanged.connect([this] {
            juce::StringArray connected;
            for(const auto& entry : mt::MTMidiDeviceManager::getMidiDeviceList(true))
                if(entry->inDevice) connected.add(entry->deviceInfo.identifier);
            midiLearn->devicesChanged(connected);
        });
    }

    void SimulatorBoard::startMidiLearnTest()
    {
        auto failures = std::make_shared<int>(0);
        auto check = [failures](bool ok, const juce::String& text) {
            if(!ok) ++*failures;
            SimulatorInterfaceImpl::log("SELFTEST " + juce::String(ok ? "PASS " : "FAIL ") + text);
        };
        selfTestSteps.push_back([] {});
        selfTestSteps.push_back([this, check] {
            auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
            auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kick-midi-project", "", false).getChildFile("Template 01");
            check(source.copyDirectoryTo(scratch) && mt::getAudioInterface().loadProject(scratch), "load isolated template for MIDI panel checks");
            if(auto* b = findButton(DCC::EN_MAINMODE)) { b->press(true); b->press(false); }
        });
        for(int i = 0; i < 8; ++i) selfTestSteps.push_back([] {});
        selfTestSteps.push_back([this, check] {
            if(juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}) == "midi-learn-preference")
            {
                check(midiLearn->mappingCount() == 1 && midiLearn->description("qlink-0").contains("Test Controller"), "MIDI mapping survives application restart");
                return;
            }
            const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("kick-midi-" + juce::Uuid().toString() + ".json");
            struct Cleanup { juce::File f; ~Cleanup() { f.deleteFile(); } } cleanup {file};
            MidiLearn learn(file);
            std::vector<int> padValues, buttonValues, encoderValues, stripValues, pressures;
            auto add = [&](juce::String id, MidiLearn::Kind kind, std::vector<int>& values) {
                learn.addTarget({id, id, kind, nullptr, [&values](int value) { values.push_back(value); },
                    [&pressures](int value) { pressures.push_back(value); }});
            };
            add("pad", MidiLearn::Kind::pad, padValues);
            add("button", MidiLearn::Kind::button, buttonValues);
            add("encoder", MidiLearn::Kind::encoder, encoderValues);
            add("strip", MidiLearn::Kind::slider, stripValues);
            auto send = [&](const juce::MidiMessage& msg, juce::String device = "test-device") { return learn.handle(device, "Test Controller", msg); };
            auto on = [](int channel, int note, int velocity = 100) { return juce::MidiMessage::noteOn(channel, note, (juce::uint8)velocity); };
            auto off = [](int channel, int note) { return juce::MidiMessage::noteOff(channel, note); };
            auto cc = [](int number, int value) { return juce::MidiMessage::controllerEvent(1, number, value); };
            check(!send(on(1, 60)), "unmapped keyboard note passes to musical routing");
            learn.arm("pad");
            check(!send(off(1, 60)), "keyboard note begun before learning still receives its note-off");
            send(off(1, 36));
            check(learn.mappingCount() == 0, "note-off cannot become a learned trigger");
            check(send(on(1, 36, 111)) && learn.mappingCount() == 1 && padValues.empty(), "learning consumes a note without firing the pad");
            check(send(off(1, 36)) && padValues.empty(), "learn gesture note-off is consumed");
            check(!send(on(2, 36)) && !send(off(2, 36)) && !send(on(1, 36), "other-device")
                && !send(off(1, 36), "other-device"), "controller identity and MIDI channel are isolated");
            check(send(on(1, 36, 87)) && padValues == std::vector<int>{87}, "mapped pad preserves note velocity and consumes musical routing");
            check(send(juce::MidiMessage::aftertouchChange(1, 36, 71)) && pressures == std::vector<int>{71}, "mapped pad preserves polyphonic pressure");
            send(on(1, 60));
            check(!send(juce::MidiMessage::channelPressureChange(1, 80)) && pressures.back() == 80,
                "channel pressure reaches mapped pads and normally routed keyboard notes together");
            send(off(1, 60));
            check(send(on(1, 36, 0)) && padValues.back() == 0, "zero-velocity note-on releases a mapped pad");
            send(on(1, 36));
            learn.remove("pad");
            check(padValues.back() == 0 && send(off(1, 36)), "removing a held mapping releases it and consumes its pending note-off");
            learn.arm("button"); send(cc(20, 0));
            check(learn.mappingCount() == 0, "CC button release cannot teach an accidental trigger");
            send(cc(20, 127)); send(cc(20, 0)); send(cc(20, 127)); send(cc(20, 100)); send(cc(20, 0));
            check(buttonValues == std::vector<int>{127, 0}, "CC buttons emit one press and release despite repeated high values");
            buttonValues.clear();
            learn.arm("button", MidiLearn::Encoding::absolute, false, true); send(cc(20, 127));
            send(cc(20, 127)); send(cc(20, 127)); send(cc(20, 0));
            check(buttonValues == std::vector<int>{127, 0, 127, 0}, "press-only CC trigger buttons release after every press");
            learn.arm("pad"); send(cc(20, 110));
            check(learn.mappingCount() == 1 && learn.description("button") == "Unmapped", "reusing an input reassigns its previous target");
            learn.arm("encoder"); send(on(1, 48)); send(off(1, 48));
            check(learn.mappingCount() == 1 && learn.isLearning(), "continuous controls require CC rather than notes");
            send(cc(21, 64)); send(cc(21, 66)); send(cc(21, 66)); send(cc(21, 63));
            check(encoderValues == std::vector<int>{2, -3}, "absolute CC produces signed encoder movement without an initial jump");
            encoderValues.clear();
            for(const auto encoding : {MidiLearn::Encoding::twosComplement, MidiLearn::Encoding::offset, MidiLearn::Encoding::signMagnitude})
            {
                learn.arm("encoder", encoding); send(cc(21, 1));
                send(cc(21, encoding == MidiLearn::Encoding::offset ? 65 : 1));
                send(cc(21, encoding == MidiLearn::Encoding::twosComplement ? 127 : encoding == MidiLearn::Encoding::offset ? 63 : 65));
            }
            check(encoderValues == std::vector<int>{1, -1, 1, -1, 1, -1}, "three common relative encoder formats decode both directions");
            learn.arm("strip"); send(cc(22, 0)); send(cc(22, 127)); send(cc(22, 0));
            check(stripValues == std::vector<int>{100, 0}, "touch strip maps the complete absolute CC range");
            learn.arm("pad"); send(on(1, 36)); send(off(1, 36)); send(on(1, 36));
            learn.devicesChanged({});
            check(padValues.back() == 0 && send(off(1, 36)), "disconnect releases mapped held pads without leaking a note-off");
            const auto beforeLate = padValues.size();
            check(send(on(1, 36)) && padValues.size() == beforeLate, "late queued input after disconnect cannot leave a pad held");
            learn.devicesChanged({"test-device"});
            send(on(1, 36));
            check(!send(juce::MidiMessage::allNotesOff(1)) && padValues.back() == 0, "MIDI All Notes Off releases mapped pads and passes to tracks");
            send(off(1, 36));
            send(on(1, 36)); learn.setEnabled(false);
            check(padValues.back() == 0 && send(off(1, 36)) && !send(on(1, 36)) && !send(off(1, 36)), "bypass releases held controls and resumes normal MIDI routing");
            learn.setEnabled(true);
            MidiLearn restored(file);
            std::vector<int> restoredValues;
            for(const auto& id : {"pad", "button", "encoder", "strip"})
                restored.addTarget({id, id, juce::String(id) == "encoder" ? MidiLearn::Kind::encoder : juce::String(id) == "strip" ? MidiLearn::Kind::slider : MidiLearn::Kind::pad,
                    nullptr, [&restoredValues](int value) { restoredValues.push_back(value); }, {}});
            restored.restore();
            check(restored.mappingCount() == learn.mappingCount() && restored.description("encoder") == learn.description("encoder"), "controller mappings and relative format reload from disk");
            restored.handle("test-device", "Test Controller", cc(21, 65));
            check(restoredValues == std::vector<int>{-1}, "restored relative encoder keeps its selected format");
            learn.setLearning(true); learn.arm("pad", MidiLearn::Encoding::absolute, true); send(on(1, 37)); send(off(1, 37));
            check(learn.isLearning(), "toolbar learn mode stays on for mapping the next control");
            learn.setLearning(false);
            check(!learn.isLearning() && !send(juce::MidiMessage::midiClock()), "cancel learning leaves MIDI clock untouched");
            learn.clear();
            check(learn.mappingCount() == 0, "clear mappings removes saved assignments");
            file.replaceWithText("{\"version\":1,\"mappings\":[null,{\"target\":\"encoder\",\"device\":\"test\",\"channel\":{},\"number\":21,\"cc\":true,\"encoding\":0}]}");
            restored.restore();
            check(restored.mappingCount() == 0, "malformed mapping entries are ignored safely");
        });
        selfTestSteps.push_back([this, check] {
            const bool restart = juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}) == "midi-learn-preference";
            // Startup enumeration correctly marks the synthetic port absent.
            // Reconnect it before injecting events, as a real controller would.
            midiLearn->devicesChanged({"test-controller"});
            std::vector<CPktMgr::HWMsg_t> packets;
            sigslot::scoped_connection capture = Controls::InputEvents::dispatchHardwareEvent.connect([&](CPktMgr::HWMsg_t& m) { packets.push_back(m); });
            auto input = [](juce::MidiMessage message) {
                mt::MTMidiMessage event;
                event.sourceIdentifier = "test-controller";
                event.sourceName = "Test Controller";
                event.message = message;
                return mt::MTMidiDeviceManager::simulatorInputFilter(event);
            };
            if(!restart)
            {
                midiLearn->clear();
                midiLearn->arm("qlink-0"); input(juce::MidiMessage::controllerEvent(1, 74, 64));
            }
            for(bool hardware : {true, false})
            {
                findParentComponentOfClass<Simulator>()->setHardwareLayout(hardware);
                packets.clear();
                input(juce::MidiMessage::controllerEvent(1, 74, 64));
                input(juce::MidiMessage::controllerEvent(1, 74, 66));
                check(!packets.empty() && packets.back().data == hw::knobMessage(DCC::EN_KNOB_1, 2).data,
                    "learned MIDI reaches firmware Q-link packets in " + juce::String(hardware ? "Hardware" : "Original") + " layout");
            }
            auto* wrapper = findParentComponentOfClass<Simulator>();
            if(wrapper) wrapper->setHardwareLayout(true);
            midiLearn->setLearning(true);
            packets.clear();
            auto& pad = pads.getPad(0);
            const auto point = pad.getLocalBounds().getCentre().toFloat();
            juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), point, juce::ModifierKeys::leftButtonModifier,
                1.f, 0.f, 0.f, 0.f, 0.f, &pad, &pad, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
            pad.mouseDown(event); pad.mouseDrag(event); pad.mouseUp(event);
            check(packets.empty() && midiLearn->getStatus().contains("Pad 1"), "clicking a pad in learn mode selects it without sounding it");
            if(wrapper)
            {
                const auto path = juce::SystemStats::getEnvironmentVariable("KICKEM_SHOT", {});
                if(path.isNotEmpty()) if(auto out = juce::File(path).getChildFile("midi-learn.png").createOutputStream())
                    juce::PNGImageFormat().writeImageToStream(wrapper->createComponentSnapshot(wrapper->getLocalBounds()), *out);
                check(wrapper->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !midiLearn->isLearning(), "Escape cancels toolbar learning");
            }
            juce::MouseEvent rightClick(juce::Desktop::getInstance().getMainMouseSource(), point, juce::ModifierKeys::rightButtonModifier,
                1.f, 0.f, 0.f, 0.f, 0.f, &pad, &pad, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
            packets.clear();
            pad.mouseDown(rightClick); pad.mouseUp(rightClick);
            check(packets.empty() && juce::PopupMenu::dismissAllActiveMenus(), "right-click opens a mapping menu without playing or releasing a pad");
        });
        selfTestSteps.push_back([failures] {
            SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failures) + " failures");
            juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failures);
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        });
        startTimer(200);
    }
}
