// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_MidiLearn.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <tuple>

namespace sim
{
    bool MidiLearn::Source::operator<(const Source& other) const
    {
        return std::tie(device, channel, cc, number) < std::tie(other.device, other.channel, other.cc, other.number);
    }

    MidiLearn::MidiLearn(juce::File file) : preferences(std::move(file)) {}
    MidiLearn::~MidiLearn() { lifetime.reset(); releaseHeld(); }

    void MidiLearn::addTarget(Target target)
    {
        const auto inserted = targets.emplace(target.id, std::move(target));
        auto* component = inserted.first->second.component;
        if(inserted.second && component && std::find(highlightControls.begin(), highlightControls.end(), component) == highlightControls.end())
            highlightControls.push_back(component);
    }

    void MidiLearn::changed(juce::String text)
    {
        status = std::move(text);
        if(onChange) onChange();
    }

    void MidiLearn::setLearning(bool value)
    {
        releaseHeld();
        learning = keepLearning = value;
        armed.clear();
        changed(value ? "Click a panel control, then move your MIDI controller" : "MIDI learn off");
    }

    void MidiLearn::setEnabled(bool value)
    {
        releaseHeld();
        enabled = value;
        if(!value) { learning = keepLearning = false; armed.clear(); }
        if(save()) changed(value ? "Controller mappings enabled" : "Mappings bypassed: MIDI passes to tracks");
    }

    void MidiLearn::arm(const juce::String& id, Encoding encoding, bool continuous, bool pulse)
    {
        auto it = targets.find(id);
        if(it == targets.end()) return;
        releaseHeld();
        learning = true; // enable mappings only after a successful assignment
        keepLearning = continuous;
        armed = id;
        armedEncoding = encoding;
        armedPulse = pulse;
        changed("Learning " + it->second.name + ((it->second.kind == Kind::encoder || it->second.kind == Kind::slider)
            ? ": move a CC control" : ": press a MIDI note or CC button"));
    }

    void MidiLearn::release(Binding& binding)
    {
        auto it = targets.find(binding.target);
        if(binding.down && it != targets.end()) it->second.apply(0);
        binding.down = false;
        binding.previous = -1;
    }

    void MidiLearn::releaseHeld() { for(auto& binding : bindings) release(binding); }

    void MidiLearn::devicesChanged(const juce::StringArray& connected)
    {
        for(const auto& id : connected) { knownDevices.insert(id); disconnectedDevices.erase(id); }
        for(const auto& id : knownDevices)
            if(!connected.contains(id)) disconnectedDevices.insert(id);
        for(auto& binding : bindings)
            if(!connected.contains(binding.source.device))
            { disconnectedDevices.insert(binding.source.device); release(binding); }
    }

