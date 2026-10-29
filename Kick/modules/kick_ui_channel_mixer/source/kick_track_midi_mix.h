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

#pragma once

namespace kick_ui
{
    struct TrackSoloMuteData
    {
        bool solo = false;
        bool mute = false;
    };
    class TrackMidiMixer : public kick_ui::IRowView
    {
    public:
        TrackMidiMixer();
        ~TrackMidiMixer() = default;

        void                               populate();
        void                               knobCommand( int knob, int value );
        void                               refreshSoloMute();
        std::array<TrackSoloMuteData, 128> soloMuteData;

    private:
        sigslot::scoped_connection midiInputsConnection;
        void                             setupProvider() override;
        void                             meterUpdate() override;
        void                             visibilityChanged() override;
        const std::vector<juce::String>& _midiInPorts{ mt::getAudioInterface().getMidiDeviceNames( true ) };

        //==============================================================================
        mt::MTBusManager& _busMgr{ ae.getBusManager() };
        std::atomic_bool  _initialized{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( TrackMidiMixer )
    };

    class TrackMidiMixerDisplay : public juce::Component
    {
    public:
        std::function<void()> onSoloMuteChanged;
        std::function<void()> onRouteChanged;
        TrackMidiMixerDisplay();
        ~TrackMidiMixerDisplay() = default;

        void                 setNumber( int ProgNo );
        void                 updateData();
        void                 setMidiPorts( const std::vector<juce::String>& MidiPorts );
        void                 meterUpdate();
        void                 setSendToList( const juce::Array<juce::String>& );
        void                 setSoloToggleButton( bool, bool );
        void                 setMuteToggleButton( bool, bool );
        void                 setData( TrackSoloMuteData* data );
        inline int           getNumber( void ) { return no; }
        void                 linkRotated( int ID, int value );
        juce::Rectangle<int> getElementBounds( int state );
        bool                 getMuteFromEngine() { return pTrack->mute; }
        bool                 getMuteDueToSoloFromEngine() { return pTrack->muteDueToSolo; }

    private:
        mt::MTAudioInterface&                   ae{ mt::getAudioInterface() };
        mt::MTITrackData*                       pTrack{ nullptr };
        std::shared_ptr<mt::Kick::ScopeTracker> scope;
        TrackSoloMuteData*                      trackData{ nullptr };
        int                                     no{ -1 };
        int                                     seq{ -1 };
        bool                                    muteDueToSolo{ false };

        ui::KickSerialNumber    srNo{ "" };
        ui::Kick2LineNameLabel  trackName{ "" };
        ui::KickLabel           trackHeader{ "MIDI TRACK" };
        ui::KickImageButton     icon{ "Icon" };
        ui::KickMeterHorizantal levelMeter;
        ui::KickButton          armButton{ "ARM" };
        ui::KickImageButton     monitorButton{ "MONITOR", "Monitor_normal", "Monitor_active" };
        ui::KickImageButton     soloButton{ "SOLO", "Solo_normal", "Solo_active" };
        ui::KickImageButton     muteButton{ "MUTE", "Mute_normal", "Mute_active" };
        ui::KickListBox         midiInput{ "MIDI INPUT", { "All" }, 0, 2 };
        ui::KickListBox         midiCh{ ui::KickListBox::withTitleBar, " MIDI CHANNEL", mt::Kick::midiChannels, 0, 4 };
        ui::KickListBox         sendTo{ "SEND TO", { "None" }, 0 };
        ui::KickLevelSlider     levelSlider;

        void clear();
        void resized() override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( TrackMidiMixerDisplay )
    };

}
