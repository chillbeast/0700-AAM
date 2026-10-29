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

#include "hardware/source/kick_Resorces.h"
#include "kick_ui_common/kick_ui_common.h"

namespace kick_ui
{
    inline static constexpr int rx = 198;
    inline static constexpr int ry = 20;
    inline static constexpr int rw = 604;
    inline static constexpr int rh = 525;

    KickModeHeader::KickModeHeader( const juce::String& Name, const juce::String& Image )
    {
        setTitle( Name );
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        icon.setImage( Image );

        mt::utils::addAndMakeVisible( this, icon, back, resources, metronome, automation, settings, newProject, headerLabel, playStop, autoLoad );
        mt::utils::disableMouseIntercepts( icon, headerLabel );
        headerLabel.setText( Name, juce::dontSendNotification );
        headerLabel.setValueFontHeight( 20 );
        headerLabel.setValueColour( ui::Theme::textColour( 1 ) );
        headerLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        back.onClick = [this]() {
            if( onBackButtonPressed )
                onBackButtonPressed( back.getToggleState() );
            back.setToggleState( false, juce::dontSendNotification );
        };

        metronome.onClick = [&]() {
            MetronomePopup::Instance().Show();
        };

        automation.onClick = [&] {
            signals::sigScreenClose(Screen::MenuProgramEdit);
            mt::utils::showNotification("Audio automation lanes are in Program Edit; global automation recording is not implemented");
        };

        settings.onClick = [&] {
            if( onSettingsClick )
                onSettingsClick();
        };

        playStop.onClick = [&]() {
            if( onPlayStopClick )
                onPlayStopClick( playStop.getToggleState() );
            else
            {
                // Play/Stop  Selected Sequence
            }
        };

        autoLoad.onClick = [&]() {
            if( onAutoLoadClick )
                onAutoLoadClick();
        };

        newProject.onClick = [&]() {
            if( onNewProjectClick )
                onNewProjectClick();
        };

        resources.onClick = [this] {
            auto& _sysResources = KSystemResources::Instance();
            _sysResources.start(); // Start the timer when the dialog is shown

            mt::utils::showDialog(
                _sysResources.getTitle(),
                &_sysResources,
                []( int ) {
                    // This lambda is called when the dialog is closed
                    KSystemResources::Instance().stop();
                },
                { rx, ry, rw, rh } );

            // Define the dialog callback for when the dialog is closed
            auto onDialogClose = [&_sysResources]( int ) {
                _sysResources.stop();
            };
        };

        ui::connectTheme( *this, [&]() {
            resources.setImages( "Resource" );
            metronome.setImages( "Metronome" );
            automation.setImages( "Automation_Normal" );
            settings.setImages( "Settings_Header" );
            newProject.setImages( "NewProject" );
            back.setImages( "Back", "Back_Active" );
            playStop.setImages( "Play/Stop", "Play_Grid", "Stop_Active" );
        } );
    }

    void KickModeHeader::showSettingsButton( bool Show )
    {
        bShowSettings = Show;
        // Perform layout again...
        resized();
    }

    void KickModeHeader::setPlayState( bool State, juce::NotificationType Notify )
    {
        playStop.setToggleState( State, Notify );
    }

    void KickModeHeader::showPlayButton( bool Show )
    {
        bShowPlay = Show;
        resized();
    }

    void KickModeHeader::showBackButton( bool Show )
    {
        bshowBackbutton = Show;
        resized();
    }
    void KickModeHeader::showAutoLoad( bool Show )
    {
        bAutoLoad = Show;
        resized();
    }

    void KickModeHeader::showNewProjectButton( bool Show )
    {
        bShowNewProject = Show;
        resized();
    }

    void KickModeHeader::showOptions( bool ShowMetronome, bool ShowAutomation )
    {
        bShowMetronome  = ShowMetronome;
        bShowAutomation = ShowAutomation;
        resized();
    }