    bool MidiLearn::handle(const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage& message)
    {
        if(deviceId.isEmpty()) return false;
        knownDevices.insert(deviceId);
        const bool disconnected = disconnectedDevices.count(deviceId) != 0;
        if(message.isController() && message.getControllerNumber() >= 120)
        {
            // Channel-mode commands belong to the musical route, not MIDI learn.
            const bool allNotes = message.isAllNotesOff() || message.isAllSoundOff();
            const bool resetControllers = message.getControllerNumber() == 121;
            for(auto& binding : bindings)
                if(binding.source.device == deviceId && binding.source.channel == message.getChannel()
                    && (allNotes || (resetControllers && binding.source.cc))) release(binding);
            if(resetControllers)
                for(auto it = passedSwitches.begin(); it != passedSwitches.end();)
                    if(it->device == deviceId && it->channel == message.getChannel()) it = passedSwitches.erase(it); else ++it;
            return false;
        }
        if(message.isAftertouch() || message.isChannelPressure())
        {
            if(disconnected) return true;
            bool consumed = false;
            for(auto& binding : bindings)
                if(binding.source.device == deviceId && binding.source.channel == message.getChannel()
                    && !binding.source.cc && (message.isChannelPressure() || binding.source.number == message.getNoteNumber()))
                {
                    const auto& target = targets.at(binding.target);
                    if(enabled && !learning && binding.down && target.pressure)
                        target.pressure(message.isAftertouch() ? message.getAfterTouchValue() : message.getChannelPressureValue());
                    consumed = true;
                }
            for(const auto& note : passedNotes)
                if(note.device == deviceId && note.channel == message.getChannel()
                    && (message.isChannelPressure() || note.number == message.getNoteNumber())) return false;
            return enabled && consumed;
        }
        if(!message.isController() && !message.isNoteOnOrOff()) return false;
        const Source source { deviceId, message.getChannel(), message.isController() ? message.getControllerNumber() : message.getNoteNumber(), message.isController() };
        const int value = source.cc ? message.getControllerValue() : (message.isNoteOn() ? message.getVelocity() : 0);
        const bool noteOff = !source.cc && message.isNoteOff();
        const bool switchCC = source.cc && source.number >= 64 && source.number <= 69;
        const int gateValue = switchCC && value < 64 ? 0 : value;
        // A note begun in normal keyboard routing must still reach its track
        // when learn mode or a mapping is enabled while that note is held.
        const bool forwardRelease = (noteOff && passedNotes.erase(source) != 0)
            || (switchCC && value < 64 && passedSwitches.erase(source) != 0);
        const bool swallowedRelease = noteOff && swallowedNotes.erase(source) != 0;
        // Reject late presses, while preserving releases already owed to the
        // musical route. Mapped controls were released by devicesChanged().
        if(disconnected) return !forwardRelease;
        if(learning)
        {
            if(!source.cc && !noteOff) swallowedNotes.insert(source);
            if(!armed.isEmpty() && !noteOff)
            {
                const auto& target = targets.at(armed);
                const bool continuous = target.kind == Kind::encoder || target.kind == Kind::slider;
                if((continuous && !source.cc) || (switchCC && gateValue == 0) || (!continuous && gateValue == 0)) return !forwardRelease;
                const auto id = armed;
                juce::String replaced;
                for(auto it = bindings.begin(); it != bindings.end();)
                {
                    if(it->target == id || (!(source < it->source) && !(it->source < source)))
                    {
                        if(it->target != id) replaced = " (reassigned from " + targets.at(it->target).name + ")";
                        release(*it);
                        it = bindings.erase(it);
                    }
                    else ++it;
                }
                bindings.push_back({source, id, deviceName, armedEncoding, source.cc ? value : -1, false, armedPulse});
                enabled = true;
                armed.clear();
                learning = keepLearning;
                if(save()) changed("Mapped " + target.name + " to " + description(id) + replaced);
            }
            return !forwardRelease;
        }
        if(enabled)
        {
            for(auto& binding : bindings)
            {
                if(source < binding.source || binding.source < source) continue;
                const auto& target = targets.at(binding.target);
                if(!source.cc && !noteOff) swallowedNotes.insert(source);
                if(target.kind == Kind::encoder)
                {
                    int delta = 0;
                    switch(binding.encoding)
                    {
                        case Encoding::absolute: if(binding.previous >= 0) delta = value - binding.previous; break;
                        case Encoding::twosComplement: delta = value < 64 ? value : value - 128; break;
                        case Encoding::offset: delta = value - 64; break;
                        case Encoding::signMagnitude: delta = value < 64 ? value : -(value - 64); break;
                    }
                    binding.previous = value;
                    if(delta) target.apply(juce::jlimit(-16, 16, delta));
                }
                else if(target.kind == Kind::slider) target.apply(juce::roundToInt(value * 100.0 / 127.0));
                else
                {
                    const bool down = gateValue > 0;
                    if(binding.pulse)
                    {
                        if(down) { target.apply(value); target.apply(0); }
                    }
                    else
                    {
                        if(!source.cc && down && binding.down && target.kind == Kind::pad) target.apply(0);
                        if(binding.down != down || (!source.cc && down && target.kind == Kind::pad))
                        { binding.down = down; target.apply(down ? value : 0); }
                    }
                }
                return !forwardRelease;
            }
        }
        if(swallowedRelease) return !forwardRelease;
        if(!source.cc && !noteOff) passedNotes.insert(source);
        if(switchCC && value >= 64) passedSwitches.insert(source);
        return false;
    }

