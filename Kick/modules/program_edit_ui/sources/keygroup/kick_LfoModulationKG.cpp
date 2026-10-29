#include "kick_LfoModulationKG.h"

namespace kick_ui
{

    LfoModulationKG::LfoModulationKG()
    {
        addAndMakeVisible( &layout );
        //pitchVS.
        layout.addComponent( vsLabel, 0, 0, 215 );
        layout.addComponent( lfoLabel, 1, 0, 215 );
        layout.addComponent( dstLabel, 2, 0, 215 );
        layout.addComponent( ctrlLabel, 3, 0, 215 );

        layout.addComponent( pitchVS, 0, 0, 173 );
        layout.addComponent( attackVS, 0, 0, 173 );
        layout.addComponent( ampVS, 0, 0, 173 );
        layout.addComponent( panVS, 0, 0, 173 );
        pitchVS.showTitle( true );
        pitchVS.showValue( true );
        attackVS.showTitle( true );
        attackVS.showValue( true );
        ampVS.showTitle( true );
        ampVS.showValue( true );
        panVS.showTitle( true );
        panVS.showValue( true );

        layout.addComponent( waveLfo, 1, 0, 173 );
        layout.addComponent( rateLfo, 1, 0, 173 );
        layout.addComponent( syncLfo, 1, 0, 173 );
        rateLfo.showTitle( true );
        rateLfo.showValue( true );

        layout.addComponent( pitchDs, 2, 0, 173 );
        layout.addComponent( ampDs, 2, 0, 173 );
        layout.addComponent( filterDs, 2, 0, 173 );
        layout.addComponent( panDs, 2, 0, 173 );
        pitchDs.showTitle( true );
        pitchDs.showValue( true );
        ampDs.showTitle( true );
        ampDs.showValue( true );
        filterDs.showTitle( true );
        filterDs.showValue( true );
        panDs.showTitle( true );
        panDs.showValue( true );

        layout.addComponent( pitchCm, 3, 0, 173 );
        layout.addComponent( pitchBendCm, 3, 0, 173 );
        layout.addComponent( wheelCm, 3, 0, 173 );
        layout.addComponent( aftDsCm, 3, 0, 173 );
        pitchCm.showTitle( true );
        pitchCm.showValue( true );
        pitchBendCm.showTitle( true );
        pitchBendCm.showValue( true );
        wheelCm.showTitle( true );
        wheelCm.showValue( true );
        aftDsCm.showTitle( true );
        aftDsCm.showValue( true );

        waveLfo.setListOptions( mt::Kick::waveShapes );
        syncLfo.setListOptions( { "None", "1/1", "1/2", "1/4" } );
    }

    LfoModulationKG::~LfoModulationKG()
    {
    }

    void LfoModulationKG::resized()
    {
        auto bnds = getLocalBounds();
        bnds.removeFromTop( 10 );
        layout.setBounds( bnds );
    }

    KeyGroupLFO::KeyGroupLFO( std::vector<int>& Keys, mt::MTKeygroupProgram*& PKeyGroup )
        : keys( Keys ), pKeyGroup( PKeyGroup ), KickGrid( 4, 4 )
    {
        setRowTitles( { "Source", "Destination", "" } );

        addComponent( lfoType, 0, 0 );
        addComponent( rate, 0, 2 );
        enabled.setClickingTogglesState(true); enabled.setComponentID("lfo-tab-on"); addComponent(enabled, 0, 3);
        enabled.onClick = [&] { if(pKeyGroup && !keys.empty()) pKeyGroup->setLFOEnabled(keys[0], enabled.getToggleState()); };

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
            if( pKeyGroup )
                pKeyGroup->setLFOType( keys[0], Index );
        };

        pitch.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeyGroup )
                pKeyGroup->setLFOPitch( keys[0], Value );
        };

        filter.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeyGroup )
                pKeyGroup->setLFOFilter( keys[0], Value );
        };

        amp.onValueChanged = [&]( juce::int64 Value ) {
            if( pKeyGroup )
                pKeyGroup->setLFOLevel( keys[0], Value );
        };

        pan.onValueChanged = [&]( double Value ) {
            if( pKeyGroup )
                pKeyGroup->setLFOPan( keys[0], Value );
        };

        rate.onValueChanged = [&]( double Value ) {
            if( pKeyGroup )
                pKeyGroup->setLFOFrequency( keys[0], Value );
        };
    }

    void KeyGroupLFO::refresh()
    {
        if( pKeyGroup && keys.size() )
            refreshKeys();
    }

    void KeyGroupLFO::refreshKeys()
    {
        if( pKeyGroup && keys.size() )
        {
            lfoType.setSelected( pKeyGroup->getLFOType( keys[0] ), juce::dontSendNotification );
            enabled.setToggleState(pKeyGroup->getLFOEnabled(keys[0]), juce::dontSendNotification);
            rate.setValue( pKeyGroup->getLFOFrequency( keys[0] ), juce::dontSendNotification );
            pitch.setValue( pKeyGroup->getLFOPitch( keys[0] ), juce::dontSendNotification );
            filter.setValue( pKeyGroup->getLFOFilter( keys[0] ), juce::dontSendNotification );
            amp.setValue( pKeyGroup->getLFOLevel( keys[0] ), juce::dontSendNotification );
            pan.setValue( pKeyGroup->getLFOPan( keys[0] ), juce::dontSendNotification );
        }
    }

    void KeyGroupLFO::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
            refresh();
    }

    void KeyGroupLFO::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            if(keys.empty()) return;
            const int mapped = pKeyGroup ? pKeyGroup->getZoneKeyForNote(padNum + mt::getAudioInterface().getBankOffset()) : -1;
            if(mapped < 0) return;
            keys[0] = mapped;
            refresh();
        }
    }

    KeyGroupModulation::KeyGroupModulation()
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

    void KeyGroupModulation::handleCommand( CPktMgr::HWMsg_t& )
    {
    }

    void KeyGroupModulation::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 1004, 94 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 0, 98, 1004, 290 }, ui::Theme::backgroundColour( 1 ) );
    }

    void KeyGroupModulation::resized()
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
