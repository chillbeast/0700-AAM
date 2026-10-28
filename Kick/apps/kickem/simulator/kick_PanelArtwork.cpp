#include "kick_PanelArtwork.h"
#include <KickPanelData.h>

namespace sim
{
    PanelArtwork::PanelArtwork()
    {
        auto xml = juce::parseXML(juce::String::fromUTF8(KickPanelData::kickpanel_svg, KickPanelData::kickpanel_svgSize));
        if(!xml) return;
        drawable = juce::Drawable::createFromSVG(*xml);
        if(auto* hits = xml->getChildByAttribute("id", "hit-regions"))
            for(auto* hit : hits->getChildIterator())
            {
                const juce::Rectangle<float> rect((float)hit->getDoubleAttribute("x"), (float)hit->getDoubleAttribute("y"),
                    (float)hit->getDoubleAttribute("width"), (float)hit->getDoubleAttribute("height"));
                regions.emplace(hit->getStringAttribute("id"), (rect * 2.f).toNearestInt());
            }
    }

    void PanelArtwork::draw(juce::Graphics& g) const
    {
        if(drawable)
            drawable->drawWithin(g, {0.f, 0.f, (float)width, (float)height}, juce::RectanglePlacement::stretchToFit, 1.f);
    }

    juce::Rectangle<int> PanelArtwork::bounds(const juce::String& id) const
    {
        const auto found = regions.find(id);
        jassert(found != regions.end());
        return found != regions.end() ? found->second : juce::Rectangle<int>();
    }
}
