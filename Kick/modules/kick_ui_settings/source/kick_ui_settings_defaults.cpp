/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "kick_ui_settings_defaults.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  KickChannelMixer
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsPDefault::SettingsPDefault()
        : SettingsInterface( 5, 4, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "Startup Settings", "Auto Save", "Time Defaults", "Project Defaults", "Default Project Name" } );

        asEnabled.setToggleState( settings.projects.isAutoSave(), juce::dontSendNotification );
        asTimeout.setSelected( settings.projects.saveAfter(), juce::sendNotification );

        addComponent( ssLaunch, 0, 0 );
        addComponent( ssRecProjects, 0, 1 );

        addComponent( asEnabled, 1, 0 );
        addComponent( asTimeout, 1, 1 );

        addComponent( pdTempo, 2, 0 );
        addComponent( pdBars, 2, 1 );
        addComponent( pdNum, 2, 2 );
        addComponent( pdDen, 2, 3 );

        addComponent( pdRootNote, 3, 0 );
        addComponent( pdScale, 3, 1 );
        addComponent( pdSeq, 3, 2 );
        addComponent( pdTracks, 3, 3 );
        addComponent( bitDepath, 0, 2 );

        addComponent( enableList, 4, 0 );
        addComponent( defaultName, 4, 1 );

        ssLaunch.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.onLaunch = ( str );
            settings.projects.isAutoLoad = (str == "Last Used");
        };
        ssRecProjects.onValueChanged = [this]( int value ) {
            settings.projects.maxRecentProjects = ( value );
        };

        bitDepath.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.bitDepth = ( str );
        };

        asEnabled.onClick = [this]() {
            settings.projects.isAutoSave = ( asEnabled.getToggleState() );
        };
        asTimeout.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.saveAfter = ( str );
        };

        pdTempo.onValueChanged = [this]( int value ) {
            settings.projects.tempo = ( value );
        };

        pdBars.onValueChanged = [this]( int value ) {
            settings.projects.bars = ( value );
        };

        pdNum.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.numerator = ( str );
        };
        pdDen.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.denominator = ( str );
        };

        pdRootNote.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.rootNote = ( index );
        };

        pdScale.onSelectionChanged = [this]( int index, const juce::String& str ) {
            settings.projects.scale = ( index );
        };

        pdSeq.onValueChanged = [this]( int value ) {
            settings.projects.numSeq = ( value );
        };
        pdTracks.onValueChanged = [this]( int value ) {
            settings.projects.numTrackPerSeq = ( value );
        };
        enableList.onSelectionChanged = [this]( int index, const juce::String& ) {
            if( index == 0 )
            {
                defaultName.setEditable( false, false );
                settings.projects.defProjectState = ( 0 );
                settings.projects.namingStyle     = ( "DISABLE" );
                settings.projects.namePrefix      = ( "" );
                defaultName.setText( settings.projects.namePrefix(), juce::dontSendNotification );
            }
            else if( index == 1 )
            {
                juce::Time   currentTime = juce::Time::getCurrentTime();
                juce::String timestamp   = currentTime.formatted( "%d%b%y_%H%M%S_" ).toUpperCase();
                defaultName.setText( timestamp, juce::dontSendNotification );
                defaultName.setEditable( false, false );
                settings.projects.defProjectState = ( 1 );
                settings.projects.namingStyle     = ( "TIME STAMP" );
                settings.projects.namePrefix      = ( timestamp );
            }
            else if( index == 2 )
            {
                defaultName.setEditable( false, true );
                settings.projects.defProjectState = ( 2 );
                settings.projects.namingStyle     = ( "CUSTOM" );
                settings.projects.namePrefix      = ( "KICK_" );
                defaultName.setText( settings.projects.namePrefix(), juce::dontSendNotification );
            }
        };

        defaultName.setValueJustification( juce::Justification::centred );
        defaultName.setValueFontHeight( 20 );
        defaultName.onTextChange = [&]() {
            juce::String text            = defaultName.getText();
            settings.projects.namePrefix = ( text );
        };
    }

    void SettingsPDefault::refresh()
    {
        juce::String str = settings.projects.onLaunch();
        ssLaunch.setSelected( str, juce::dontSendNotification );

        ssRecProjects.setValue( settings.projects.maxRecentProjects(), juce::dontSendNotification );
        asEnabled.setToggleState( settings.projects.isAutoSave(), juce::dontSendNotification );
        asTimeout.setSelected( settings.projects.saveAfter(), juce::dontSendNotification );

        bitDepath.setSelected( settings.projects.bitDepth(), juce::dontSendNotification );
        pdBars.setValue( settings.projects.bars(), juce::dontSendNotification );
        pdTempo.setValue( settings.projects.tempo(), juce::dontSendNotification );

        pdNum.setSelected( settings.projects.numerator(), juce::dontSendNotification );
        pdDen.setSelected( settings.projects.denominator(), juce::dontSendNotification );

        pdRootNote.setSelected( settings.projects.rootNote(), juce::dontSendNotification );
        pdScale.setSelected( settings.projects.scale(), juce::dontSendNotification );

        pdSeq.setValue( settings.projects.numSeq(), juce::dontSendNotification );
        pdTracks.setValue( settings.projects.numTrackPerSeq(), juce::dontSendNotification );

        enableList.setSelected( settings.projects.namingStyle(), juce::dontSendNotification );
        defaultName.setText( settings.projects.namePrefix(), juce::dontSendNotification );
    }

    void SettingsPDefault::setupHeader( kick_ui::KickModeHeader& Hdr ) {}

    void SettingsPDefault::handleCommand( CPktMgr::HWMsg_t& ) {}
}
