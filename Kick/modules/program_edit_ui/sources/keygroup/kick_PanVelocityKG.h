namespace kick_ui
{
    class PanVelocityEditKG : public juce::Component
    {
    public:
        PanVelocityEditKG();
        ~PanVelocityEditKG();
        void resized() override;

    protected:
        ui::KickGrid    rcLayout{ 4, 2, true };
        ui::KickListBox layerOne{ "L1", 1 };
        ui::KickListBox layerTwo{ "L2", 1 };
        ui::KickListBox layerThree{ "L3", 1 };
        ui::KickListBox layerFour{ "L4", 1 };

        ui::KickRotary  layerOnePan{ "Semi", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerOneVelocityStart{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerOneVelocityEnd{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickListBox layerOneRoot{ "SMP", 1 };

        ui::KickRotary  layerTwoPan{ "Semi", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerTwoVelocityStart{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerTwoVelocityEnd{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickListBox layerTwoRoot{ "SMP", 1 };

        ui::KickRotary  layerThreePan{ "Semi", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerThreeVelocityStart{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerThreeVelocityEnd{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickListBox layerThreeRoot{ "SMP", 1 };

        ui::KickRotary  layerFourPan{ "Semi", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerFourVelocityStart{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  layerFourVelocityEnd{ "Volume", juce::Slider::TextBoxLeft, true };
        ui::KickListBox layerFourRoot{ "SMP", 1 };
    };

    class KGLayerMix : public ui::KickGrid
        , public KeyGroupInterface
    {
    public:
        KGLayerMix( std::vector<int>& Keys, mt::MTKeygroupProgram*& pKeyGroup );
        ~KGLayerMix() = default;

        void refresh() override;
        void refreshKeys();

    private:
        std::vector<int>&       keys;
        mt::MTKeygroupProgram*& pKeygroup;

        std::array<ui::KickRotary, 4> semi;
        std::array<ui::KickRotary, 4> fine;
        std::array<ui::KickRotary, 4> amp;
        std::array<ui::KickRotary, 4> pan;
        std::array<ui::KickRotary, 4> velocityLow, velocityHigh;


        void visibilityChanged() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
    };

    class KeyGroupInserts : public juce::Component
        , public KeyGroupInterface
    {
    public:
        KeyGroupInserts( mt::MTKeygroupProgram*& pKeyGroup );
        ~KeyGroupInserts() = default;

        void refresh() override;

    private:
        ui::KickLabel insert1{ "Insert 01" }, insert2{ "Insert 02" }, insert3{ "Insert 03" }, insert4{ "Insert 04" };
        ui::KickLabel insert1Label{ "" };
        ui::KickLabel insert2Label{ "" };
        ui::KickLabel insert3Label{ "" };
        ui::KickLabel insert4Label{ "" };

        ui::KickListBox         type{ "TYPE" };
        mt::MTKeygroupProgram*& pKeygroup;

        ui::KickRotary cutoff{ "CUTOFF", juce::Slider::TextBoxLeft, true };
        ui::KickRotary resonance{ "RESONANCE", juce::Slider::TextBoxLeft, true };
        ui::KickRotary dryWet1{ "DRY/WET", juce::Slider::TextBoxLeft, true };
        ui::KickRotary time1{ "TIME", juce::Slider::TextBoxLeft, true };
        ui::KickRotary feedBack{ "FEEDBACK", juce::Slider::TextBoxLeft, true };
        ui::KickRotary width1{ "WIDTH", juce::Slider::TextBoxLeft, true };
        ui::KickRotary dryWet2{ "DRY/WET", juce::Slider::TextBoxLeft, true };
        ui::KickRotary time2{ "TIME", juce::Slider::TextBoxLeft, true };
        ui::KickRotary ratio{ "RATIO", juce::Slider::TextBoxLeft, true };
        ui::KickRotary width2{ "WIDTH", juce::Slider::TextBoxLeft, true };
        ui::KickRotary dryWet3{ "DRY/WET", juce::Slider::TextBoxLeft, true };
        ui::KickRotary rotary1{ "-", juce::Slider::TextBoxLeft, true };
        ui::KickRotary rotary2{ "-", juce::Slider::TextBoxLeft, true };
        ui::KickRotary rotary3{ "-", juce::Slider::TextBoxLeft, true };
        ui::KickRotary rotary4{ "-", juce::Slider::TextBoxLeft, true };

        ui::KickButton edit1{ "E", "E" };
        ui::KickButton edit2{ "E", "E" };
        ui::KickButton edit3{ "E", "E" };
        ui::KickButton edit4{ "E", "E" };
        ui::KickButton del1{ "D", "D" };
        ui::KickButton del2{ "D", "D" };
        ui::KickButton del3{ "D", "D" };
        ui::KickButton del4{ "D", "D" };

        void deleteInsert( unsigned int Position );
        void paint( juce::Graphics& ) override;
        void resized() override;
        void visibilityChanged() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
    };
}
