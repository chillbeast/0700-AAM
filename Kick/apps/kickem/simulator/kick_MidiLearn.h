// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include <functional>

namespace sim
{
    // Owned and called exclusively on the message thread. MIDI is intercepted
    // after the device manager's existing MIDI-thread queue has been drained.
    class MidiLearn
    {
    public:
        enum class Kind { button, pad, encoder, slider };
        enum class Encoding { absolute, twosComplement, offset, signMagnitude };
        struct Target
        {
            juce::String id, name;
            Kind kind;
            juce::Component* component;
            std::function<void(int)> apply;
            std::function<void(int)> pressure;
        };
        explicit MidiLearn(juce::File preferences);
        ~MidiLearn();
        void addTarget(Target target);
        void restore();
        void setLearning(bool enabled);
        bool isLearning() const { return learning; }
        bool isEnabled() const { return enabled; }
        void setEnabled(bool value);
        void arm(const juce::String& target, Encoding encoding = Encoding::absolute, bool continuous = false, bool pulse = false);
        bool handle(const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage& message);
        bool gesture(const juce::String& target, const juce::MouseEvent& event);
        void showTargetMenu(const juce::String& target);
        void showMenu(juce::Component* anchor);
        void paintTargets(juce::Graphics& g, juce::Component& root) const;
        void remove(const juce::String& target);
        void clear();
        void releaseHeld();
        void devicesChanged(const juce::StringArray& connectedIds);
        juce::String description(const juce::String& target) const;
        juce::String getStatus() const { return status; }
        int mappingCount() const { return (int)bindings.size(); }
        std::function<void()> onChange;

    private:
        struct Source
        {
            juce::String device;
            int channel = 0, number = 0;
            bool cc = false;
            bool operator<(const Source& other) const;
        };
        struct Binding
        {
            Source source;
            juce::String target, deviceName;
            Encoding encoding = Encoding::absolute;
            int previous = -1;
            bool down = false;
            bool pulse = false;
        };
        void changed(juce::String text);
        bool save();
        void release(Binding& binding);
        juce::PopupMenu targetMenu(const juce::String& target);
        static juce::String encodingName(Encoding encoding);
        juce::File preferences;
        std::map<juce::String, Target> targets;
        std::vector<juce::Component*> highlightControls;
        std::vector<Binding> bindings;
        std::set<Source> swallowedNotes, passedNotes, passedSwitches;
        std::set<juce::String> knownDevices, disconnectedDevices;
        juce::String armed, status = "Right-click a panel control to learn MIDI";
        Encoding armedEncoding = Encoding::absolute;
        bool learning = false, keepLearning = false, enabled = true;
        bool armedPulse = false;
        // Popup callbacks can outlive the simulator window.
        struct Lifetime {};
        std::shared_ptr<Lifetime> lifetime = std::make_shared<Lifetime>();
    };
}
