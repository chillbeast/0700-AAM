namespace kick_ui
{
    KeyGroupMaster::KeyGroupMaster( std::vector<int>& Keys, mt::MTKeygroupProgram*& pKeyGroup )
        : ui::KickGrid( 4, 4 ), keys( Keys ), pKeygroup( pKeyGroup )
    {
        setRowTitles( { "Master", " ", "KeyGroup Settings", "Layer Control" } );

        addComponent( levelSlider, 0, 0 );
        addComponent( panSlider, 0, 1 );
        addComponent( semiSlider, 0, 2 );
        addComponent( fineSlider, 0, 3 );

        addComponent( insertsButton, 1, 1 );
        addComponent( outListBox, 1, 2 );

        addComponent( keyTrack, 2, 1 );
        addComponent( muteGroup, 2, 2 );

        addComponent( layerPlay, 3, 0 );
        addComponent( samplePlay, 3, 1 );
        addComponent( kgPoly, 3, 3 );

        std::vector<juce::String> busNames;
        auto&                     outBusNames = mt::getAudioInterface().getOutputBusNames();
        for( auto& n : outBusNames )
            busNames.push_back( n );
        outListBox.setListOptions( busNames );
        outListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        outListBox.setJustificationType( juce::Justification::centred );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), insertsButton, keyTrack, samplePlay, layerPlay );
        mt::utils::setValueFontHeight( 16, insertsButton, keyTrack );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), samplePlay, layerPlay );

        muteGroup.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        kgPoly.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

        levelSlider.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        panSlider.setRange( PAN::MIN, PAN::MAX, PAN::STEP );
        semiSlider.setRange( SEMI::MIN, SEMI::MAX, SEMI::DEFAULT );
        fineSlider.setRange( FINE::MIN, FINE::MAX, FINE::STEP );

        insertsButton.onClick = [&]() {
            kick_ui::signals::onScreenData( kick_ui::Screen::Inserts, (void*)( dynamic_cast<mt::MTIInsertsInterface*>( pKeygroup ) ) );
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
        };

        keyTrack.setComponentID("keygroup-key-track");
        keyTrack.setClickingTogglesState(true);
        keyTrack.onClick = [&]() {
            if(!pKeygroup) return;
            if(auto* undo = pKeygroup->getUndoManager()) undo->beginNewTransaction("Key tracking");
            for(auto key : keys)
                if(auto* zone = pKeygroup->getKeysClip(key)) zone->keyTracking = keyTrack.getToggleState();
        };

        kgPoly.onValueChanged = [&]( int Index ) {
            if( pKeygroup )
                pKeygroup->setPolyphony( Index );
        };

        levelSlider.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeygroup )
                pKeygroup->setLevel( Value );
        };

        panSlider.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeygroup )
                pKeygroup->setPan( Value );
        };

        semiSlider.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeygroup )
                pKeygroup->setSemi( Value );
        };

        fineSlider.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeygroup )
                pKeygroup->setFine( Value );
        };

        layerPlay.onSelectionChanged = [&]( const int Index, const juce::String& Value ) {
            if( pKeygroup )
                pKeygroup->setNoteLayerPlay( keys[0], toEnum<mt::LayerPlayType>( Index ) );
        };

        samplePlay.onSelectionChanged = [&]( const int Index, const juce::String& Value ) {
            if(pKeygroup && !keys.empty() && Index >= 0 && Index < 3)
                pKeygroup->setNoteSamplePlay(keys[0], Index == 0 ? mt::SamplePlayType::ONE_SHOT : Index == 1 ? mt::SamplePlayType::NOTE_ON : mt::SamplePlayType::NOTE_OFF);
        };
        samplePlay.setComponentID("keygroup-sample-play");

        muteGroup.onValueChanged = [&]( int Index ) {
            if( pKeygroup )
                pKeygroup->setNoteMuteGroup( keys[0], Index );
        };

        outListBox.onSelectionChanged = [&]( int, const juce::String& OutPutName ) {
            if( pKeygroup ) pKeygroup->setOutput( OutPutName );
        };

        ui::connectTheme( *this, [&]() {
            insertsButton.setToggleButtonText( "INSERTS", "INSERTS" );
        } );
    }

    void KeyGroupMaster::visibilityChanged()
    {
        if( isVisible() )
            refresh();
    }

    void KeyGroupMaster::refresh()
    {
        auto levelHandler   = [&]() { levelSlider.setValue( pKeygroup->level.get(), juce::dontSendNotification ); };
        auto panHandler     = [&]() { panSlider.setValue( pKeygroup->pan.get(), juce::dontSendNotification ); };
        auto semiHandler    = [&]() { semiSlider.setValue( pKeygroup->semi.get(), juce::dontSendNotification ); };
        auto fineHandler    = [&]() { fineSlider.setValue( pKeygroup->fine.get(), juce::dontSendNotification ); };
        auto polyHandler    = [&]() { kgPoly.setValue( pKeygroup->polyphony.get(), juce::dontSendNotification ); };
        auto outListHandler = [&]() { outListBox.setSelected( pKeygroup->output.get(), juce::dontSendNotification ); };

        scope.reset();
        if( pKeygroup && keys.size() )
        {
            if(auto* zone = pKeygroup->getKeysClip(keys[0]))
                keyTrack.setToggleState(zone->keyTracking.get(), juce::dontSendNotification);
            scope = std::make_shared<mt::Kick::ScopeTracker>();
            if(auto* zone = pKeygroup->getKeysClip(keys[0]))
                zone->keyTracking.onValueChange.connect([this] {
                    if(pKeygroup && !keys.empty())
                        if(auto* current = pKeygroup->getKeysClip(keys[0]))
                            keyTrack.setToggleState(current->keyTracking.get(), juce::dontSendNotification);
                }, scope);

            pKeygroup->level.onValueChange.connect( levelHandler, scope );
            pKeygroup->pan.onValueChange.connect( panHandler, scope );
            pKeygroup->semi.onValueChange.connect( semiHandler, scope );
            pKeygroup->fine.onValueChange.connect( fineHandler, scope );
            pKeygroup->polyphony.onValueChange.connect( polyHandler, scope );
            pKeygroup->output.onValueChange.connect( outListHandler, scope );
        }

        if( pKeygroup && keys.size() )
        {
            pKeygroup->level.onValueChange();
            pKeygroup->pan.onValueChange();
            pKeygroup->semi.onValueChange();
            pKeygroup->fine.onValueChange();
            pKeygroup->polyphony.onValueChange();
            pKeygroup->output.onValueChange();
        }

        if( pKeygroup && keys.size() )
        {
            muteGroup.setValue( pKeygroup->getNoteMuteGroup( keys[0] ), juce::dontSendNotification );
            auto mode = pKeygroup->getNoteSamplePlay(keys[0]);
            samplePlay.setSelected(mode == mt::SamplePlayType::ONE_SHOT ? 0 : mode == mt::SamplePlayType::NOTE_ON ? 1 : 2, juce::dontSendNotification);
            layerPlay.setSelected( (int)pKeygroup->getNoteLayerPlay( keys[0] ), juce::dontSendNotification );
            outListBox.setSelected( pKeygroup->getOutput(), juce::dontSendNotification );
        }
    }

    void KeyGroupMaster::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(keys.empty()) return;
            keys[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }
}
