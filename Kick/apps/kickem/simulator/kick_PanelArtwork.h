#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace sim
{
    // The SVG is both the editable artwork and the source of interactive bounds.
    class PanelArtwork
    {
    public:
        static constexpr int width = 1360, height = 1350;
        PanelArtwork();
        void draw(juce::Graphics&) const;
        juce::Rectangle<int> bounds(const juce::String& id) const;
        bool isValid() const { return drawable != nullptr && regions.size() == 57; }

    private:
        std::unique_ptr<juce::Drawable> drawable;
        std::map<juce::String, juce::Rectangle<int>> regions;
    };
}
