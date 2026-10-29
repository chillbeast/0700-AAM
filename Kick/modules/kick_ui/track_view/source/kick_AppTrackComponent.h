/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

class AppTrackDisplay : public juce::Component
    , public juce::AsyncUpdater
{
public:
    std::function<void()> onSoloMuteChange;
    AppTrackDisplay( int Seq = 0, int Track = 0 );
    ~AppTrackDisplay();

    std::function<void( int )> onTrackChange;

    std::function<void( juce::String& )> onNameChange;
    std::function<void( int )>           onProgramChange;
    std::function<void( int )>           onLengthChange;
    std::function<void( int )>           onTransposeChange;
    std::function<void( bool )>          onVelocityChange;
    std::function<void( bool )>          onMuteChange;
    std::function<void( bool )>          onSoloChange;

    void setTrack( int Seq, int Track );
    int  getSeqNo( void ) const { return seq; };
    int  getTrackNo( void ) const { return track; };
    void refreshSoloMute();

    void                 setMidiPorts( const std::vector<juce::String>* MidiPorts );
    void                 linkRotated( int ID, int Value );
    void                 updateMeter();
    ui::KickNumberT<int> length{ "Length", 0, 100, 4, 1, true };
    ui::KickRotary       transpose{ "TRANSPOSE", juce::Slider::TextBoxBelow, true };
    ui::KickRotary       level{ "LEVEL", juce::Slider::TextBoxBelow, true };
    ui::KickListBox      midiPort{ ui::KickListBox::withTitleBar, "I/P MIDI PORT", { "All" }, 0, 2 };
    ui::KickListBox      inputmidi{ ui::KickListBox::withTitleBar, "I/P MIDI CH", mt::Kick::midiChannels, 1, 3 };

protected:
    void sendPercentValueToHardware( float value );

private:
    void updateThemeImages( bool loadOrUnload );
    void refreshRecordingControls();
    void mouseDown( const juce::MouseEvent& e ) override;

    mt::MTAudioInterface&                   ae{ mt::getAudioInterface() };
    mt::MTITrackData*                       pTrack{ nullptr };
    std::shared_ptr<mt::Kick::ScopeTracker> scope;
    std::shared_ptr<mt::Kick::ScopeTracker> recordingScope;

    int                    seq{ -1 };
    int                    track{ -1 };
    juce::String           selMidiPort{ "" };
    const mt::MTMeterData* trkMeter{nullptr};

private:
    ui::Kick2LineNameLabel                  name{ "" };
    ui::KickImageButton                     mute{ TRANS( "Mute" ) };
    ui::KickImageButton                     solo{ TRANS( "Solo" ) };
    ui::KickButton                          arm{"ARM"};
    ui::KickImageButton                     monitor{ TRANS( "Monitor" ) };
    ui::KickMeterVerticalWithMarks          levelMeter;
    kick_ui::KickProgramSelectorWithDisplay selector{ false };

    bool soloData{ false };
    bool muteData{ false };
    bool muteDueToSoloData{ false };

    void resized() override;
    void handleAsyncUpdate() override;
    void visibilityChanged() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppTrackDisplay )
};

class AppTrackComponent : public juce::Component
    , public mt::MTAudioInterfaceListener
{
public:
    std::function<void()> onSoloMuteChange;

    AppTrackComponent( int Seq, int Track, juce::String name = "" );
    ~AppTrackComponent();

    inline void updateData() { track.triggerAsyncUpdate(); };
    inline void updateMeter() { track.updateMeter(); }
    inline void linkRotated( int ID, int Value ) { track.linkRotated( ID, Value ); };

    void                  setTrack( int Seq, int Track );
    int                   getSeqNo( void ) { return track.getSeqNo(); }
    int                   getTrackNo( void ) { return track.getTrackNo(); }
    void                  refreshSoloMute() { track.refreshSoloMute(); }
    void                  setMidiPorts( const std::vector<juce::String>* MidiPorts );
    std::function<void()> onTrackEdit;

private:
    ui::KickSerialNumber srNo{ "" };
    AppTrackDisplay      track{ -1, -1 };
    ui::KickImageButton  options{ "Options" };
    int                  trackNo;
    void                 resized() override;

    void mouseUp( const juce::MouseEvent& e ) override;
    void visibilityChanged() override;
    void updateThemeImages( bool loadOrUnload );

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppTrackComponent )
};

class AppTrackHeader : public juce::TableHeaderComponent
{
public:
    AppTrackHeader();
    ~AppTrackHeader() = default;

private:
    ui::KickLabel srNo{ "Sr." }, track{ "Track" }, prog{ "Prog" }, midiIn{ "Midi In" }, len{ "Length" }, trans{ "Trans" }, vel{ "Velocity" }, opts{ "Options" };
    void          resized();
};
