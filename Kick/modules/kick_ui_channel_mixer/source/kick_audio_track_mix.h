#pragma once
namespace kick_ui
{
    class AudioTrackMixer : public kick_ui::IRowView
        , public mt::MTAudioInterfaceListener
    {
    public:
        AudioTrackMixer();
        ~AudioTrackMixer() = default;

        void populate( void );
        void knobCommand( int knob, int value );

    private:
        void setupProvider() override;
        void visibilityChanged() override;

        void onProgramListChanged() override;
        void meterUpdate() override;
        void updateProgramData();

        std::atomic_bool _initialized{ false };
        std::vector<int> audioData;
        int              numAudioPrograms{ 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AudioTrackMixer )
    };

    class AudioTrackDisplay : public juce::Component
    {
    public:
        std::function<void()> onSoloChanged;

        AudioTrackDisplay();
        ~AudioTrackDisplay() = default;

        void                 meterUpdate();
        void                 setAudioNumber( const int ID, const int ProgNo );
        void                 linkRotated( int ID, int value );
        juce::Rectangle<int> getElementBounds( int state );

    private:
        mt::MTAudioInterface&                   ae{ mt::getAudioInterface() };
        std::shared_ptr<mt::Kick::ScopeTracker> scope;
        mt::MTAudioProgram*                     pProg{ nullptr };

        int progNo{ -1 };

        ui::KickSerialNumber srNo{ "" };
        ui::KickImageButton  icon{ "Icon" };
        ui::KickLabel        audioName{ "" };
        ui::KickLabel        audioHeader{ "TRACK" };

        ui::KickMeterHorizantal levelMeter1;
        ui::KickMeterHorizantal levelMeter2;
        ui::KickLevelSlider     levelSlider;
        ui::KickPanRotary       panSlider;

        ui::KickListBox inputListBox{ "INPUT" };
        ui::KickListBox outputListBox{ "OUT" };

        ui::KickButton      inserts{ "Inserts", "Inserts" };
        ui::KickButton armButton{ "ARM" };
        ui::KickImageButton monitorButton{ "MONITOR", "Monitor_normal", "Monitor_active" };
        ui::KickImageButton soloButton{ "SOLO", "Solo_normal", "Solo_active" };
        ui::KickImageButton muteButton{ "MUTE", "Mute_normal", "Mute_active" };

        void resized() override;
        void clear();
        void updateData();
    };
}