    void KickModeHeader::setIcon( const juce::String& Image )
    {
        icon.setImage( Image );
    }

    void KickModeHeader::addComponent( Component* Child )
    {
        if( Child )
        {
            if( comp )
                removeChildComponent( comp );

            comp = Child;
            addChildComponent( comp );
            // Perform layout again...
            resized();
        }
    }

    void KickModeHeader::addButton( juce::Component* Button, int Width )
    {
        buttons.push_back( std::make_pair( Button, Width ) );
        addAndMakeVisible( Button );
        // Perform layout again...
        resized();
    };

    void KickModeHeader::removeButton( juce::Component* Button )
    {
        removeChildComponent( Button );
        // Search in the vector
        auto it = std::remove_if( buttons.begin(), buttons.end(), [Button]( std::pair<juce::Component*, int>& elem ) {
            return elem.first == Button;
        } );
        // Remove from the vector
        buttons.erase( it, buttons.end() );
        // Perform layout again...
        resized();
    }

    void KickModeHeader::removeAllButtons()
    {
        for( auto& it : buttons )
        {
            removeChildComponent( it.first );
        }
        buttons.clear();
        // Perform layout again...
        resized();
    }

    void KickModeHeader::setEnableNewProject( bool enable )
    {
        newProject.setVisible( enable );
    }

    void KickModeHeader::setAutoLoadToggleState( bool state, juce::NotificationType notification )
    {
        autoLoad.setToggleState( state, notification );
    }

    void KickModeHeader::setNewProjectToggleState( bool state, juce::NotificationType notification )
    {
        newProject.setToggleState( state, notification );
    }

