#pragma once
namespace kick_ui
{
    class LfoModulationKG : public juce::Component
    {
    public:
        LfoModulationKG();
        ~LfoModulationKG();
        void resized();

    private:
        ui::KickGrid  layout{ 4, 1 };
        ui::KickLabel vsLabel{ TRANS( "Velocity Sensitivity" ) };
        ui::KickLabel lfoLabel{ TRANS( "LFO" ) };
        ui::KickLabel dstLabel{ TRANS( "Destination" ) };
        ui::KickLabel ctrlLabel{ TRANS( "Controller Mod" ) };

        ui::KickSlider pitchVS{ "Pitch", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider attackVS{ "Attack", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider ampVS{ "Amp", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider panVS{ "Pan", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        ui::KickListBox waveLfo{ "Wave", 1 };
        ui::KickSlider  rateLfo{ "Rate", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickListBox syncLfo{ "Sync", 1 };

        ui::KickSlider pitchDs{ "Pitch", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider filterDs{ "Filter", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider ampDs{ "Amp", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider panDs{ "Pan", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        ui::KickSlider pitchCm{ "Pitch", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider pitchBendCm{ "Pitch Bend", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider wheelCm{ "Wheel>LFO", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider aftDsCm{ "AFT>FLT", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
    };

    class KeyGroupLFO : public ui::KickGrid
        , public KeyGroupInterface
    {
    public:
        KeyGroupLFO( std::vector<int>& Keys, mt::MTKeygroupProgram*& PKeyGroup );
        ~KeyGroupLFO() = default;

        void refresh() override;
        void refreshKeys();

    private:
        ui::KickButton enabled{"LFO ON"};

        std::vector<int>&       keys;
        mt::MTKeygroupProgram*& pKeyGroup;

        ui::KickListBox                 lfoType{ "WAVEFORM", { "SINE", "TRIANGLE", "SAW_UP", "SAW_DOWN", "SQUARE", "NOISE", "SAMPLE_HOLD" }, 0, 1 };
        ui::KickRotaryWithAttachedValue rate{ "RATE", juce::Slider::TextBoxAbove, true };

        ui::KickRotaryWithAttachedValue pitch{ "PITCH", juce::Slider::TextBoxAbove, true };
        ui::KickRotaryWithAttachedValue filter{ "FILTER", juce::Slider::TextBoxAbove, true };
        ui::KickRotaryWithAttachedValue amp{ "AMP", juce::Slider::TextBoxAbove, true };
        ui::KickRotaryWithAttachedValue pan{ "PAN", juce::Slider::TextBoxAbove, true };

        void visibilityChanged() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
    };

    class KeyGroupModulation : public juce::Component
        , public KeyGroupInterface
    {
    public:
        KeyGroupModulation();
        ~KeyGroupModulation() = default;

        void refresh() override {}

    private:
        ui::KickLabel source{ "Source" }, destination{ "Destination" };

        std::array<ui::KickListBox, 4> listBox;

        std::unique_ptr<ui::KickNumber> lfos[4];
        std::unique_ptr<ui::KickNumber> filters[4];
        std::unique_ptr<ui::KickNumber> envs[4];
        std::unique_ptr<ui::KickNumber> customs[4];

        void paint( juce::Graphics& ) override;
        void resized() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
    };
}
