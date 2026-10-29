namespace kick_ui
{
    PanVelocityEditKG::PanVelocityEditKG()
    {
        addAndMakeVisible( rcLayout );
        rcLayout.setColumnWidth( { 354, -1 } );
        std::vector<juce::String> columTitle( 5 );
        columTitle[0] = "Layers";
        columTitle[1] = "Pan";
        columTitle[2] = "VelStart";
        columTitle[3] = "VelEnd";
        columTitle[4] = "Root Note";

        rcLayout.setRowTitles( columTitle );

        rcLayout.addComponent( layerOne, 0, 0, 334 );
        rcLayout.addComponent( layerTwo, 1, 0, 334 );
        rcLayout.addComponent( layerThree, 2, 0, 334 );
        rcLayout.addComponent( layerFour, 3, 0, 334 );

        rcLayout.addComponent( layerOnePan, 0, 1, 140 );
        rcLayout.addComponent( layerOneVelocityStart, 0, 1, 140 );
        rcLayout.addComponent( layerOneVelocityEnd, 0, 1, 140 );
        rcLayout.addComponent( layerOneRoot, 0, 1, 140 );

        rcLayout.addComponent( layerTwoPan, 1, 1, 140 );
        rcLayout.addComponent( layerTwoVelocityStart, 1, 1, 140 );
        rcLayout.addComponent( layerTwoVelocityEnd, 1, 1, 140 );
        rcLayout.addComponent( layerTwoRoot, 1, 1, 140 );

        rcLayout.addComponent( layerThreePan, 2, 1, 140 );
        rcLayout.addComponent( layerThreeVelocityStart, 2, 1, 140 );
        rcLayout.addComponent( layerThreeVelocityEnd, 2, 1, 140 );
        rcLayout.addComponent( layerThreeRoot, 2, 1, 140 );

        rcLayout.addComponent( layerFourPan, 3, 1, 140 );
        rcLayout.addComponent( layerFourVelocityStart, 3, 1, 140 );
        rcLayout.addComponent( layerFourVelocityEnd, 3, 1, 140 );
        rcLayout.addComponent( layerFourRoot, 3, 1, 140 );

        auto sampleFiles = mt::getAudioInterface().getAssets( mt::AssetTypes::Samples );

        layerOne.setListOptions( sampleFiles );
        layerTwo.setListOptions( sampleFiles );
        layerThree.setListOptions( sampleFiles );
        layerFour.setListOptions( sampleFiles );
    }

    void PanVelocityEditKG::resized()
    {
        auto bnds = getLocalBounds();
        bnds.removeFromTop( 10 );
        rcLayout.setBounds( bnds );
    }

    PanVelocityEditKG::~PanVelocityEditKG()
    {}

    KGLayerMix::KGLayerMix( std::vector<int>& Keys, mt::MTKeygroupProgram*& pKeyGroup )
        : keys( Keys ), pKeygroup( pKeyGroup ), KickGrid( 4, 6 )
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
            velocityLow[i].onValueChanged = [&, i](double v) { if(pKeygroup && !keys.empty()) { pKeygroup->setLayerVelocityStart(keys[0], i, (int)v); refresh(); } };
            velocityHigh[i].onValueChanged = [&, i](double v) { if(pKeygroup && !keys.empty()) { pKeygroup->setLayerVelocityEnd(keys[0], i, (int)v); refresh(); } };


            semi[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pKeygroup )
                    pKeygroup->setLayerSemi( keys[0], i, Value );
            };

            fine[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pKeygroup )
                    pKeygroup->setLayerFine( keys[0], i, Value );
            };

            amp[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pKeygroup )
                    pKeygroup->setLayerGain( keys[0], i, CommonUtils::midiToGain( Value ) );
            };

            pan[i].onValueChanged = [&, i]( double Value ) {
                if( pKeygroup )
                    pKeygroup->setLayerPan( keys[0], i, Value );
            };
        }
    }

    void KGLayerMix::refresh()
    {
        if( pKeygroup && keys.size() )
            refreshKeys();
    }

    void KGLayerMix::refreshKeys()
    {
        if( pKeygroup && keys.size() )
        {
            for( auto index = 0; index < 4; ++index )
            {
                semi[index].setValue( pKeygroup->getLayerSemi( keys[0], index ), juce::dontSendNotification );
                fine[index].setValue( pKeygroup->getLayerFine( keys[0], index ), juce::dontSendNotification );
                amp[index].setValue( CommonUtils::gainToMidi( pKeygroup->getLayerGain( keys[0], index ) ), juce::dontSendNotification );
                pan[index].setValue( pKeygroup->getLayerPan( keys[0], index ), juce::dontSendNotification );
                velocityLow[index].setValue(pKeygroup->getLayerVelocityStart(keys[0], index), juce::dontSendNotification);
                velocityHigh[index].setValue(pKeygroup->getLayerVelocityEnd(keys[0], index), juce::dontSendNotification);

            }
        }
    }

    void KGLayerMix::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(keys.empty()) return;
            const int mapped = pKeygroup ? pKeygroup->getZoneKeyForNote(padNum + mt::getAudioInterface().getBankOffset()) : -1;
            if(mapped < 0) return;
            keys[0] = mapped;
            refresh();
        }
    }

    void KGLayerMix::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
            refresh();
    }

    KeyGroupInserts::KeyGroupInserts( mt::MTKeygroupProgram*& pKeyGroup )
        : pKeygroup( pKeyGroup )
    {
        mt::utils::addAndMakeVisible( this, insert1Label, insert2Label, insert3Label, insert4Label, type, edit1, edit2, edit3, edit4, del1, del2, del3, del4, rotary1, rotary2, rotary3, rotary4 );
        mt::utils::addAndMakeVisible( this, cutoff, resonance, feedBack, time1, time2, dryWet1, dryWet2, dryWet3, width1, width2, ratio, insert1, insert2, insert3, insert4 );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), insert1, insert2, insert3, insert4, del1, del2, del3, del4, edit1, edit2, edit3, edit4 );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), insert1Label, insert2Label, insert3Label, insert4Label );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 1 ), insert1, insert2, insert3, insert4 );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 3 ), insert1Label, insert2Label, insert3Label, insert4Label );
        mt::utils::setValueFontHeight( 16, insert1, insert2, insert3, insert4 );
        mt::utils::setValueFontHeight( 18, insert1Label, insert2Label, insert3Label, insert4Label, del1, del2, del3, del4, edit1, edit2, edit3, edit4 );
        mt::utils::setValueJustification( juce::Justification::centred, insert1Label, insert2Label, insert3Label, insert4Label );

        auto insertAdder = [&]() {
            kick_ui::signals::onScreenData( kick_ui::Screen::Inserts, (void*)( dynamic_cast<mt::MTIInsertsInterface*>( pKeygroup ) ) );
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
        };

        edit1.onClick = insertAdder;
        edit1.onClick = insertAdder;
        edit1.onClick = insertAdder;
        edit1.onClick = insertAdder;

        del1.onClick = [&]() { deleteInsert( 0 ); };
        del2.onClick = [&]() { deleteInsert( 1 ); };
        del3.onClick = [&]() { deleteInsert( 2 ); };
        del4.onClick = [&]() { deleteInsert( 3 ); };

        insert1.setPadding( 10 );
        insert2.setPadding( 10 );
        insert3.setPadding( 10 );
        insert4.setPadding( 10 );

        type.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

        ui::connectTheme( *this, [&]() {

        } );
    }

    void KeyGroupInserts::deleteInsert( unsigned int Position )
    {
        if( pKeygroup )
        {
            pKeygroup->removeInsert( Position );
            refresh();
        }
    }

    void KeyGroupInserts::refresh()
    {
        if( pKeygroup )
        {
            insert1Label.setText( pKeygroup->getInsertName( 0 ), juce::dontSendNotification );
            insert2Label.setText( pKeygroup->getInsertName( 1 ), juce::dontSendNotification );
            insert3Label.setText( pKeygroup->getInsertName( 2 ), juce::dontSendNotification );
            insert4Label.setText( pKeygroup->getInsertName( 3 ), juce::dontSendNotification );
        }
    }

    void KeyGroupInserts::visibilityChanged()
    {
        if( isVisible() )
            refresh();
    }

    void KeyGroupInserts::handleCommand( CPktMgr::HWMsg_t& )
    {
    }

    void KeyGroupInserts::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 0, 98, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 0, 196, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 0, 294, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
    }

    void KeyGroupInserts::resized()
    {
        insert1.setBoundsRelative( .019f, .05f, .16f, .14f );
        insert1Label.setBoundsRelative( .19f, .038f, .14f, .165f );
        edit1.setBoundsRelative( .342f, .05f, .044f, .14f );
        del1.setBoundsRelative( .39f, .05f, .044f, .14f );
        type.setBoundsRelative( .45f, .013f, .12f, .165f );
        cutoff.setBoundsRelative( .609f, .013f, .08f, .217f );
        resonance.setBoundsRelative( .746f, .013f, .08f, .217f );
        dryWet1.setBoundsRelative( .882f, .013f, .08f, .217f );

        insert2.setBoundsRelative( .019f, .304f, .16f, .14f );
        insert2Label.setBoundsRelative( .19f, .291f, .14f, .165f );
        edit2.setBoundsRelative( .342f, .304f, .044f, .14f );
        del2.setBoundsRelative( .39f, .304f, .044f, .14f );
        time1.setBoundsRelative( .472f, .266f, .08f, .217f );
        feedBack.setBoundsRelative( .609f, .266f, .08f, .217f );
        width1.setBoundsRelative( .746f, .266f, .08f, .217f );
        dryWet2.setBoundsRelative( .882f, .266f, .08f, .217f );

        insert3.setBoundsRelative( .019f, .558f, .16f, .14f );
        insert3Label.setBoundsRelative( .19f, .545f, .14f, .165f );
        edit3.setBoundsRelative( .342f, .558f, .044f, .14f );
        del3.setBoundsRelative( .39f, .558f, .044f, .14f );
        time2.setBoundsRelative( .472f, .52f, .08f, .217f );
        ratio.setBoundsRelative( .609f, .52f, .08f, .217f );
        width2.setBoundsRelative( .746f, .52f, .08f, .217f );
        dryWet3.setBoundsRelative( .882f, .52f, .08f, .217f );

        insert4.setBoundsRelative( .019f, .812f, .16f, .14f );
        insert4Label.setBoundsRelative( .19f, .799f, .14f, .165f );
        edit4.setBoundsRelative( .342f, .812f, .044f, .14f );
        del4.setBoundsRelative( .39f, .812f, .044f, .14f );
        rotary1.setBoundsRelative( .472f, .774f, .08f, .217f );
        rotary2.setBoundsRelative( .609f, .774f, .08f, .217f );
        rotary3.setBoundsRelative( .746f, .774f, .08f, .217f );
        rotary4.setBoundsRelative( .882f, .774f, .08f, .217f );
    }
}
