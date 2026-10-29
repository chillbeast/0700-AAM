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

    DPLayerMix::DPLayerMix( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum )
        : pads( Pads ), pDrum( PDrum ), KickGrid( 4, 6 )
    {
        setRowTitles( { "LAYER 1", "LAYER 2", "LAYER 3", "LAYER 4" } );

        for( auto i = 0; i < 4; ++i )
        {
            semi[i].setTitle( "SEMI" );
            fine[i].setTitle( "FINE" );
            amp[i].setTitle( "LEVEL" );
            pan[i].setTitle( "PAN" );

            semi[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            fine[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            amp[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            pan[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );

            semi[i].setCentered( true );
            fine[i].setCentered( true );
            pan[i].setCentered( true );

            semi[i].setRange( SEMI::MIN, SEMI::MAX, SEMI::STEP );
            fine[i].setRange( FINE::MIN, FINE::MAX, FINE::STEP );
            amp[i].setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
            pan[i].setRange( PAN::MIN, PAN::MAX, PAN::STEP );

            semi[i].setDoubleClickReturnValue( true, 0, 0 );
            fine[i].setDoubleClickReturnValue( true, 0, 0 );
            amp[i].setDoubleClickReturnValue( true, 127, 0 );
            pan[i].setDoubleClickReturnValue( true, 0, 0 );

            addComponent( semi[i], i, 0 );
            addComponent( fine[i], i, 1 );
            addComponent( amp[i], i, 2 );
            addComponent( pan[i], i, 3 );
            velocityLow[i].setTitle("VEL LOW"); velocityHigh[i].setTitle("VEL HIGH");
            velocityLow[i].setRange(0, 127, 1); velocityHigh[i].setRange(0, 127, 1);
            velocityLow[i].setComponentID("layer-velocity-low-" + juce::String(i));
            velocityHigh[i].setComponentID("layer-velocity-high-" + juce::String(i));
            addComponent(velocityLow[i], i, 4); addComponent(velocityHigh[i], i, 5);
            velocityLow[i].onValueChanged = [&, i](double v) { if(pDrum && !pads.empty()) { pDrum->setLayerVelocityStart(pads[0], i, (int)v); refresh(); } };
            velocityHigh[i].onValueChanged = [&, i](double v) { if(pDrum && !pads.empty()) { pDrum->setLayerVelocityEnd(pads[0], i, (int)v); refresh(); } };


            semi[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setLayerSemi( pads[0], i, Value );
            };

            fine[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setLayerFine( pads[0], i, Value );
            };

            amp[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setLayerGain( pads[0], i, CommonUtils::midiToGain( Value ) );
            };

            pan[i].onValueChanged = [&, i]( double Value ) {
                if( pDrum )
                    pDrum->setLayerPan( pads[0], i, Value );
            };
        }
    }

    void DPLayerMix::refresh()
    {
        if( pDrum && pads.size() )
            refreshPads();
    }

    void DPLayerMix::refreshPads()
    {
        if( pDrum && pads.size() )
        {
            for( auto index = 0; index < 4; ++index )
            {
                semi[index].setValue( pDrum->getLayerSemi( pads[0], index ), juce::dontSendNotification );
                fine[index].setValue( pDrum->getLayerFine( pads[0], index ), juce::dontSendNotification );
                amp[index].setValue( CommonUtils::gainToMidi( pDrum->getLayerGain( pads[0], index ) ), juce::dontSendNotification );
                pan[index].setValue( pDrum->getLayerPan( pads[0], index ), juce::dontSendNotification );
                velocityLow[index].setValue(pDrum->getLayerVelocityStart(pads[0], index), juce::dontSendNotification);
                velocityHigh[index].setValue(pDrum->getLayerVelocityEnd(pads[0], index), juce::dontSendNotification);

            }
        }
    }

    void DPLayerMix::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(pads.empty()) return;
            pads[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void DPLayerMix::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
            refresh();
    }
}