    void KickModeHeader::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 2 ) );
    }

    void KickModeHeader::resized()
    {
        if( getLocalBounds().isEmpty() )
            return;

        back.setVisible( bshowBackbutton );
        if( !bshowBackbutton )
        {
            icon.setBoundsRelative( 0.020f, 0.1842f, 0.040f, 0.6316f );
            headerLabel.setBoundsRelative( 0.07f, .06f, .217f, .978f );
        }
        else
        {
            back.setBoundsRelative( 0.00f, 0.0f, 0.040f, 1.0f );
            icon.setBoundsRelative( 0.049f, 0.1842f, 0.040f, 0.6316f );
            headerLabel.setBoundsRelative( .097f, .21f, .217f, .578f );
        }
        resources.setBoundsRelative( 0.96f, 0.0f, 0.040f, 1.0f );

        auto x = iconX;

        if( bShowPlay )
        {
            x -= iconSize;
            playStop.setBoundsRelative( x, 0.1842f, 0.043f, 0.6316f );
        }
        if( bShowMetronome )
        {
            x -= iconSize;
            metronome.setBoundsRelative( x, 0.1842f, 0.043f, 0.6316f );
        }
        if( bShowAutomation )
        {
            x -= iconSize;
            automation.setBoundsRelative( x, 0.1842f, 0.043f, 0.6316f );
        }
        if( bShowSettings )
        {
            x -= iconSize;
            settings.setBoundsRelative( x, 0.1842f, 0.043f, 0.6316f );
        }
        if( bAutoLoad )
        {
            x -= iconSize;
            autoLoad.setBoundsRelative( x, 0.205f, 0.043f, 0.5531f );
        }
        if( bShowNewProject )
        {
            x -= newProjectSize;
            newProject.setBoundsRelative( x, 0.1842f, newProjectWidth, 0.6316f );
        }
        x -= buttonX;

        // User buttons...
        for( auto& it : buttons )
        {
            float buttonWidth;
            buttonWidth = ( (float)it.second / 1024.0f );
            x -= ( buttonWidth + buttonSpace );
            it.first->setBoundsRelative( x, 0.1842f, buttonWidth, 0.6316f );
        }

        // User components
        if( comp )
            comp->setBoundsRelative( 0.225f, 0.f, 0.40f, 1.f );
    }

    void KickModeHeader::visibilityChanged()
    {
        for( auto it : getChildren() )
        {
            it->setVisible( isVisible() );
            it->setWantsKeyboardFocus( false );
        }
    }

    //////////////////////////////////////
    //MetronomePopup
    //////////////////////////////////////
    // Static Methods
    void MetronomePopup::Show()
    {
        auto& metronomeInstance = MetronomePopup::Instance();
        mt::utils::showDialog(
            "METRONOME", &metronomeInstance, [&]( int R ) {},
            ui::layout::dialog, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    // Class methods...
    MetronomePopup::MetronomePopup()
    {
        level.setRange( MIDI::MIN, MIDI::MAX, 1 );
        level.showTitleAndValue( true, juce::Slider::NoTextBox, juce::Justification::centred );

        onOffButton.showTitle( true );
        countIn.setJustificationType( juce::Justification::centred );
        rate.setJustificationType( juce::Justification::centred );
        mode.setJustificationType( juce::Justification::centred );
        style.setJustificationType( juce::Justification::centred );

        countIn.setTitleColour( ui::Theme::textColour( 1 ) );
        rate.setTitleColour( ui::Theme::textColour( 1 ) );
        level.showTitleAndValue( true, juce::Slider::NoTextBox, juce::Justification::centred );
        level.setTitleJustification( juce::Justification::centredTop );
        level.setValueJustification( juce::Justification::centredBottom );

        mt::utils::addAndMakeVisible( this, onOffButton, level, mode, countIn, rate, style );

        level.onValueChanged = [&]( double Value ) {
            metronome.gain = juce::jlimit<float>( 0.f, 1.f, ( MIDI::SCALER * Value ) );
        };

        onOffButton.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            metronome.enabled = ( 1 == Index );
        };

        countIn.onSelectionChanged = [&]( int Index, juce::String ) {
            metronome.countIn = ( Index );
        };

        rate.onSelectionChanged = [&]( int Index, juce::String ) {
            metronome.rate = ( toEnum<mt::MTMetronomeRate>( Index ) );
        };

        mode.onSelectionChanged = [&]( int Index, juce::String ) {
            metronome.mode = ( toEnum<mt::MTMetronomeMode>( Index ) );
        };

        style.onSelectionChanged = [&]( int Index, juce::String ) {
            metronome.style = ( toEnum<mt::MTMetronomeStyle>( Index ) );
        };

        level.setValue( ( 127 * metronome.gain() ), juce::dontSendNotification );
        onOffButton.setSelected( metronome.enabled() ? 1 : 0, juce::dontSendNotification );
        countIn.setSelected( metronome.countIn(), juce::dontSendNotification );
        rate.setSelected( (int)metronome.rate(), juce::dontSendNotification );
        mode.setSelected( (int)metronome.mode(), juce::dontSendNotification );
        style.setSelected( (int)metronome.style(), juce::dontSendNotification );
    }

    void MetronomePopup::resized()
    {
        onOffButton.setBoundsRelative( 0.084f, 0.125f, 0.332f, 0.165f );
        level.setBoundsRelative( 0.084f, 0.417f, 0.332f, 0.165f );
        mode.setBoundsRelative( 0.084f, 0.708f, 0.332f, 0.165f );

        countIn.setBoundsRelative( 0.583f, 0.125f, 0.332f, 0.165f );
        rate.setBoundsRelative( 0.583f, 0.417f, 0.332f, 0.165f );
        style.setBoundsRelative( 0.583f, 0.708f, 0.332f, 0.165f );
    }

    void MetronomePopup::close()
    {
        if( auto dw = getCurrentlyModalComponent() )
            dw->exitModalState( 0 );
    }

    void MetronomePopup::linkRotated( int knob, double value )
    {
        switch( knob )
        {
            case 0: level.knobRotated( value ); break;
            case 1: rate.knobRotated( value ); break;
            case 2: onOffButton.knobRotated( value ); break;
        }
    }
}
