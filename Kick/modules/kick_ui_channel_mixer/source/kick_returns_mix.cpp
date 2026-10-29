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
    RetrunsMixer::RetrunsMixer()
        : kick_ui::IRowView( "Program View" )
    {
        setupProvider();
        setMargin( 0, 6, 2, 0 );
    }

    void RetrunsMixer::setupProvider()
    {
        provider.numRows = [&]() -> int {
            return _busMgr.getNumReturns();
        };

        provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            auto maxItems = _busMgr.getNumReturns();
            if( juce::isPositiveAndBelow( Row, maxItems ) )
            {
                auto comp = dynamic_cast<ReturnsMixerDisplay*>( Existing );
                if( comp == nullptr )
                {
                    comp                = new ReturnsMixerDisplay();
                    comp->onSoloChanged = [&]() { refresh(); };
                }
                comp->setBusNumber( Row );
                return comp;
            }
            return nullptr;
        };
        _initialized = true;
    }

    void RetrunsMixer::populate( void )
    {
        if( !_initialized.load() )
            return;

        refresh();
    }

    void RetrunsMixer::knobCommand( int knob, int value )
    {
        auto&                             rowComp       = getRowComponents();
        auto                              comp1         = dynamic_cast<ReturnsMixerDisplay*>( rowComp[0] );
        auto                              elementBounds = comp1->getElementBounds( ( knob - 1 ) % 4 + 1 );
        auto                              numRows       = getNumRows();
        auto                              rows          = getRowNumbers( 3, ( ( knob - 1 ) / 4 ) + 1, elementBounds.getX(), elementBounds.getWidth(), 128 );
        std::vector<ReturnsMixerDisplay*> entry;

        for( auto i = 0; i < rows.size(); i++ )
        {
            int rowIndex = rows[i];
            if( rowIndex < numRows )
            {
                auto comp = dynamic_cast<ReturnsMixerDisplay*>( rowComp[rowIndex] );
                entry.push_back( comp );
            }
        }

        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
            case contracts::DeviceControlContracts::EN_KNOB_2:
            case contracts::DeviceControlContracts::EN_KNOB_3:
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( entry.size() > 0 && entry[0] ) entry[0]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_1 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_5:
            case contracts::DeviceControlContracts::EN_KNOB_6:
            case contracts::DeviceControlContracts::EN_KNOB_7:
            case contracts::DeviceControlContracts::EN_KNOB_8:
                if( entry.size() > 1 && entry[1] ) entry[1]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_5 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_9:
            case contracts::DeviceControlContracts::EN_KNOB_10:
            case contracts::DeviceControlContracts::EN_KNOB_11:
            case contracts::DeviceControlContracts::EN_KNOB_12:
                if( entry.size() > 2 && entry[2] ) entry[2]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_9 ), value );
                break;
        }
    }

    //==============================================================================
    void RetrunsMixer::visibilityChanged()
    {
        IRowView::visibilityChanged();

        auto  visible = isVisible();
        auto& comps   = getRowComponents();
        auto& names   = _busMgr.getReturnBusNames();
        for( auto& it : names )
        {
            _busMgr.enableInMetering( it, visible );
            _busMgr.enableOutMetering( it, visible );
        }
    }

    void RetrunsMixer::meterUpdate()
    {
        auto& comps = getRowComponents();
        for( auto& it : comps )
        {
            if( auto r = dynamic_cast<ReturnsMixerDisplay*>( it ) )
            {
                r->meterUpdate();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////    Row content - ReturnsMixerDisplay
    //////////////////////////////////////////////////////////////////////////////////////////
    ReturnsMixerDisplay::ReturnsMixerDisplay()
    {
        std::vector<juce::String> busNames;
        auto&                     outBusNames = busMgr.getOutputBusNames();
        for( auto& n : outBusNames )
            busNames.push_back( n );

        ioListBox.setListOptions( busNames );
        returnName.setEditable( false );

        mt::utils::addChildComponents( this, srNo, icon, returnName, returnHeader, ioListBox, levelMeter1, levelMeter2, programInserts, solo, mute, levelSlider, panSlider );

        srNo.setFontProperties( 36 );
        srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 6 ) );
        icon.toFront( true );

        returnName.setValueFontHeight( 18 );
        returnName.setValueBackgroundColour( ui::Theme::textColour( 4 ) );
        returnName.setPadding( 10 );
        returnHeader.setValueJustification( juce::Justification::topLeft );
        returnHeader.setValueColour( ui::Theme::textColour( 1 ) );
        returnHeader.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        ioListBox.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        ioListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        ioListBox.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour( 9 ) );
        ioListBox.setTitleFontHeight( 16 );

        programInserts.setValueColour( ui::Theme::textColour( 1 ) );
        programInserts.setValueFontHeight( 18 );

        panSlider.onValueChanged     = [&]( double Value ) { if( pBus) pBus->pan = Value; };
        levelSlider.onValueChanged   = [&]( double Value ) { if( pBus) pBus->gain = CommonUtils::midiToGain(Value); };
        mute.onClick                 = [&]() { if( pBus) pBus->mute = mute.getToggleState() ; };
        ioListBox.onSelectionChanged = [&]( int, const juce::String& Out ) { if( pBus ) pBus->routedTo = Out; };
        programInserts.onClick       = [&]() {
            if( pBus )
            {
                kick_ui::signals::onScreenData( Screen::Inserts, (void*)( pBus->getInsertsInterface() ) );
                kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
            }
        };

        solo.onClick = [&]() {
            if(pBus) pBus->solo = solo.getToggleState();

            if( onSoloChanged )
                onSoloChanged();
        };

        ui::connectTheme( *this, [&]() {
            solo.setImages( "Solo_normal", "Solo_active" );
            mute.setImages( "Mute_normal", "Mute_active" );
            updateData();
        } );
    };

    void ReturnsMixerDisplay::setBusNumber( int BusNo )
    {
        busNo = BusNo;
        pBus  = busMgr.getReturns( busNo );

        auto ioValueHandler    = [&]() { ioListBox.setSelected( pBus ? pBus->routedTo.get() : "", juce::dontSendNotification ); };
        auto muteValueHandler  = [&]() { mute.setToggleState( pBus ? pBus->mute.get() : false, juce::dontSendNotification ); };
        auto soloValueHandler = [&]() { solo.setToggleState(pBus ? pBus->solo.get() : false, juce::dontSendNotification); };
        auto panValueHandler   = [&]() { panSlider.setValue( pBus ? (double)pBus->pan.get() : 0.0, juce::dontSendNotification ); };
        auto levelValueHandler = [&]() { levelSlider.setValue( CommonUtils::gainToMidi( pBus ? (double)pBus->gain.get() : 0.0 ), juce::dontSendNotification ); };

        scope.reset();
        if( pBus )
        {
            scope = std::make_shared<mt::Kick::ScopeTracker>();

            pBus->routedTo.onValueChange.connect( ioValueHandler, scope );
            pBus->solo.onValueChange.connect(soloValueHandler, scope);
            pBus->mute.onValueChange.connect( muteValueHandler, scope );
            pBus->pan.onValueChange.connect( panValueHandler, scope );
            pBus->gain.onValueChange.connect( levelValueHandler, scope );
        }

        updateData();
    }

    void ReturnsMixerDisplay::updateData()
    {
        setProgramIcon( icon, mt::MTProgramType::Unknown );

        auto srNoValueHandler = [&]() { srNo.setText( juce::String( busNo + 1 ) ); };
        auto nameValueHandler = [&]() { returnName.setText( pBus ? pBus->name : "", juce::dontSendNotification ); };

        // Trigger data update
        srNoValueHandler();
        nameValueHandler();

        if( pBus )
        {
            pBus->routedTo.onValueChange();
            pBus->solo.onValueChange();
            pBus->mute.onValueChange();
            pBus->pan.onValueChange();
            pBus->gain.onValueChange();
        }
    }

    void ReturnsMixerDisplay::clear()
    {
        busNo = -1;
        returnName.setText( "", juce::dontSendNotification );
        levelSlider.setValue( 0.0, juce::dontSendNotification );
        panSlider.setValue( 0.0, juce::dontSendNotification );
        solo.setToggleState( false, juce::dontSendNotification );
        mute.setToggleState( false, juce::dontSendNotification );
        ioListBox.setSelected( 0, juce::dontSendNotification );
    }

    void ReturnsMixerDisplay::resized()
    {
        srNo.setBoundsRelative( .001f, .0f, .044f, .982f );
        icon.setBoundsRelative( .001f, .491f, .044f, .491f );
        returnHeader.setBoundsRelative( .065f, .0734f, .1603f, .2201f );
        returnName.setBoundsRelative( .065f, .267f, .1603f, .404f );
        levelMeter1.setBoundsRelative( .0651f, .68f, .1603f, .2935f );
        levelMeter2.setBoundsRelative( .4346f, .68f, .1607f, .2935f );
        programInserts.setBoundsRelative( .2594f, .3035f, .099f, .3928f );
        ioListBox.setBoundsRelative( .4346f, .0892f, .1653f, .5714f );
        levelSlider.setBoundsRelative( .6198f, .0892f, .1653f, .8214f );
        panSlider.setBoundsRelative( .8049f, .0892f, .0792f, .8214f );
        solo.setBoundsRelative( .9108f, .0892f, .0594f, .3928f );
        mute.setBoundsRelative( .9108f, .5178f, .0594f, .3928f );
    }

    void ReturnsMixerDisplay::meterUpdate()
    {
        auto& in  = pBus ? pBus->readInMeter() : AudIO::_emptyMeterData;
        auto& out = pBus ? pBus->readOutMeter() : AudIO::_emptyMeterData;
        levelMeter1.setSignalStrength( in[0].rms, in[1].rms, in[0].peak, in[0].peak );
        levelMeter2.setSignalStrength( out[0].rms, out[1].rms, out[1].peak, out[1].peak );
    }

    void ReturnsMixerDisplay::mouseUp( const juce::MouseEvent& e )
    {
        if( e.mouseWasClicked() )
        {
            updateData();
        }
    }

    void ReturnsMixerDisplay::visibilityChanged()
    {
        bool visible = isVisible();

        for( auto child : getChildren() )
            child->setVisible( visible );

        if( visible )
            updateData();
    }

    void ReturnsMixerDisplay::linkRotated( int ID, int value )
    {
        switch( ID )
        {
            case 0: ioListBox.knobRotated( value ); break;
            case 1: levelSlider.knobRotated( value ); break;
            case 2: panSlider.knobRotated( value ); break;
            case 3:
                if(pBus && value != 0) { pBus->mute = value > 0; pBus->solo = value < 0; }
                break;
        }
    }

    juce::Rectangle<int> ReturnsMixerDisplay::getElementBounds( int state )
    {
        switch( state )
        {
            case 1: return ioListBox.getBounds(); break;
            case 2: return levelSlider.getBounds(); break;
            case 3: return panSlider.getBounds(); break;
            case 4: return mute.getBounds(); break;
            default: return ioListBox.getBounds(); break;
        }
    }
}
