#include "kick_audio_track_mix.h"

namespace kick_ui
{
    AudioTrackMixer::AudioTrackMixer()
        : kick_ui::IRowView( "Audio Track Mixer" )
    {
        setupProvider();
        setMargin( 0, 6, 2, 0 );
    }

    void AudioTrackMixer::setupProvider()
    {
        provider.numRows = [&]() -> int {
            return numAudioPrograms;
        };

        provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            if( juce::isPositiveAndBelow( Row, numAudioPrograms ) )
            {
                auto comp = dynamic_cast<AudioTrackDisplay*>( Existing );
                if( comp == nullptr )
                {
                    comp                = new AudioTrackDisplay();
                    comp->onSoloChanged = [&]() { refresh(); };
                }
                comp->setAudioNumber( Row, audioData[Row] );
                return comp;
            }
            return nullptr;
        };
        _initialized = true;
    }

    void AudioTrackMixer::populate( void )
    {
        if( !isVisible() || !_initialized.load() )
            return;
        updateProgramData();
        refresh();
    }

    void AudioTrackMixer::onProgramListChanged()
    {
        if( ae.isProjectLoaded() )
            populate();
    }

    void AudioTrackMixer::visibilityChanged()
    {
        IRowView::visibilityChanged();

        if( isVisible() )
        {
            ae.addListener( this );
            updateProgramData();
        }
        else
            ae.removeListener( this );
    }

    void AudioTrackMixer::meterUpdate()
    {
        updateProgramData();
        if( numAudioPrograms <= 0 )
            refresh();
        else
        {
            auto& comps = getRowComponents();
            for( auto& it : comps )
            {
                if( auto r = dynamic_cast<AudioTrackDisplay*>( it ) )
                {
                    r->meterUpdate();
                }
            }
        }
    }

    void AudioTrackMixer::updateProgramData()
    {
        audioData.clear();

        numAudioPrograms = 0;
        for( auto idx = 0; idx < ae.getNumPrograms(); ++idx )
        {
            auto type = ae.getProgram( idx )->getType();
            if( type == mt::MTProgramType::Audio )
            {
                audioData.push_back( idx );
                ++numAudioPrograms;
            }
        }
    }

    void AudioTrackMixer::knobCommand( int knob, int value )
    {
        auto&                           rowComp       = getRowComponents();
        auto                            comp1         = dynamic_cast<AudioTrackDisplay*>( rowComp[0] );
        auto                            elementBounds = comp1->getElementBounds( ( knob - 1 ) % 4 + 1 );
        auto                            numRows       = getNumRows();
        auto                            rows          = getRowNumbers( 3, ( knob - 1 ) / 4 + 1, elementBounds.getX(), elementBounds.getWidth(), 50 );
        std::vector<AudioTrackDisplay*> entry;

        for( auto i = 0; i < rows.size(); i++ )
        {
            int rowIndex = rows[i];
            if( rowIndex >= 0 && rowIndex < numRows )
            {
                auto comp = dynamic_cast<AudioTrackDisplay*>( rowComp[rowIndex] );
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

    //////////////////////////////////////////////////////////////////////////////////

    AudioTrackDisplay::AudioTrackDisplay()
    {
        mt::utils::addAndMakeVisible( this, srNo, icon, audioName, audioHeader, levelMeter1, levelMeter2, levelSlider, inserts, panSlider, inputListBox, outputListBox, armButton, soloButton, muteButton, monitorButton );
        mt::utils::setTitleFontHeight( 16, levelSlider, panSlider, inputListBox, outputListBox );
        mt::utils::setValueFontHeight( 18, levelSlider, panSlider, audioName, inserts );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), audioHeader, inserts );

        std::vector<juce::String> busNames;
        auto&                     outBusNames = ae.getOutputBusNames();
        for( auto& n : outBusNames )
            busNames.push_back( n );
        outputListBox.setListOptions( busNames );
        busNames.clear();
        busNames.push_back("None");
        auto& inputBuses = ae.getInputBusNames();
        for( auto& bus : inputBuses )
            busNames.push_back( bus );
        inputListBox.setListOptions( busNames );

        srNo.setFontProperties( 36 );
        srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 6 ) );
        icon.toFront( true );

        levelSlider.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        levelSlider.setTitleJustification( juce::Justification::centredTop );
        levelSlider.setValueJustification( juce::Justification::centredBottom );

        audioName.setEditable( false );
        audioName.setValueBackgroundColour( ui::Theme::textColour( 4 ) );
        audioName.setPadding( 10 );
        audioHeader.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        audioHeader.setValueJustification( juce::Justification::topLeft );

        inputListBox.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        inputListBox.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour( 9 ) );
        inputListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        outputListBox.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        outputListBox.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour( 9 ) );
        outputListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

        levelSlider.setRange( MIDI::MIN, MIDI::MAX, MIDI::DEFAULT );
        panSlider.setRange( PAN::MIN, PAN::MAX, PAN::STEP );

        setProgramIcon( icon, mt::MTProgramType::Audio );

        inserts.onClick = [&]() {
            kick_ui::signals::onScreenData( Screen::Inserts, (void*)( dynamic_cast<mt::MTIInsertsInterface*>( pProg ) ) );
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
        };

        inputListBox.onSelectionChanged = [&]( int Index, const juce::String& Option ) {
            if(pProg) pProg->input = Option;
        };
        armButton.setClickingTogglesState(true); monitorButton.setClickingTogglesState(true);
        armButton.setComponentID("audio-arm"); monitorButton.setComponentID("audio-monitor"); inputListBox.setComponentID("audio-input");
        armButton.onClick = [this] { if(pProg) pProg->armed = armButton.getToggleState(); };
        monitorButton.onClick = [this] { if(pProg) pProg->monitor = monitorButton.getToggleState(); };

        outputListBox.onSelectionChanged = [&]( int Index, const juce::String& Option ) {
            if( pProg )
                pProg->output = Option;
        };

        levelSlider.onValueChanged = [&]( double Value ) {
            if( pProg )
                pProg->level = (int)Value;
        };

        panSlider.onValueChanged = [&]( double Value ) {
            if( pProg )
                pProg->pan = Value;
        };

        muteButton.onClick = [&]() {
            if( pProg )
                pProg->mute = muteButton.getToggleState();
        };

        soloButton.onClick = [&]() {
            if( pProg )
                pProg->solo = soloButton.getToggleState();
        };

        ui::connectTheme( *this, [&]() {
            monitorButton.setImages( "Monitor_normal", "Monitor_active" );
            soloButton.setImages( "Solo_normal", "Solo_active" );
            muteButton.setImages( "Mute_normal", "Mute_active" );
            setProgramIcon( icon, mt::MTProgramType::Audio );
        } );
    }

    void AudioTrackDisplay::setAudioNumber( const int ID, const int ProgNo )
    {
        srNo.setText( juce::String( ID + 1 ) );
        progNo = ProgNo;
        pProg  = dynamic_cast<mt::MTAudioProgram*>(ae.getProgram( progNo ));

        auto nameHandler   = [&]() { audioName.setText( pProg ? pProg->name.get() : "", juce::dontSendNotification ); };
        auto muteHandler   = [&]() { muteButton.setToggleState( pProg ? pProg->mute.get() : false, juce::dontSendNotification ); };
        auto soloHandler   = [&]() { soloButton.setToggleState( pProg ? pProg->solo.get() : false, juce::dontSendNotification ); };
        auto panHandler    = [&]() { panSlider.setValue( pProg ? (double)pProg->pan.get() : 0.0, juce::dontSendNotification ); };
        auto levelHandler  = [&]() { levelSlider.setValue( pProg ? (double)pProg->level.get() : 0.0, juce::dontSendNotification ); };
        auto outputHandler = [&]() { outputListBox.setSelected( pProg ? pProg->output.get() : "", juce::dontSendNotification ); };
        auto inputHandler = [this] { inputListBox.setSelected(pProg ? pProg->input.get() : "None", juce::dontSendNotification); };
        auto armHandler = [this] { armButton.setToggleState(pProg && pProg->armed.get(), juce::dontSendNotification); };
        auto monitorHandler = [this] { monitorButton.setToggleState(pProg && pProg->monitor.get(), juce::dontSendNotification); };

        scope.reset();
        if( pProg )
        {
            scope = std::make_shared<mt::Kick::ScopeTracker>();

            pProg->name.onValueChange.connect( nameHandler, scope );
            pProg->mute.onValueChange.connect( muteHandler, scope );
            pProg->solo.onValueChange.connect( soloHandler, scope );
            pProg->level.onValueChange.connect( levelHandler, scope );
            pProg->pan.onValueChange.connect( panHandler, scope );
            pProg->output.onValueChange.connect( outputHandler, scope );
            pProg->input.onValueChange.connect(inputHandler, scope);
            pProg->armed.onValueChange.connect(armHandler, scope);
            pProg->monitor.onValueChange.connect(monitorHandler, scope);
        }
        outputHandler(); inputHandler(); armHandler(); monitorHandler();
        updateData();
    }

    void AudioTrackDisplay::clear()
    {
        progNo = -1;
        pProg  = nullptr;
        srNo.setImage( getEditProgramIcon( mt::MTProgramType::Unknown ) );
        audioName.setText( "", juce::dontSendNotification );
        panSlider.setValue( 0.0, juce::dontSendNotification );
        soloButton.setToggleState( false, juce::dontSendNotification );
        muteButton.setToggleState( false, juce::dontSendNotification );
        armButton.setToggleState(false, juce::dontSendNotification); monitorButton.setToggleState(false, juce::dontSendNotification);
        inputListBox.setSelected("None", juce::dontSendNotification); scope.reset();
    }

    void AudioTrackDisplay::updateData()
    {
        auto iconValueHandler = [&]() { srNo.setImage( getEditProgramIcon( pProg ? pProg->type.get() : mt::MTProgramType::Unknown ) ); };

        iconValueHandler();
        if( pProg )
        {
            pProg->name.onValueChange();
            pProg->mute.onValueChange();
            pProg->solo.onValueChange();
            pProg->level.onValueChange();
            pProg->pan.onValueChange();
        }
        else
            clear();
    }

    void AudioTrackDisplay::meterUpdate()
    {
        if( pProg )
        {
            auto& in  = pProg->readInMeter();
            auto& out = pProg->readOutMeter();
            levelMeter1.setSignalStrength( in[0].rms, in[1].rms, in[0].peak, in[1].peak );
            levelMeter2.setSignalStrength( out[0].rms, out[1].rms, out[0].peak, out[1].peak );
        }
    }

    void AudioTrackDisplay::resized()
    {
        srNo.setBoundsRelative( .001f, .0f, .044f, .982f );
        icon.setBoundsRelative( .001f, .491f, .044f, .491f );
        audioHeader.setBoundsRelative( .065f, .0704f, .1603f, .2201f );
        audioName.setBoundsRelative( .065f, .257f, .1603f, .409f );
        inserts.setBoundsRelative( .2594f, .3035f, .099f, .3928f );
        inputListBox.setBoundsRelative( .3683f, .0892f, .1188f, .5714f );
        outputListBox.setBoundsRelative( .5069f, .0892f, .1188f, .5714f );
        levelMeter1.setBoundsRelative( .357f, .68f, .121f, .2935f );
        levelMeter2.setBoundsRelative( .497f, .68f, .121f, .2935f );

        levelSlider.setBoundsRelative( .6455f, .0892f, .1188f, .8214f );
        panSlider.setBoundsRelative( .7841f, .0892f, .0435f, .8214f );
        monitorButton.setBoundsRelative( .8465f, .5178f, .0594f, .3928f );
        armButton.setBoundsRelative(.8465f, .0892f, .0594f, .3928f);
        soloButton.setBoundsRelative( .9108f, .0892f, .0594f, .3928f );
        muteButton.setBoundsRelative( .9108f, .5178f, .0594f, .3928f );
    }

    void AudioTrackDisplay::linkRotated( int ID, int value )
    {
        switch( ID )
        {
            case 0: outputListBox.knobRotated( value ); break;
            case 1: levelSlider.knobRotated( value ); break;
            case 2: panSlider.knobRotated( value ); break;
            case 3:
                muteButton.setToggleState( value > 0, juce::dontSendNotification );
                if(pProg) pProg->mute = value > 0;
                break;
        }
    }

    juce::Rectangle<int> AudioTrackDisplay::getElementBounds( int state )
    {
        switch( state )
        {
            case 1: return outputListBox.getBounds(); break;
            case 2: return levelSlider.getBounds(); break;
            case 3: return panSlider.getBounds(); break;
            case 4: return muteButton.getBounds(); break;
            default: return outputListBox.getBounds(); break;
        }
    }
}
