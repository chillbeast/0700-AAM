namespace kick_ui
{

    PadPerform::PadPerform()
        : kick_ui::IScreen( kick_ui::Screen::MenuPadPerform )
    {
        mt::utils::addChildComponents( this, header, content );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        ui::connectTheme( *this, [&]() {
            header.setIcon( "Icon_PadPerform" );
        } );
    }

    void PadPerform::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        content.setBounds( b );
    }

    void PadPerform::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
    }

    int PadPerform::transposeNote( int Note )
    {
        return content.transposeNote( Note );
    }

    std::vector<int> PadPerform::transpose( int Note )
    {
        return content.transpose( Note );
    }

    void PadPerform::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int              bankOffset = ae.getBankOffset();
            auto             padNo      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
            velocity = ae.getPadVelocity(padNo, velocity);
            std::vector<int> pads;

            padNo += bankOffset;
            pads = transpose( padNo );

            for( auto& it : pads )
            {
                juce::MidiMessage Msg;

                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    Msg = juce::MidiMessage::noteOn( 1, it, (juce::uint8)velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                    Msg = juce::MidiMessage::noteOff( 1, it );

                ae.addLiveMidiMsg( ae.getSelected(), Msg );
            }
            pads.clear();
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                content.knobCommand( knob, value );
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
    }

    /// @brief
    PadPerformContent::PadPerformContent()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::with1Row ) )
    {
        mt::utils::addChildComponents( this, type, rootNote, scale, startOn, octave, bankInc, bankDec, bank, chromaticBtn, notesBtn, chordsBtn, progressionBtn, chord, scale_octave, pads.get() );
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::Padperform, chromaticBtn, notesBtn, chordsBtn, progressionBtn );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), type, chord, startOn );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), chromaticBtn, notesBtn, chordsBtn, progressionBtn, bank );
        mt::utils::setValueFontHeight( 18, chromaticBtn, notesBtn, chordsBtn, progressionBtn, bank );
        mt::utils::setTitleFontHeight( 16, scale, rootNote, octave, chord, startOn );
        mt::utils::setValueFontHeight( 20, scale, rootNote, octave, chord, startOn );
        mt::utils::setJustificationType( juce::Justification::centred, scale, rootNote, octave, chord, startOn, type );

        pads->setRow1Justification( -1, juce::Justification::centred );
        bank.setInterceptsMouseClicks( false, false );
        bank.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        bank.setValueJustification( juce::Justification::centred );
        bankInc.toFront( true );
        bankDec.toFront( true );

        selectedStartOn    = 0;
        octaveIndex        = 0;
        bankIndex          = 0;
        activeDisplayIndex = 0;
        selectedType       = 0;
        selectedScale      = 0;
        selectedOctave     = 0;
        selectedRootNote   = 0;

        mt::signals::bankChanged.connect( [&]( int index, juce::String bankName ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuPadPerform )
            {
                bankIndex = index;
                pads->setBank( bankIndex );
            }
        } );

        pads->OnBankChanged = [&]( const juce::String& bankName ) {
            bank.setText( bankName, juce::dontSendNotification );
            refreshData();
        };

        bankInc.onClick = [&]() {
            ++bankIndex;
            if( bankIndex > 7 )
                bankIndex = 7;
            pads->setBank( bankIndex );
            bankInc.setToggleState( false, juce::dontSendNotification );
        };

        bankDec.onClick = [&]() {
            --bankIndex;
            if( bankIndex < 0 )
                bankIndex = 0;
            pads->setBank( bankIndex );
            bankDec.setToggleState( false, juce::dontSendNotification );
        };

        chromaticBtn.onClick = [&]() {
            if( chromaticBtn.getToggleState() && pPadPerform )
            {
                pPadPerform->performMode = mt::PerformanceMode::CHROMATIC;
                selectedType             = 1;
                refreshData();
            }
        };

        notesBtn.onClick = [&]() {
            if( notesBtn.getToggleState() && pPadPerform )
            {
                pPadPerform->performMode = mt::PerformanceMode::NOTES;
                selectedType             = 2;
                refreshData();
            }
        };

        chordsBtn.onClick = [&]() {
            if( chordsBtn.getToggleState() && pPadPerform )
            {
                pPadPerform->performMode = mt::PerformanceMode::CHORDS;
                selectedType             = 3;
                chord.setVisible( true );
                refreshData();
            }
        };

        progressionBtn.onClick = [&]() {
            if( progressionBtn.getToggleState() && pPadPerform )
            {
                pPadPerform->performMode = mt::PerformanceMode::PROGRESSION;
                selectedType             = 4;
                refreshData();
            }
        };

        //octaveInc.onClick = [&]() {
        //    if( selectedStartOn == 1 )
        //    {
        //        octaveIndex = octaveIndex + 1;

        //        int tempOctave      = octaveIndex + selectedOctave;
        //        int tempRootNote    = selectedRootNote;
        //        int tempStartOfNote = tempRootNote + ( tempOctave * 12 );

        //        if( tempStartOfNote < 128 )
        //        {
        //            /*if( bankIndex < 7 )
        //                bankIndex++;*/
        //            pads->setBank( mt::Kick::padBankNames[bankIndex] );
        //            Controls::InputEvents::clearAllPads();
        //            displayFromNote( tempStartOfNote );

        //            // Update octave ListBox
        //            octave.setSelected( octave.getSelectedIndex() + 1 );
        //        }
        //        else
        //            octaveIndex = octaveIndex - 1;
        //    }
        //    else
        //    {
        //        auto startPadIndex = startOfNotes + ( bankIndex * 16 );
        //        if( bankIndex < 7 && startPadIndex < 112 )
        //        {
        //            int tempIndex = 0;
        //            //bankIndex++;
        //            tempIndex = startOfNotes + ( bankIndex * 16 );
        //            Controls::InputEvents::clearAllPads();
        //            displayFromNote( tempIndex );
        //        }
        //        pads->setBank( mt::Kick::padBankNames[bankIndex] );

        //        // Update octave ListBox
        //        octave.setSelected( octave.getSelectedIndex() + 1 );
        //    }
        //    octaveInc.setToggleState( false, juce::dontSendNotification );
        //};

        //octaveDec.onClick = [&]() {
        //    if( octave.getSelectedIndex() == 0 )
        //    {
        //        octaveDec.setToggleState( false, juce::dontSendNotification );
        //        return;
        //    }
        //    if( selectedStartOn == 1 )
        //    {
        //        if( octaveIndex > 0 )
        //        {
        //            octaveIndex = octaveIndex - 1;

        //            int tempOctave      = octaveIndex + selectedOctave;
        //            int tempRootNote    = selectedRootNote;
        //            int tempStartOfNote = tempRootNote + ( tempOctave * 12 );

        //            if( tempStartOfNote >= 0 )
        //            {
        //                /*if( bankIndex > 0 )
        //                    bankIndex--;*/

        //                pads->setBank( mt::Kick::padBankNames[bankIndex] );
        //                Controls::InputEvents::clearAllPads();
        //                displayFromNote( tempStartOfNote );

        //                // Update octave ListBox
        //                octave.setSelected( octave.getSelectedIndex() - 1 );
        //            }
        //            else
        //                octaveIndex = octaveIndex + 1;
        //        }
        //        octave.setSelected( octave.getSelectedIndex() - 1 );
        //    }
        //    else
        //    {
        //        if( bankIndex >= 0 )
        //        {
        //            int tempIndex = 0;
        //            // bankIndex--;
        //            tempIndex = startOfNotes + ( bankIndex * 16 );
        //            Controls::InputEvents::clearAllPads();
        //            displayFromNote( tempIndex );
        //            pads->setBank( mt::Kick::padBankNames[bankIndex] );
        //        }

        //        // Update octave ListBox
        //        octave.setSelected( octave.getSelectedIndex() - 1 );
        //    }
        //    octaveDec.setToggleState( false, juce::dontSendNotification );
        //};

        rootNote.onSelectionChanged = [&]( int Sel, const juce::String& RootNote ) {
            if( pPadPerform )
            {
                pPadPerform->performRootNote = RootNote;
                selectedRootNote             = Sel;
                refreshData();
            }
        };

        octave.onSelectionChanged = [&]( int Sel, const juce::String& Octave ) {
            if( pPadPerform )
            {
                pPadPerform->performOctave = Octave;
                if( Sel <= 10 && bankIndex < 7 )
                {
                    selectedOctave = Sel;
                    refreshData();
                }
            }
        };

        scale.onSelectionChanged = [&]( int Sel, const juce::String& Scale ) {
            if( pPadPerform )
            {
                pPadPerform->performScale = Scale;
                selectedScale             = Sel;
                refreshData();
            }
        };

        startOn.onSelectionChanged = [&]( int Sel, const juce::String& StartOnType ) {
            if( pPadPerform )
            {
                pPadPerform->performBanks = StartOnType;
                selectedStartOn           = Sel;
                refreshData();
            }
        };

        chord.onSelectionChanged = [&]( int Sel, const juce::String& Chord ) {
            if( pPadPerform )
            {
                pPadPerform->performChords = Chord;
                selectedChord              = Sel;
                refreshData();
            }
        };

        ui::connectTheme( *this, [&]() {
            chromaticBtn.setToggleButtonText( "Chromatic", "Chromatic" );
            notesBtn.setToggleButtonText( "Notes", "Notes" );
            chordsBtn.setToggleButtonText( "Chords", "Chords" );
            progressionBtn.setToggleButtonText( "Progressions", "Progressions" );
        } );
    }

    void PadPerformContent::resized()
    {
        // Left Side...
        scale.setBoundsRelative( .02f, .0456f, .315f, .14f );
        rootNote.setBoundsRelative( .02f, .2065f, .315f, .14f );
        octave.setBoundsRelative( .02f, .369f, .315f, .14f );
        chord.setBoundsRelative( .02f, .532f, .315f, .14f );
        startOn.setBoundsRelative( .02f, .695f, .315f, .14f );

        // Right Side...
        pads->setBoundsRelative( .358f, .02f, .63f, .87f );

        // Bottom Row...
        chromaticBtn.setBoundsRelative( 0.0097f, .912f, .117f, .0742f );
        notesBtn.setBoundsRelative( .1310f, .912f, .117f, .0742f );
        chordsBtn.setBoundsRelative( .2523f, .912f, .117f, .0742f );
        progressionBtn.setBoundsRelative( .3736f, .912f, .117f, .0742f );
        bank.setBoundsRelative( .8317f, .91f, .1565f, .0782f );
        bankDec.setBoundsRelative( .8317f, .912f, .0332f, .0742f );
        bankInc.setBoundsRelative( .9549f, .912f, .0332f, .0742f );
    }

    void PadPerformContent::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            int bankId;
            int btnId = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            DBG( "Pad Perform: " << btnId );
            switch( btnId )
            {
                // Bank buttons...
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A: pads->setBank( "A" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B: pads->setBank( "B" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C: pads->setBank( "C" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D: pads->setBank( "D" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_E: pads->setBank( "E" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_F: pads->setBank( "F" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_G: pads->setBank( "G" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_H: pads->setBank( "H" ); break;
                default: break; // DBG( "Unknown" ); break;
            }
        }
    }
    void PadPerformContent::knobCommand( int knob, int value )
    {
        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
                type.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_2:
                scale.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_3:
                rootNote.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_4:
                scale.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_5:
                chord.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_6:
                startOn.knobRotated( value );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_7:
                octave.knobRotated( value );
                break;
            default:
                type.knobRotated( value );
                break;
        }
    }

    void PadPerformContent::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( isVisible() || isShowing() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            pPadPerform = ae.getPadPerformanceData();
            refresh();
            //refreshData();
        }
        else
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );

            contracts::DclSignals::refreshPadLeds();
        }
    }

    void PadPerformContent::refresh()
    {
        if( pPadPerform )
        {
            selectedRootNote = rootNote.getIndex( pPadPerform->performRootNote.get() );
            selectedChord    = chord.getIndex( pPadPerform->performChords.get() );
            selectedOctave   = octave.getIndex( pPadPerform->performOctave.get() );
            selectedScale    = scale.getIndex( pPadPerform->performScale.get() );
            selectedStartOn  = startOn.getIndex( pPadPerform->performBanks.get() );
            scale.setSelected( pPadPerform->performScale.get(), juce::dontSendNotification );
            octave.setSelected( pPadPerform->performOctave.get(), juce::dontSendNotification );
            rootNote.setSelected( pPadPerform->performRootNote.get(), juce::dontSendNotification );
            chord.setSelected( pPadPerform->performChords.get(), juce::dontSendNotification );
            startOn.setSelected( pPadPerform->performBanks.get(), juce::dontSendNotification );
            auto padPerformMode = pPadPerform->performMode.get();
            chromaticBtn.setToggleState( padPerformMode == mt::PerformanceMode::CHROMATIC, juce::sendNotification );
            notesBtn.setToggleState( padPerformMode == mt::PerformanceMode::NOTES, juce::sendNotification );
            chordsBtn.setToggleState( padPerformMode == mt::PerformanceMode::CHORDS, juce::sendNotification );
            progressionBtn.setToggleState( padPerformMode == mt::PerformanceMode::PROGRESSION, juce::sendNotification );
        }
    }

    int PadPerformContent::transposeNote( int Note )
    {
        auto idx = juce::jlimit( MIDI::MIN, MIDI::MAX, Note );
        return noteData[idx].activeDisplayIndex;
    }

    std::vector<int> PadPerformContent::transpose( int Note )
    {
        std::vector<int> notes;

        // Root note...
        auto idx = transposeNote( Note );
        notes.push_back( idx );

        auto noteMappingType = (enumNoteTypes)( selectedType );
        if( EN_TYPE_CHORDS == noteMappingType )
        {
            auto chrd = chord.getSelected();
            auto it   = mt::ScaleAndNotes::chordFormulaMap.find( chrd );
            if( it != mt::ScaleAndNotes::chordFormulaMap.end() )
            {
                auto& chordFormula = it->second;
                for( auto i : chordFormula )
                {
                    notes.push_back( idx + i );
                }
            }
        }

        return notes;
    }

    void PadPerformContent::refreshData()
    {
        pads->refreshAllPads();
        selectionChanged();
        pushToUI();
        pushToHW();
    }

    void PadPerformContent::selectionChanged()
    {
        //bankIndex          = 0;
        octaveIndex        = 0;
        activeDisplayIndex = 0;
        //Clear Pad Names
        pads->clearAllPadData();
        if( EN_TYPE_CHORDS != selectedType && EN_TYPE_PROGRESSION != selectedType )
            chord.setVisible( false );

        updateNoteData();
        pushToUI();
        pushToHW();
        pushPadInfoToDisplay();
    }

    void PadPerformContent::pushPadInfoToHw( int id, enumNoteMode f_enNoteMode )
    {
        int  colourId = 0;
        auto state    = contracts::DeviceControlContracts::PadButtonState::StateOn;

        if( f_enNoteMode == EN_NOTE_ROOT )
            colourId = 4;
        else if( f_enNoteMode == EN_NOTE_ACTIVE )
            colourId = 1;
        else
        {
            colourId = 0;
        }
        id = id % 16;

        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setPadDataMessage( id, state ? contracts::DeviceControlContracts::PadButtonState::StateOn : contracts::DeviceControlContracts::PadButtonState::StateOff,
                                                                        convertToHwColour( colourId ) ) );
    }

    void PadPerformContent::pushPadInfoToDisplay()
    {
        int           noteIndex         = 0;
        int           scaleFormulaStep  = 0;
        int           scaleFormulaValue = 0;
        int           scaleFormulaIndex = 0;
        enumNoteTypes enNoteTypes       = (enumNoteTypes)selectedType;
        enumNoteMode  enNoteMode        = EN_NOTE_NOT_AVAILABLE;

        startOfNotes = juce::jlimit( MIDI::MIN, MIDI::MAX, selectedRootNote + ( selectedOctave * 12 ) );

        if( mt::ScaleAndNotes::maxMidiNote < startOfNotes )
            return;

        juce::String selectedRootNoteName = rootNote.getSelectedValue();

        enum mt::ScaleAndNotes::scale_type scaleFormTypes = ( mt::ScaleAndNotes::scale_type )( 0 + selectedScale );

        std::vector<int> scaleFormula = mt::ScaleAndNotes::scaleFormulaMap.at( scaleFormTypes );

        mt::MTPadColors padData     = { mt::EmptyPadColorMode::Off, mt::PadColorMode::OFF, { 0 } };
        int             padColourId = 0;

        for( int index = 0; index < 128; index++ )
        {
            juce::String noteName       = mt::ScaleAndNotes::midiNoteNames[index];
            st_NoteData[index].noteName = noteName;
            // Check for Note mode
            if( index >= startOfNotes )
            {
                // Check for Note Mode
                if( scaleFormulaStep == 0 )
                {
                    scaleFormulaStep = scaleFormula[scaleFormulaIndex];
                    // Upper limit check
                    scaleFormulaIndex++;
                    scaleFormulaIndex = ( scaleFormulaIndex >= scaleFormula.size() ) ? 0 : ( scaleFormulaIndex );
                    enNoteMode        = EN_NOTE_ACTIVE;
                }
                else
                {
                    enNoteMode = EN_NOTE_INACTIVE;
                }
                scaleFormulaStep--;

                // Check for Root Note

                if( ( selectedRootNoteName[0] == noteName[0] ) && ( noteName[1] != '#' ) )
                {
                    enNoteMode = EN_NOTE_ROOT;
                }

                noteIndex = index - startOfNotes;

                st_NoteData[index].enNoteMode = enNoteMode;
                switch( enNoteTypes )
                {
                    case EN_TYPE_OFF:
                    case EN_TYPE_CHROMATIC:
                    case EN_TYPE_CHORDS:
                    case EN_TYPE_PROGRESSION:
                    case EN_TYPE_CHROMATIC_CHORDS:
                    {
                        padColourId = pushPadInfoToUI( noteIndex, noteName, enNoteMode );
                        break;
                    }
                    case EN_TYPE_NOTES:
                    default:
                    {
                        if( ( enNoteMode == EN_NOTE_ACTIVE ) || ( enNoteMode == EN_NOTE_ROOT ) )
                        {
                            st_NoteData[activeDisplayIndex].activeDisplayIndex = activeDisplayIndex;
                            padColourId                                        = pushPadInfoToUI( activeDisplayIndex, noteName, enNoteMode );
                            activeDisplayIndex++;
                        }
                        break;
                    }
                }
                padData.Colours[noteIndex] = padColourId;
            }
            else
            {
                enNoteMode                    = EN_NOTE_NOT_AVAILABLE;
                st_NoteData[index].enNoteMode = enNoteMode;
            }
        }
        displayFromNote( startOfNotes );

        if( noteIndex != 0 )
        {
            ae.setKeyGroupPadColour( padData );
        }
        scaleFormula.clear();
    }

    int PadPerformContent::pushPadInfoToUI( int id, const juce::String& text, enumNoteMode f_enNoteMode )
    {
        static bool displayPad = false;
        int         colourId   = 0;
        auto        state      = contracts::DeviceControlContracts::PadButtonState::StateOn;

        pads->setActive( id, true );
        pads->setRow1Text( id, text, juce::dontSendNotification );
        if( f_enNoteMode == EN_NOTE_ROOT )
            colourId = 4;
        else if( f_enNoteMode == EN_NOTE_ACTIVE )
            colourId = 1;
        else
        {
            colourId = 0;
            pads->setActive( id, false );
        }
        pads->setBorderColor( id, ui::Theme::highlightColour( colourId ), 3.0f, juce::dontSendNotification );
        return convertToHwColour( colourId );
    }

    int PadPerformContent::convertToHwColour( int colourId )
    {
        int returnColourId = 0;
        switch( colourId )
        {
            case 0:
                returnColourId = (int)contracts::DeviceControlContracts::PadColourIds::PadColourTrans;
                break;
            case 1:
                returnColourId = (int)contracts::DeviceControlContracts::PadColourIds::padColourDarkYellow;
                break;
            case 2:
                break;
            case 3:
                break;
            case 4:
                returnColourId = (int)contracts::DeviceControlContracts::PadColourIds::PadColourRed;
                break;
            case 5:
                break;
            case 6:
                break;
            case 7:
                break;
        }
        return returnColourId;
    }

    void PadPerformContent::displayFromNote( int f_padIndex )
    {
        int  visualIndex = 0;
        int  padIndex    = f_padIndex;
        int  colourId    = 0;
        auto state       = contracts::DeviceControlContracts::PadButtonState::StateOn;

        enumNoteMode  enNoteMode  = EN_NOTE_NOT_AVAILABLE;
        enumNoteTypes enNoteTypes = (enumNoteTypes)selectedType;

        while( visualIndex != 16 )
        {
            juce::String noteName = st_NoteData[padIndex].noteName;
            enNoteMode            = st_NoteData[padIndex].enNoteMode;
            switch( enNoteTypes )
            {
                case EN_TYPE_OFF:
                case EN_TYPE_CHROMATIC:
                case EN_TYPE_CHORDS:
                case EN_TYPE_PROGRESSION:
                case EN_TYPE_CHROMATIC_CHORDS:
                {
                    pushPadInfoToUI( ( ( bankIndex * 16 ) + visualIndex ), noteName, enNoteMode );
                    pushPadInfoToHw( ( ( bankIndex * 16 ) + visualIndex ), enNoteMode );
                    visualIndex++;
                    break;
                }
                case EN_TYPE_NOTES:
                default:
                {
                    if( ( enNoteMode == EN_NOTE_ACTIVE ) || ( enNoteMode == EN_NOTE_ROOT ) )
                    {
                        pushPadInfoToUI( ( ( bankIndex * 16 ) + visualIndex ), noteName, enNoteMode );
                        pushPadInfoToHw( ( ( bankIndex * 16 ) + visualIndex ), enNoteMode );
                        visualIndex++;
                    }
                    break;
                }
            }

            padIndex++;
            if( padIndex >= 128 )
                break;
        }
    }

    void PadPerformContent::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( { 10.0f, 10.0f, 342.0f, 396.0f }, 5.0f );
    }
    void PadPerformContent::updateNoteNames( void )
    {
    }

    void PadPerformContent::updateNoteData( void )
    {
        clearNoteData();

        auto octaveNum       = octave.getSelectedIndex();
        auto rootNoteNum     = rootNote.getSelectedIndex() + ( octaveNum * 12 );
        auto scaleNum        = scale.getSelectedIndex();
        auto noteMappingType = (enumNoteTypes)( selectedType );

        auto& midiNoteNames = mt::ScaleAndNotes::noteNames();
        auto& scaleFormula  = mt::ScaleAndNotes::scaleFormulaMap.at( ( mt::ScaleAndNotes::scale_type )( scaleNum ) );

        auto max       = noteData.size();
        int  noteIndex = rootNoteNum;
        int  scaleIdx  = noteIndex;
        int  scaleIt   = 0;

        enumNoteMode noteMode;
        bool         isInScale = ( ( EN_TYPE_NOTES == noteMappingType ) || ( EN_TYPE_CHORDS == noteMappingType ) || ( EN_TYPE_CHROMATIC_CHORDS == noteMappingType ) );
        for( auto i = 0; i < max; ++i )
        {
            if( selectedStartOn == 1 )
            {
                if( ( i % 16 == 0 ) && ( i != 0 ) )
                {
                    noteIndex = noteIndex - 4;
                    scaleIdx  = scaleIdx - 4;
                    scaleIt   = 0;
                }
            }
            noteData[i].activeDisplayIndex = noteIndex;
            noteData[i].noteName           = midiNoteNames[noteIndex];

            if( noteIndex == scaleIdx )
            {
                noteMode = ( 0 == scaleIt ) ? EN_NOTE_ROOT : EN_NOTE_ACTIVE;

                auto inc = scaleFormula[(size_t)scaleIt];
                scaleIdx += inc;

                if( isInScale )
                    noteIndex += ( inc - 1 ); // noteIndex will be incremented by one later, hence pre-substract...

                if( ++scaleIt >= (int)scaleFormula.size() )
                    scaleIt = 0;
            }
            else
                noteMode = EN_NOTE_INACTIVE;

            noteData[i].enNoteMode   = noteMode;
            noteData[i].borderColour = ( EN_NOTE_ROOT == noteMode )   ? ui::Theme::highlightColour( 2 ) :
                                       ( EN_NOTE_ACTIVE == noteMode ) ? ui::Theme::highlightColour( 1 ) :
                                                                        ui::Theme::highlightColour( 0 );
            noteData[i].colour       = ( EN_NOTE_ROOT == noteMode )   ? contracts::DeviceControlContracts::PadColourIds::PadColourRed :
                                       ( EN_NOTE_ACTIVE == noteMode ) ? contracts::DeviceControlContracts::PadColourIds::padColourDarkYellow :
                                                                        contracts::DeviceControlContracts::PadColourIds::PadColourTrans;

            if( ++noteIndex >= 128 ) break;
        }
    }

    void PadPerformContent::clearNoteData( void )
    {
        for( auto& note : noteData )
            note.clear();
    }

    void PadPerformContent::pushToUI( void )
    {
        /*for( auto i = 0; i < 128; ++i )
        {
            auto& note = noteData[i];

            pads->setRow1Text( i, note.noteName, juce::dontSendNotification );
            pads->setBorderColor( i, note.borderColour, 3.0f, juce::dontSendNotification );
            pads->setActive( i, note.enNoteMode != EN_NOTE_INACTIVE );
        }*/

        auto idx = juce::jlimit( MIDI::MIN, MIDI::MAX, bankIndex * 16 );
        for( auto i = 0; i < 16; ++i, ++idx )
        {
            auto& note = noteData[idx];
            pads->setRow1Text( idx, note.noteName, juce::dontSendNotification );
            pads->setBorderColor( idx, note.borderColour, 3.0f, juce::dontSendNotification );
            pads->setActive( idx, note.enNoteMode != EN_NOTE_INACTIVE );
        }
    }

    void PadPerformContent::pushToHW( void )
    {
        auto             idx = juce::jlimit( MIDI::MIN, MIDI::MAX, bankIndex * 16 );
        std::vector<int> colourId;
        for( auto i = 0; i < 16; ++i, ++idx )
        {
            auto& note = noteData[idx];
            colourId.push_back( note.colour );
        }
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setAllPadDataColourInfoMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, colourId ) );
    }
}