    juce::String MidiLearn::encodingName(Encoding encoding)
    {
        switch(encoding)
        {
            case Encoding::absolute: return "Absolute CC";
            case Encoding::twosComplement: return "Relative: two's complement (1 / 127)";
            case Encoding::offset: return "Relative: offset (65 / 63)";
            case Encoding::signMagnitude: return "Relative: sign magnitude (1 / 65)";
        }
        return {};
    }

    juce::String MidiLearn::description(const juce::String& id) const
    {
        for(const auto& binding : bindings) if(binding.target == id)
            return binding.deviceName + ", ch " + juce::String(binding.source.channel)
                + (binding.source.cc ? ", CC " : ", note ") + juce::String(binding.source.number);
        return "Unmapped";
    }

    void MidiLearn::remove(const juce::String& id)
    {
        for(auto it = bindings.begin(); it != bindings.end();)
            if(it->target == id) { release(*it); it = bindings.erase(it); } else ++it;
        const auto target = targets.find(id);
        if(save()) changed("Removed mapping: " + (target != targets.end() ? target->second.name : juce::String("control")));
    }

    void MidiLearn::clear()
    {
        setLearning(false);
        bindings.clear();
        if(save()) changed("All controller mappings cleared");
    }

    bool MidiLearn::save()
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("version", 1);
        object->setProperty("enabled", enabled);
        juce::Array<juce::var> rows;
        for(const auto& binding : bindings)
        {
            juce::DynamicObject::Ptr row = new juce::DynamicObject();
            row->setProperty("target", binding.target);
            row->setProperty("device", binding.source.device);
            row->setProperty("deviceName", binding.deviceName);
            row->setProperty("channel", binding.source.channel);
            row->setProperty("number", binding.source.number);
            row->setProperty("cc", binding.source.cc);
            row->setProperty("encoding", (int)binding.encoding);
            row->setProperty("pulse", binding.pulse);
            rows.add(juce::var(row.get()));
        }
        object->setProperty("mappings", rows);
        if(!preferences.getParentDirectory().createDirectory() || !preferences.replaceWithText(juce::JSON::toString(juce::var(object.get()))))
        {
            changed("Could not save MIDI mappings: " + preferences.getFullPathName());
            return false;
        }
        return true;
    }

    void MidiLearn::restore()
    {
        if(!preferences.existsAsFile()) return;
        const auto data = preferences.getSize() <= 1024 * 1024 ? juce::JSON::parse(preferences) : juce::var();
        if(!data.isObject() || !data["version"].isInt() || (int)data["version"] != 1 || !data["mappings"].isArray())
        { changed("MIDI mapping file is invalid; existing file retained"); return; }
        releaseHeld();
        bindings.clear();
        learning = keepLearning = false;
        armed.clear();
        enabled = !data["enabled"].isBool() || (bool)data["enabled"];
        for(const auto& row : *data["mappings"].getArray())
        {
            if(!row.isObject() || !row["target"].isString() || !row["device"].isString()
                || !row["channel"].isInt() || !row["number"].isInt() || !row["encoding"].isInt() || !row["cc"].isBool()) continue;
            const auto id = row["target"].toString();
            const auto found = targets.find(id);
            const int channel = row["channel"], number = row["number"], encoding = row["encoding"];
            const bool cc = row["cc"];
            if(found == targets.end() || row["device"].toString().isEmpty() || channel < 1 || channel > 16
                || number < 0 || number > 127 || encoding < 0 || encoding > 3
                || (cc && number >= 120)
                || (!cc && (found->second.kind == Kind::encoder || found->second.kind == Kind::slider))) continue;
            Source source {row["device"].toString(), channel, number, cc};
            knownDevices.insert(source.device);
            bool duplicate = false;
            for(const auto& b : bindings) duplicate |= b.target == id || (!(source < b.source) && !(b.source < source));
            if(!duplicate) bindings.push_back({source, id, row["deviceName"].toString(), (Encoding)encoding, -1, false,
                row["pulse"].isBool() && (bool)row["pulse"]});
        }
        changed(juce::String(mappingCount()) + " controller mappings loaded");
    }

    juce::PopupMenu MidiLearn::targetMenu(const juce::String& id)
    {
        juce::PopupMenu menu;
        const auto& target = targets.at(id);
        menu.addSectionHeader(target.name);
        menu.addSectionHeader(description(id));
        const std::weak_ptr<Lifetime> weak = lifetime;
        auto safe = [this, weak](std::function<void()> action) { if(!weak.expired()) action(); };
        menu.addItem("Learn MIDI", [this, safe, id] { safe([&] { arm(id); }); });
        if(target.kind == Kind::button)
            menu.addItem("Learn MIDI trigger button (press only)", [this, safe, id] { safe([&] { arm(id, Encoding::absolute, false, true); }); });
        for(const auto& binding : bindings) if(binding.target == id)
            menu.addSectionHeader(target.kind == Kind::encoder ? encodingName(binding.encoding)
                : binding.pulse ? "Trigger on every press" : "Momentary press / release");
        if(target.kind == Kind::encoder)
        {
            juce::PopupMenu relative;
            for(int i = 1; i <= 3; ++i)
                relative.addItem(encodingName((Encoding)i), [this, safe, id, i] { safe([&] { arm(id, (Encoding)i); }); });
            menu.addSubMenu("Learn relative encoder", relative);
        }
        menu.addItem("Remove mapping", [this, safe, id] { safe([&] { remove(id); }); });
        return menu;
    }

    void MidiLearn::showTargetMenu(const juce::String& id)
    {
        if(targets.count(id) == 0) return;
        auto menu = targetMenu(id);
        // A component can expose an alternate command or the jog's Enter push.
        for(const auto& item : targets)
            if(item.first != id && item.second.component == targets.at(id).component)
                menu.addSubMenu(item.second.name, targetMenu(item.first));
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(targets.at(id).component));
    }

    bool MidiLearn::gesture(const juce::String& id, const juce::MouseEvent& event)
    {
        if(event.mods.isPopupMenu()) { showTargetMenu(id); return true; }
        if(learning) { arm(id, Encoding::absolute, keepLearning); return true; }
        return false;
    }

    void MidiLearn::showMenu(juce::Component* anchor)
    {
        juce::PopupMenu menu;
        const std::weak_ptr<Lifetime> weak = lifetime;
        menu.addItem("Controller mappings enabled", true, enabled, [this, weak] { if(!weak.expired()) setEnabled(!enabled); });
        menu.addItem("Cancel MIDI learn", learning, false, [this, weak] { if(!weak.expired()) setLearning(false); });
        menu.addItem("Release held controls", [this, weak] { if(!weak.expired()) releaseHeld(); });
        menu.addSeparator();
        menu.addSectionHeader("MIDI inputs (opened automatically)");
        const auto inputs = juce::MidiInput::getAvailableDevices();
        if(inputs.isEmpty()) menu.addItem(0, "No MIDI inputs connected", false);
        for(const auto& input : inputs) menu.addItem(0, input.name, false);
        menu.addSeparator();
        juce::PopupMenu mapped;
        for(const auto& binding : bindings) mapped.addSubMenu(targets.at(binding.target).name, targetMenu(binding.target));
        menu.addSubMenu("Mapped controls (" + juce::String(mappingCount()) + ")", mapped, !bindings.empty());
        juce::PopupMenu clearMenu;
        clearMenu.addItem("Remove all controller mappings", [this, weak] { if(!weak.expired()) clear(); });
        menu.addSubMenu("Clear mappings", clearMenu, !bindings.empty());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(anchor));
    }

    void MidiLearn::paintTargets(juce::Graphics& g, juce::Component& root) const
    {
        if(!learning) return;
        const auto selectedTarget = targets.find(armed);
        const auto* selectedControl = selectedTarget != targets.end() ? selectedTarget->second.component : nullptr;
        for(auto* control : highlightControls)
        {
            if(!control->isVisible()) continue;
            const bool selected = selectedControl == control;
            g.setColour(selected ? juce::Colours::orange : juce::Colours::cyan.withAlpha(0.45f));
            g.drawRoundedRectangle(root.getLocalArea(control, control->getLocalBounds()).toFloat().reduced(2), 4, selected ? 4.f : 2.f);
        }
    }
}
