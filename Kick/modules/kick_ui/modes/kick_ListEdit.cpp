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
    /////////////////////////////////////////////////////////////////////////////////////
    ///////////////               List Edit Mode
    /////////////////////////////////////////////////////////////////////////////////////

    ListEditMode::ListEditMode()
        : IScreen( Screen::MenuListEdit )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        tableView.reset( new ui::KickTableView( "List", provider ) );

        tableView->setRowHeight( 52 );
        tableView->setRowSpacing( 0 );
        tableView->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        tableView->setRowBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        tableView->setMargin( 0, 0, 4, 0 );
        tableView->enableSelectMode( false );
        for( int i = 0; i < headerTitles.size(); ++i )
        {
            tableHeaderData.emplace_back( std::make_unique<ui::KickLabel>( headerTitles[i] ) );
            mt::utils::addChildComponents( this, tableHeaderData[i].get() );
            tableHeaderData[i]->setValueJustification( juce::Justification::centred );
            tableHeaderData[i]->setFontHeight( 16 );
            tableHeaderData[i]->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        }

        mt::utils::addChildComponents( this, header, tableView.get(), filterListBox, snapButton, insertButton, deleteButton, trackSelector, tcButton );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        setInterceptsMouseClicks( true, true );

        header.showSettingsButton( true );
        header.onSettingsClick = [&]() {
            gridSettings.setScreen( GridSettings::ListEdit );
            mt::utils::showDialog( "SETTINGS", &gridSettings, [this]( int R ) {

            },
                                   { 0, 0, 650, 520 } );
        };

        filterListBox.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        filterListBox.setTitleFontHeight( 16 );
        filterListBox.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        filterListBox.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );
        trackSelector.tracks.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );

        snapButton.setValueColour( ui::Theme::textColour( 1 ) );
        snapButton.setValueFontHeight( 18 );
        deleteButton.setValueColour( ui::Theme::textColour( 1 ) );
        deleteButton.setValueFontHeight( 18 );
        insertButton.setValueColour( ui::Theme::textColour( 1 ) );
        insertButton.setValueFontHeight( 18 );
        tcButton.setValueColour( ui::Theme::textColour( 1 ) );
        tcButton.setValueFontHeight( 18 );

        provider.numRows = [&]() {
            return numNotes;
        };

        provider.onRowSelected = [&]( int Row ) {
            auto isEnable = tableView->getSelectMode();
            if( !isEnable )
            {
                mt::MTNoteRO note( noteList.getChild( Row ) );
                ae.selectNote( sel, false );
                ae.selectNote( sel, note.number(), note.start(), true );
            }
        };

        provider.cellComponent = [&]( int Row, juce::Component* Existing ) -> juce::Component* {
            auto comp = dynamic_cast<ListEntry*>( Existing );
            if( juce::isPositiveAndBelow( Row, numNotes ) )
            {
                if( comp == nullptr )
                    comp = new ListEntry( Row, noteList.getChild( Row ) );
                comp->setInterceptsMouseClicks( true, true );
                comp->setValues( Row, noteList.getChild( Row ) );
                return comp;
            }
            return nullptr;
        };

        insertButton.onClick = [&]() {
            auto p = new ListEditInsertDialog();
            mt::utils::showOptions(
                p->getName(), p, [&, p]( int R ) {
                    if( R == UI::APPLY )
                    {
                        auto& data     = p->getData();
                        auto& gridData = GridData::Instance();
                        auto  startPos = 0, startBeat = 0, startTick = 0;
                        auto  startBar       = p->getStartBar();
                        auto  startBeatValue = p->getStartBeat();
                        auto  startTickValue = p->getStartTick();
                        if( startBar != 1 )
                        {
                            startPos = gridData.getTicksUptoBar( startBar - 1 );
                        }
                        startBeat  = gridData.getTicksPerBar( startBar - 1 );
                        auto total = ( startPos + ( ( startBeat / gridData.getBeatsPerBar( startBar - 1 ) ) * ( startBeatValue - 1 ) ) ) + startTickValue;
                        ae.addOrModifyNote( data.padIndex, total, gridData.getTicksPerQuantization( data.bar ) );
                        visibilityChanged();
                        tableView->refresh();
                    }
                    insertButton.setToggleState( false, juce::dontSendNotification );
                    if( p ) delete p;
                } );
        };

        deleteButton.onClick = [&]() {
            if( noteList.isValid() && noteList.getNumChildren() )
            {
                mt::MTNote n( noteList.getChild( tableView->getSelectedRow() ), &mt::getAudioInterface().getUndoManager() );
                ae.eraseNote( n.number.get(), n.start.get(), -1 );
                numNotes = noteList.isValid() ? noteList.getNumChildren() : 0;
                if( tableView->getNumRows() == 1 )
                    tableView->selectRow( 0 );
                tableView->refresh();
                deleteButton.setToggleState( false, juce::dontSendNotification );
            }
        };

        trackSelector.tracks.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            ae.setSelectedTrack( Sel );
            noteList = mt::getAudioInterface().getNoteList();
            numNotes = noteList.isValid() ? noteList.getNumChildren() : 0;
            refreshData();
        };

        tcButton.onClick = [&]() {
            ListEditTimingCorrect::show();
        };
        gridData.gridPlayHeadChanged.connect( [&]() {
            updateViewPosition();
        } );

        ui::connectTheme( *this, [&]() {
            snapButton.setToggleButtonText( "Snap:On", "Snap:Off" );
            deleteButton.setToggleButtonText( "Delete", "Delete" );
            insertButton.setToggleButtonText( "Insert", "Insert" );
            header.setIcon( "Icon_listEdit" );
        } );
    }

    ListEditMode::~ListEditMode() {}

    void ListEditMode::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, tableView->getBounds(), ui::Theme::backgroundColour( 3 ), 5.0f, false, false, true, true );
        mt::utils::fillRoundedRect( g, { 10, 86, 1002, 54 }, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, false, false );
        mt::utils::renderRoundedRect( g, tableView->getBounds(), ui::Theme::backgroundColour( 0 ), 1, 6.f, false, false );
        mt::utils::renderRoundedRect( g, { 10, 86, 1002, 54 }, ui::Theme::backgroundColour( 0 ), 1, 6.f, true, true, false, false );
    }

    void ListEditMode::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        auto cntlButtons = b.removeFromBottom( 54 );
        cntlButtons.removeFromRight( 10 ); // For Right Space

        tableView->setBounds( b.withTrimmedTop( 64 ).reduced( 10, 0 ) );

        trackSelector.setBoundsRelative( 0.225f, 0.0267f, 0.1956f, 0.0735f );

        tcButton.setBoundsRelative( 0.7f, 0.031f, 0.048f, 0.0792f );
        filterListBox.setBoundsRelative( 0.624f, 0.029f, 0.0724f, 0.0735f );

        snapButton.setBounds( cntlButtons.removeFromRight( 120 ).reduced( 0, 10 ) );
        deleteButton.setBounds( cntlButtons.removeFromRight( 128 ).reduced( 4, 10 ) );
        insertButton.setBounds( cntlButtons.removeFromRight( 122 ).reduced( 0, 10 ) );

        tableHeaderData[0]->setBoundsRelative( 0.0190f, 0.172f, 0.0587f, 0.0735f );
        tableHeaderData[1]->setBoundsRelative( 0.0975f, 0.172f, 0.1448f, 0.0735f );
        tableHeaderData[2]->setBoundsRelative( 0.2550f, 0.172f, 0.1174f, 0.0735f );
        tableHeaderData[3]->setBoundsRelative( 0.3980f, 0.172f, 0.0782f, 0.0735f );
        tableHeaderData[4]->setBoundsRelative( 0.493f, 0.172f, 0.0782f, 0.0735f );
        tableHeaderData[5]->setBoundsRelative( 0.590f, 0.172f, 0.1448f, 0.0735f );
        tableHeaderData[6]->setBoundsRelative( 0.755f, 0.172f, 0.0587f, 0.0735f );
        tableHeaderData[7]->setBoundsRelative( 0.8375f, 0.172f, 0.0587f, 0.0735f );
        tableHeaderData[8]->setBoundsRelative( 0.9158f, 0.172f, 0.049f, 0.0735f );
    }

    void ListEditMode::visibilityChanged()
    {
        for( auto it : getChildren() )
            it->setVisible( isVisible() );
        trackSelector.setVisible( isVisible() );

        if( isVisible() )
        {
            trackSelector.tracks.setSelected( ae.getSelectedTrack() );
            markSelectedRows();
        }
        else
        {
            noteList = {};
            numNotes = 0;
        }
    }

    void ListEditMode::refreshData()
    {
        if( isVisible() )
            tableView->refresh();
    }

    void ListEditMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) && gridSettings.getHittingPadMode() )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg ) + ae.getBankOffset();
            tableView->clearSelection();
            tableView->enableSelectMode( true );
            auto i = 0;
            for( const auto& it : noteList )
            {
                auto note = mt::MTNoteRO( it );
                if( padNum == note.number() )
                {
                    tableView->selectRow( i );
                }
                i++;
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
        tableView->enableSelectMode( false );
    }

    void ListEditMode::knobCommand( int knob, int value )
    {
        auto& rowComp       = tableView->getRowComponents();
        auto  comp1         = dynamic_cast<ListEntry*>( rowComp[0] );
        auto  elementBounds = comp1->getElementBounds( ( knob - 1 ) % 4 + 1 );
        auto  totalRows     = tableView->getNumRows();
        auto  rows          = tableView->getRowNumbers( 4, ( ( knob - 1 ) / 4 ) + 1, elementBounds.getX(), elementBounds.getWidth(), 128 );

        std::vector<ListEntry*> entry;

        for( auto i = 0; i < rows.size(); i++ )
        {
            int rowIndex = rows[i];
            if( rowIndex < totalRows )
            {
                auto comp = dynamic_cast<ListEntry*>( rowComp[rowIndex] );
                entry.push_back( comp );
            }
        }
        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
            case contracts::DeviceControlContracts::EN_KNOB_2:
            case contracts::DeviceControlContracts::EN_KNOB_3:
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( entry.size() >= 1 && entry[0] ) entry[0]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_1 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_5:
            case contracts::DeviceControlContracts::EN_KNOB_6:
            case contracts::DeviceControlContracts::EN_KNOB_7:
            case contracts::DeviceControlContracts::EN_KNOB_8:
                if( entry.size() >= 2 && entry[1] ) entry[1]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_5 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_9:
            case contracts::DeviceControlContracts::EN_KNOB_10:
            case contracts::DeviceControlContracts::EN_KNOB_11:
            case contracts::DeviceControlContracts::EN_KNOB_12:
                if( entry.size() >= 3 && entry[2] ) entry[2]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_9 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_13:
            case contracts::DeviceControlContracts::EN_KNOB_14:
            case contracts::DeviceControlContracts::EN_KNOB_15:
            case contracts::DeviceControlContracts::EN_KNOB_16:
                if( entry.size() >= 4 && entry[3] ) entry[3]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_13 ), value );
                break;
        }
    }

    void ListEditMode::markSelectedRows()
    {
        tableView->clearSelection();
        tableView->enableSelectMode( true );

        auto selectedNotes = ae.getSelectedNoteList();

        for( const auto& it : selectedNotes )
        {
            mt::MTNoteRO selectedNote( it );
            auto         i = 0;
            for( const auto& itr : noteList )
            {
                mt::MTNoteRO note( itr );
                if( selectedNote.number() == note.number() && selectedNote.start() == note.start() )
                {
                    tableView->selectRow( i );
                }
                i++;
            }
        }
        tableView->enableSelectMode( false );
    }

    void ListEditMode::updateViewPosition()
    {
        if( isVisible() )
        {
            // Total Number of Rows and  Height
            auto totalRows      = tableView->getNumRows();
            auto totalRowhieght = tableView->getRowHeight();
            auto totalHeight    = totalRows * totalRowhieght;

            auto& ViewPort       = tableView->getTableViewPort();
            auto  xPos           = ViewPort.getViewPositionX();
            auto  ViewPortHeight = ViewPort.getHeight();

            auto normized   = ( gridData.trackPlayHead.normized );
            auto scrollType = gridSettings.getAutoScrollType();
            if( scrollType == Follow )
            {
                int scrollY = static_cast<int>( normized * ( totalHeight - ViewPortHeight ) );
                ViewPort.setViewPosition( xPos, scrollY );
                if( normized == 0.0f )
                {
                    ViewPort.setViewPosition( 0, 0 );
                }
            }
            else if( scrollType == Page )
            {
                auto totalPages  = std::ceil( static_cast<float>( totalHeight ) / ViewPortHeight );
                auto currentPage = static_cast<int>( normized * totalPages );

                ViewPort.setViewPosition( xPos, currentPage * ViewPortHeight );

                if( normized == 0.0f )
                {
                    ViewPort.setViewPosition( 0, 0 );
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////
    ///////////////               List Entry
    /////////////////////////////////////////////////////////////////////////////////////

    ListEntry::ListEntry( int Row, juce::ValueTree Vt )
    {
        addMouseListener( this, true );
        mt::utils::addAndMakeVisible( this, row, time, note, length, velocity, type, rat, prob, value );
        row.setValueJustification( juce::Justification::centred );
        time.setValueJustification( juce::Justification::centred );
        type.setValueJustification( juce::Justification::centred );
        length.setValueJustification( juce::Justification::centred );
        velocity.setValueJustification( juce::Justification::centred );
        rat.setValueJustification( juce::Justification::centred );
        prob.setValueJustification( juce::Justification::centred );
        value.setValueJustification( juce::Justification::centred );
        note.setJustificationType( juce::Justification::centred );

        row.setFontHeight( 16 );
        type.setFontHeight( 16 );
        time.setFontHeight( 16 );
        length.setValueFontHeight( 16 );
        velocity.setValueFontHeight( 16 );
        rat.setValueFontHeight( 16 );
        prob.setValueFontHeight( 16 );
        value.setValueFontHeight( 16 );
        note.setValueFontHeight( 16 );

        value.setValue( 26, juce::dontSendNotification );
        prob.setPostFix( "%" );

        row.setTextColour( ui::Theme::textColour( 1 ) );
        time.setTextColour( ui::Theme::textColour( 1 ) );
        type.setTextColour( ui::Theme::textColour( 1 ) );
        note.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        length.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        velocity.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        value.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        prob.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        rat.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );

        row.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        time.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        note.setBackGroundColours( ui::Theme::backgroundColour( 9 ), ui::Theme::backgroundColour( 9 ) );
        length.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        velocity.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        value.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        prob.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        rat.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        setValues( Row, Vt );

        note.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            if( vt.isValid() ) vt.setProperty( mt::IDs::no, Sel, &mt::getAudioInterface().getUndoManager() );
        };
        length.onValueChanged = [&]( int V ) {
            if( vt.isValid() ) vt.setProperty( mt::IDs::length, V, &mt::getAudioInterface().getUndoManager() );
        };
        velocity.onValueChanged = [&]( int V ) {
            if( vt.isValid() ) vt.setProperty( mt::IDs::velocity, V, &mt::getAudioInterface().getUndoManager() );
        };
        prob.onValueChanged = [&]( int V ) {
            if( vt.isValid() ) vt.setProperty( mt::IDs::probability, V, &mt::getAudioInterface().getUndoManager() );
        };
        rat.onValueChanged = [&]( int V ) {
            if( vt.isValid() ) vt.setProperty( mt::IDs::ratchet, V, &mt::getAudioInterface().getUndoManager() );
        };
    }

    void ListEntry::setValues( int Row, juce::ValueTree Vt )
    {
        vt = Vt;
        row.setText( juce::String( Row + 1 ), juce::dontSendNotification );
        refreshData();
    }

    void ListEntry::setSelected( bool Select )
    {
        isSelected = Select;
        repaint();
    }

    void ListEntry::refreshData()
    {
        if( vt.isValid() )
        {
            auto& ae  = mt::getAudioInterface();
            auto& sel = ae.getSelected();

            int n = vt.getProperty( mt::IDs::no );
            int t = vt.getProperty( mt::IDs::start );
            int l = vt.getProperty( mt::IDs::length );
            int v = vt.getProperty( mt::IDs::velocity );
            int p = vt.getProperty( mt::IDs::probability );
            int r = vt.getProperty( mt::IDs::ratchet );

            auto bnb      = ae.toBnB( sel, (unsigned int)t );
            auto timeText = juce::String( bnb.bar + 1 ).paddedLeft( '0', 2 ) + ":" + juce::String( bnb.beat + 1 ).paddedLeft( '0', 2 ) + ":" + juce::String( bnb.tick ).paddedLeft( '0', 2 );
            time.setText( timeText, juce::dontSendNotification );
            note.setSelected( n, juce::sendNotification );
            length.setValue( l, juce::dontSendNotification );
            velocity.setValue( v, juce::dontSendNotification );
            prob.setValue( p, juce::sendNotification );
            rat.setValue( r, juce::sendNotification );
            type.setText( "Note", juce::sendNotification );
        }
    }

    juce::Rectangle<int> ListEntry::getElementBounds( int state )
    {
        switch( state )
        {
            case 1: return note.getBounds(); break;
            case 2: return length.getBounds(); break;
            case 3: return velocity.getBounds(); break;
            case 4: return prob.getBounds(); break;
            default: return note.getBounds(); break;
        }
    }

    void ListEntry::resized()
    {
        auto b = getLocalBounds();
        row.setBounds( b.removeFromLeft( 69 ).reduced( 8 ) );
        b.removeFromLeft( 10 );
        time.setBounds( b.removeFromLeft( 167 ).reduced( 8 ) );
        note.setBounds( b.removeFromLeft( 139 ).reduced( 8 ) );
        length.setBounds( b.removeFromLeft( 99 ).reduced( 8 ) );
        velocity.setBounds( b.removeFromLeft( 99 ).reduced( 8 ) );
        type.setBounds( b.removeFromLeft( 167 ).reduced( 8 ) );
        b.removeFromLeft( 4 );
        value.setBounds( b.removeFromLeft( 79 ).reduced( 8 ) );
        b.removeFromLeft( 4 );
        prob.setBounds( b.removeFromLeft( 79 ).reduced( 8 ) );
        rat.setBounds( b.removeFromLeft( 70 ).reduced( 8 ) );
    }

    void ListEntry::linkRotated( int ID, int Value )
    {
        switch( ID )
        {
            case 0: note.knobRotated( Value ); break;
            case 1: length.knobRotated( Value ); break;
            case 2: velocity.knobRotated( Value ); break;
            case 3: prob.knobRotated( Value ); break;
        }
    }

    ListEditTimingCorrect::ListEditTimingCorrect()
    {
        mt::utils::addAndMakeVisible( this, globalTC, timeDivisionLabel, timeDivList, triplet, type, events, swing, shiftTiming, window, strength );
        addAndMakeVisible(fromBar); addAndMakeVisible(toBar);
        const int bars = juce::jmax(1, mt::getAudioInterface().getSequenceLength(target.sequence));
        fromBar.setRange(1, bars, 1); toBar.setRange(1, bars, 1);
        toBar.setValue(bars, juce::dontSendNotification);
        events.onSelectionChanged = [this](int index, const juce::String&) { fromBar.setVisible(index == 2); toBar.setVisible(index == 2); };
        fromBar.setVisible(false); toBar.setVisible(false);

        mt::utils::setTitleFontHeight( 16, globalTC, type, events, swing, shiftTiming, window, strength );
        mt::utils::setValueFontHeight( 16, globalTC, type, events, swing, shiftTiming, window, strength, timeDivList );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), timeDivisionLabel );
        mt::utils::setValueJustification( juce::Justification::centred, timeDivisionLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), timeDivisionLabel );
        mt::utils::setValueFontHeight( 20, timeDivList, type, events );
        mt::utils::setValueFontHeight( 16, timeDivisionLabel );
        mt::utils::setJustificationType( juce::Justification::centred, timeDivList, type, events );

        mt::utils::setTitleProportion( .36f, globalTC, type, events, swing, shiftTiming, window, strength );
        mt::utils::setValueProportion( .36f, swing, shiftTiming, window, strength );
        mt::utils::setTitleJustification( juce::Justification::centredTop, globalTC, type, events, swing, shiftTiming, window, strength );
        mt::utils::setValueJustification( juce::Justification::centredBottom, swing, shiftTiming, window, strength );

        swing.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        shiftTiming.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        window.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        strength.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );

        swing.setRange( 50, 75, 1 );
        shiftTiming.setRange( -48, 48, 1 );
        window.setRange( 0, 100, 1 );
        strength.setRange( 0, 100, 1 );

        window.setValue( 100, juce::dontSendNotification );
        strength.setValue( 100, juce::dontSendNotification );

        triplet.onClick = [&]() {
            timeDivList.setListOptions( triplet.getToggleState() ? std::vector<juce::String>{"BAR", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T", "1/64T"} : std::vector<juce::String>{"BAR", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"}, timeDivList.getSelectedIndex() );
        };
        const auto options = mt::getAudioInterface().getTimingCorrect();
        globalTC.setSelected(options.enabled ? 1 : 0, juce::dontSendNotification);
        const bool isTriplet = (int)options.division % 3 == 0;
        triplet.setToggleState(isTriplet, juce::dontSendNotification);
        timeDivList.setListOptions(isTriplet ? std::vector<juce::String>{"BAR", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T", "1/64T"} : std::vector<juce::String>{"BAR", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"});
        const std::vector<int> straight{1, 2, 4, 8, 16, 32, 64}, triplets{1, 3, 6, 12, 24, 48, 96};
        const auto& divisions = isTriplet ? triplets : straight;
        for(int i = 0; i < (int)divisions.size(); ++i)
            if(divisions[(size_t)i] == (int)options.division) timeDivList.setSelected(i, juce::dontSendNotification);
        swing.setValue(options.swing, juce::dontSendNotification);
        shiftTiming.setValue(options.shift, juce::dontSendNotification);
        window.setValue(options.window, juce::dontSendNotification);
        strength.setValue(options.strength, juce::dontSendNotification);
    }

    mt::TimingCorrect ListEditTimingCorrect::getOptions()
    {
        mt::TimingCorrect options;
        const int division[] = {1, 2, 4, 8, 16, 32, 64};
        const int index = juce::jlimit(0, 6, timeDivList.getSelectedIndex());
        options.division = (mt::MTTimeBase)(index == 0 ? 1 : division[index] * (triplet.getToggleState() ? 3 : 2) / 2);
        if(auto* tempo = mt::getAudioInterface().getTempoMgr(target.sequence)) options.barTicks = tempo->getTicksPerBar();
        options.enabled = globalTC.getSelected() == 1;
        options.type = (mt::TimingCorrect::Type)type.getSelectedIndex();
        options.selectedOnly = events.getSelectedIndex() == 1;
        if(events.getSelectedIndex() == 2)
            if(auto* tempo = mt::getAudioInterface().getTempoMgr(target.sequence))
            {
                const int first = juce::jmin(fromBar.getValue(), toBar.getValue());
                const int last = juce::jmax(fromBar.getValue(), toBar.getValue());
                options.rangeStart = tempo->getTicksUpto(first - 1);
                options.rangeEnd = tempo->getTicksUpto(last);
            }
        options.swing = (int)swing.getValue(); options.shift = (int)shiftTiming.getValue();
        options.window = (int)window.getValue(); options.strength = (int)strength.getValue();
        return options;
    }

    void ListEditTimingCorrect::show()
    {
        auto* dialog = new ListEditTimingCorrect();
        auto& ae = mt::getAudioInterface();
        const auto selected = ae.getSelected();
        const auto project = ae.getValueTree();
        const auto trackState = ae.getTrackState(selected.sequence, selected.track);
        mt::utils::showOptions("TIMING CORRECT", dialog, [dialog, selected, project, trackState](int result) {
            auto& audio = mt::getAudioInterface();
            if(result == UI::APPLY && audio.getValueTree() == project && audio.getTrackState(selected.sequence, selected.track) == trackState)
            {
                const auto options = dialog->getOptions();
                const int changed = audio.applyTimingCorrect(selected, options);
                // The dialog's recording settings belong to the same sequence
                // captured on open; don't apply them to a newly selected sequence.
                if(audio.getSelectedSequence() == selected.sequence) audio.setTimingCorrect(options);
                mt::utils::showNotification(changed < 0 ? "Track is no longer available" : juce::String(changed) + " notes updated");
            }
            delete dialog;
        }, {0, 0, 650, 520});
    }

    void ListEditTimingCorrect::resized()
    {
        globalTC.setBoundsRelative( .1246f, .0985f, .3076f, .1674f );
        type.setBoundsRelative( .0892f, .3977f, .3876f, .1074f );
        swing.setBoundsRelative( .0892f, .5344f, .3876f, .1822f );
        window.setBoundsRelative( .0892f, .7487f, .3876f, .1822f );

        timeDivisionLabel.setBoundsRelative( .523f, .0985f, .3876f, .0394f );
        timeDivList.setBoundsRelative( .523f, .1576f, .2553f, .1083f );
        triplet.setBoundsRelative( .7876f, .1576f, .123f, .1083f );
        events.setBoundsRelative( .523f, .3977f, .3876f, .1074f );
        fromBar.setBoundsRelative(.0892f, .277f, .3876f, .105f);
        toBar.setBoundsRelative(.523f, .277f, .3876f, .105f);
        shiftTiming.setBoundsRelative( .523f, .5344f, .3876f, .1822f );
        strength.setBoundsRelative( .523f, .7487f, .3876f, .1822f );
    }
}
