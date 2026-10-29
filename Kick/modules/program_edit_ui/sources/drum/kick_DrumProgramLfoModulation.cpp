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

namespace kick_ui
{

    DPLfoModulation::DPLfoModulation( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum )
        : pads( Pads ), pDrum( PDrum ), KickGrid( 4, 4 )
    {
        setRowTitles( { "SOURCE", "DESTINATION", "", "" } );

        addComponent( lfoType, 0, 0 );
        addComponent( rate, 0, 2 );
        enabled.setClickingTogglesState(true); enabled.setComponentID("lfo-tab-on"); addComponent(enabled, 0, 3);
        enabled.onClick = [&] { if(pDrum && !pads.empty()) pDrum->setLFOEnabled(pads[0], enabled.getToggleState()); };

        //addComponent( layer4, 1, 3 );

        addComponent( pitch, 1, 0 );
        addComponent( filter, 1, 1 );
        addComponent( amp, 1, 2 );
        addComponent( pan, 1, 3 );

        rate.setRange( 0.1, 200, 0.1 );

        pitch.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        filter.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        amp.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        pan.setRange( PAN::MIN, PAN::MAX, PAN::STEP );

        rate.setDoubleClickReturnValue( true, 1, 0 );
        pitch.setDoubleClickReturnValue( true, 0, 0 );
        filter.setDoubleClickReturnValue( true, 0, 0 );
        amp.setDoubleClickReturnValue( true, 0, 0 );
        pan.setDoubleClickReturnValue( true, 0, 0 );

        lfoType.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        lfoType.setJustificationType( juce::Justification::centred );

        lfoType.onSelectionChanged = [&]( int Index, const juce::String& ) {
            if( pDrum )
                pDrum->setLFOType( pads[0], Index );
        };

        pitch.onValueChanged = [&]( juce::int64 Value ) {
            if( pDrum )
                pDrum->setLFOPitch( pads[0], Value );
        };

        filter.onValueChanged = [&]( juce::int64 Value ) {
            if( pDrum )
                pDrum->setLFOFilter( pads[0], Value );
        };

        amp.onValueChanged = [&]( juce::int64 Value ) {
            if( pDrum )
                pDrum->setLFOLevel( pads[0], Value );
        };

        pan.onValueChanged = [&]( double Value ) {
            if( pDrum )
                pDrum->setLFOPan( pads[0], Value );
        };

        rate.onValueChanged = [&]( double Value ) {
            if( pDrum )
                pDrum->setLFOFrequency( pads[0], Value );
        };

        ui::connectTheme( *this, [&]() {
            num.setImages( "num_Header" );
        } );
    }

    void DPLfoModulation::refresh()
    {
        refreshPads();
    }

    void DPLfoModulation::refreshPads()
    {
        if( pDrum && pads.size() )
        {
            lfoType.setSelected( pDrum->getLFOType( pads[0] ), juce::dontSendNotification );
            enabled.setToggleState(pDrum->getLFOEnabled(pads[0]), juce::dontSendNotification);
            rate.setValue( pDrum->getLFOFrequency( pads[0] ), juce::dontSendNotification );
            pitch.setValue( pDrum->getLFOPitch( pads[0] ), juce::dontSendNotification );
            filter.setValue( pDrum->getLFOFilter( pads[0] ), juce::dontSendNotification );
            amp.setValue( pDrum->getLFOLevel( pads[0] ), juce::dontSendNotification );
            pan.setValue( pDrum->getLFOPan( pads[0] ), juce::dontSendNotification );
        }
    }

    void DPLfoModulation::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(pads.empty()) return;
            pads[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void DPLfoModulation::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
            refresh();
    }

    DrumGroupModulation::DrumGroupModulation( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum )
        : pads( Pads ), pDrum( PDrum )
    {
        mt::utils::addAndMakeVisible( this, source, destination );

        source.setValueColour( ui::Theme::textColour( 1 ) );
        destination.setValueColour( ui::Theme::textColour( 1 ) );

        source.setValueBackgroundColour( ui::Theme::backgroundColour( 1 ) );
        destination.setValueBackgroundColour( ui::Theme::backgroundColour( 1 ) );
        source.setPadding( 10 );
        destination.setPadding( 10 );

        for( auto id = 0; id < 4; ++id )
        {
            lfos[id]    = std::make_unique<ui::KickNumber>( "LFO", 1, 120, 1 );
            filters[id] = std::make_unique<ui::KickNumber>( "FILTER", 1, 120, 1 );
            envs[id]    = std::make_unique<ui::KickNumber>( "ENV", 1, 120, 1 );
            customs[id] = std::make_unique<ui::KickNumber>( "CUSTOM", 1, 120, 1 );

            addAndMakeVisible( lfos[id].get() );
            addAndMakeVisible( filters[id].get() );
            addAndMakeVisible( envs[id].get() );
            addAndMakeVisible( customs[id].get() );
            addAndMakeVisible( listBox[id] );

            lfos[id]->setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
            filters[id]->setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
            envs[id]->setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
            customs[id]->setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
            listBox[id].setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

            lfos[id]->setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
            filters[id]->setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
            envs[id]->setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
            customs[id]->setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

            listBox[id].setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 1 ) );
            listBox[id].setJustificationType( juce::Justification::centred );
            listBox[id].setDisplayFormat( ui::KickListBox::withoutTitleBar );
        }
    }

    void DrumGroupModulation::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 0, 98, 1004, 290 }, ui::Theme::backgroundColour( 1 ) );
    }

    void DrumGroupModulation::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(pads.empty()) return;
            pads[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void DrumGroupModulation::resized()
    {
        source.setBoundsRelative( .012f, .064f, .159f, .114f );
        destination.setBoundsRelative( .012f, .572f, .159f, .114f );

        auto x = .23f;
        for( auto id = 0; id < 4; ++id )
        {
            listBox[id].setBoundsRelative( x, .064f, .12f, .114f );
            lfos[id]->setBoundsRelative( x, .266f, .12f, .165f );
            filters[id]->setBoundsRelative( x, .448f, .12f, .165f );
            envs[id]->setBoundsRelative( x, .63f, .12f, .165f );
            customs[id]->setBoundsRelative( x, .81f, .12f, .165f );
            x += .2;
        }
    }
}
