// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_ui/kick_ui.h>
#include <audio_midi/audio_midi.h>
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
