/*******************************************************************************
  The block below describes the properties of this module, and is read by
  the Projucer to automatically generate project code that uses it.
  For details about the syntax and how to create or use a module, see the
  JUCE Module Format.txt file.

  BEGIN_JUCE_MODULE_DECLARATION

   ID:                 kick_leds
   vendor:             MusicTribe
   version:            0.1.0
   name:               MusicTribe ui
   description:        Classes aand interfaces for DAW
   website:            http://www.musictribe.com/
   license:            AGPLv3

   dependencies:       audio_engine, kick_ui_components

  END_JUCE_MODULE_DECLARATION

 *******************************************************************************/

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

#include <audio_engine/audio_engine.h>
#include <kick_ui_components/kick_ui_components.h>
#include <unordered_set>

namespace kick_ui
{
    class KickLEDs : private mt::MTAudioInterfaceListener
        , private juce::Timer
    {
    public:
        KickLEDs();
        ~KickLEDs();

    private:
        void onSequenceSelectionChange( const mt::MTSelected& /*Selected*/ ) override;
        void onTrackSelectionChange( const mt::MTSelected& /*Selected*/ ) override;
        void onProgramSelectionChange( const mt::MTSelected& /*Selected*/ ) override;
        void onTrackProgramChanged( int Track, int Program ) override;
        void onProjectLoaded( juce::File ) override;
        void onProjectClose() override;
        void onProgramListChanged() override;
        void changePadColours( const juce::String& Program, const mt::MTProgramType Type );
        void sendPadSettings();
        void timerCallback() override;
        std::map<int, bool> performanceLEDs;

    private:
        mt::MTAudioInterface&               ae{ mt::getAudioInterface() };
        mt::KickSettings&                   settings{ mt::getSettings() };
        bool                                projectLoadCompleted{ false };
        Screen                              currentScreen  = Screen::Invalid;
        std::unordered_set<kick_ui::Screen> invalidScreens = {
            kick_ui::Screen::MenuSampler,
            kick_ui::Screen::MenuAssignSample,
            kick_ui::Screen::HomeSequencerView,
            kick_ui::Screen::MenuTrackMute,
            kick_ui::Screen::MenuPadMute
        };
    };
}
