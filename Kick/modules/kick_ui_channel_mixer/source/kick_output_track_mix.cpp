#include "kick_output_track_mix.h"

namespace kick_ui
{
    OutputTrackMixer::OutputTrackMixer()
        : kick_ui::IRowView( "OUTPUT TRACK" )
    {
        setupProvider();
        setMargin( 0, 6, 2, 0 );
    }

    void OutputTrackMixer::setupProvider()
    {
        provider.numRows = [&]() -> int {
            return _busMgr.getNumOuts();
        };

        provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            auto maxItems = _busMgr.getNumOuts();
            if( juce::isPositiveAndBelow( Row, maxItems ) )
            {
                auto comp = dynamic_cast<OutputTrackDisplay*>( Existing );
                if( comp == nullptr )
                {
                    comp                = new OutputTrackDisplay();
                    comp->onSoloChanged = [&]() { refresh(); };
                }
                comp->setOutputNumber( Row );
                return comp;
            }
            return nullptr;
        };
        _initialized = true;
    }

    void OutputTrackMixer::visibilityChanged()
    {
        IRowView::visibilityChanged();
    }

    void OutputTrackMixer::meterUpdate()
    {
        auto& comps = getRowComponents();
        for( auto& it : comps )
        {
            if( auto r = dynamic_cast<OutputTrackDisplay*>( it ) )
            {
                r->meterUpdate();
            }
        }
    }

    void OutputTrackMixer::knobCommand( int knob, int value )
    {
        auto&                            rowComp       = getRowComponents();
        auto                             comp1         = dynamic_cast<OutputTrackDisplay*>( rowComp[0] );
        auto                             elementBounds = comp1->getElementBounds( ( knob - 1 ) % 4 + 1 );
        auto                             numRows       = getNumRows();
        auto                             rows          = getRowNumbers( 3, ( ( knob - 1 ) / 4 ) + 1, elementBounds.getX(), elementBounds.getWidth(), 50 );
        std::vector<OutputTrackDisplay*> entry;

        for( auto i = 0; i < rows.size(); i++ )
        {
            int rowIndex = rows[i];
            if( rowIndex < numRows )
            {
                auto comp = dynamic_cast<OutputTrackDisplay*>( rowComp[rowIndex] );
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

    //////////////////////////////////////////////////////////////////////////

    OutputTrackDisplay::OutputTrackDisplay()
    {
        outPutName.setEditable( false );

        mt::utils::addChildComponents( this, srNo, icon, outPutName, outPutHeader, levelMeter1, levelMeter2, inserts, levelSlider, panSlider, solo, mute );

        srNo.setFontProperties( 36 );
        srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 6 ) );
        icon.toFront( true );
        outPutName.setValueFontHeight( 18 );
        outPutName.setValueBackgroundColour( ui::Theme::textColour( 4 ) );
        outPutName.setPadding( 10 );
        outPutHeader.setValueJustification( juce::Justification::topLeft );
        outPutHeader.setValueColour( ui::Theme::textColour( 1 ) );
        outPutHeader.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        outPutHeader.setValueJustification( juce::Justification::topLeft );

        inserts.setValueColour( ui::Theme::textColour( 1 ) );
        inserts.setValueFontHeight( 18 );

        setProgramIcon( icon, mt::MTProgramType::Unknown );

        panSlider.onValueChanged   = [&]( double Value ) { if( pBus) pBus->pan = Value; };
        levelSlider.onValueChanged = [&]( double Value ) { if( pBus) pBus->gain = CommonUtils::midiToGain(Value); };
        mute.onClick               = [&]() { if( pBus) pBus->mute = mute.getToggleState() ; };
        inserts.onClick            = [&]() {
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
            setProgramIcon( icon, mt::MTProgramType::Unknown );
        } );
    }

    void OutputTrackDisplay::setOutputNumber( const int ID )
    {
        if( !ae.isProjectLoaded() )
            return;

        busNo = ID;
        pBus  = busMgr.getOutput( busNo );

        auto muteValueHandler  = [&]() { mute.setToggleState( pBus ? pBus->mute.get() : false, juce::dontSendNotification ); };
        auto soloValueHandler = [&]() { solo.setToggleState(pBus ? pBus->solo.get() : false, juce::dontSendNotification); };
        auto panValueHandler   = [&]() { panSlider.setValue( pBus ? (double)pBus->pan.get() : 0.0, juce::dontSendNotification ); };
        auto levelValueHandler = [&]() { levelSlider.setValue( CommonUtils::gainToMidi( pBus ? (double)pBus->gain.get() : 0.0 ), juce::dontSendNotification ); };

        scope.reset();
        if( pBus )
        {
            scope = std::make_shared<mt::Kick::ScopeTracker>();

            srNo.setText( juce::String( busNo + 1 ) );

            pBus->solo.onValueChange.connect(soloValueHandler, scope);
            pBus->mute.onValueChange.connect( ( muteValueHandler ), scope );
            pBus->pan.onValueChange.connect( ( panValueHandler ), scope );
            pBus->gain.onValueChange.connect( ( levelValueHandler ), scope );
            solo.setToggleState(pBus->solo.get(), juce::dontSendNotification);
        }

        updateData();
    }

    void OutputTrackDisplay::updateData()
    {
        if( isVisible() )
        {
            setProgramIcon( icon, mt::MTProgramType::Unknown );

            auto nameValueHandler = [&]() { srNo.setText( juce::String( busNo + 1 ) ); };
            auto srNoValueHandler = [&]() { outPutName.setText( pBus ? pBus->name : "", juce::dontSendNotification ); };

            // Force data update.
            srNoValueHandler();
            nameValueHandler();
            if( pBus )
            {
                pBus->solo.onValueChange();
            pBus->mute.onValueChange();
                pBus->pan.onValueChange();
                pBus->gain.onValueChange();
            }
        }
    }

    void OutputTrackDisplay::clear()
    {
        busNo = -1;
        outPutName.setText( "", juce::dontSendNotification );
        levelSlider.setValue( 0.0, juce::dontSendNotification );
        panSlider.setValue( 0.0, juce::dontSendNotification );
        solo.setToggleState( false, juce::dontSendNotification );
        mute.setToggleState( false, juce::dontSendNotification );
    }

    void OutputTrackDisplay::meterUpdate()
    {
        auto& in  = pBus ? pBus->readInMeter() : AudIO::_emptyMeterData;
        auto& out = pBus ? pBus->readOutMeter() : AudIO::_emptyMeterData;
        levelMeter1.setSignalStrength( in[0].rms, in[1].rms, in[0].peak, in[0].peak );
        levelMeter2.setSignalStrength( out[0].rms, out[1].rms, out[1].peak, out[1].peak );
    }

    void OutputTrackDisplay::resized()
    {
        srNo.setBoundsRelative( .001f, .0f, .044f, .982f );
        icon.setBoundsRelative( .001f, .491f, .044f, .491f );
        outPutHeader.setBoundsRelative( .065f, .0734f, .1603f, .2201f );
        outPutName.setBoundsRelative( .065f, .267f, .1603f, .404f );
        levelMeter1.setBoundsRelative( .0651f, .68f, .1603f, .2935f );
        levelMeter2.setBoundsRelative( .4346f, .68f, .1607f, .2935f );
        inserts.setBoundsRelative( .2594f, .3035f, .099f, .3928f );
        levelSlider.setBoundsRelative( .6198f, .0892f, .1653f, .8214f );
        panSlider.setBoundsRelative( .8049f, .0892f, .0792f, .8214f );
        solo.setBoundsRelative( .9108f, .0892f, .0594f, .3928f );
        mute.setBoundsRelative( .9108f, .5178f, .0594f, .3928f );
    }

    void OutputTrackDisplay::linkRotated( int ID, int value )
    {
        switch( ID )
        {
            case 1: levelSlider.knobRotated( value ); break;
            case 2: panSlider.knobRotated( value ); break;
            case 3:
                if(pBus && value != 0) { pBus->mute = value > 0; pBus->solo = value < 0; }
                break;
        }
    }

    juce::Rectangle<int> OutputTrackDisplay::getElementBounds( int state )
    {
        switch( state )
        {
            case 2: return levelSlider.getBounds(); break;
            case 3: return panSlider.getBounds(); break;
            case 4: return mute.getBounds(); break;
            default: return {}; break;
        }
    }

    void OutputTrackDisplay::visibilityChanged()
    {
        bool visible = isVisible();

        for( auto child : getChildren() )
            child->setVisible( visible );

        if( visible )
            updateData();
    }
}
