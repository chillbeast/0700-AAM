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
    class SequenceMeterOptions : public juce::Component
    {
    public:
        ui::KickNumber numerator{"BEATS PER BAR", 1, 32, 4};
        ui::KickListBox denominator{"BEAT UNIT", {"1", "2", "4", "8", "16", "32", "64"}, 2};
        SequenceMeterOptions(int num, int den)
        {
            addAndMakeVisible(numerator); addAndMakeVisible(denominator);
            numerator.setValue(num, juce::dontSendNotification);
            denominator.setSelected(juce::String(den), juce::dontSendNotification);
        }
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(20);
            numerator.setBounds(bounds.removeFromLeft(bounds.getWidth() / 2).reduced(10));
            denominator.setBounds(bounds.reduced(10));
        }
    };

    class APPSequenceOptionDeleteBars : public juce::Component
    {
    public:
        APPSequenceOptionDeleteBars( const juce::String& Title, bool insert )
            : isInsert( insert )
        {
            setName( Title );
            mt::utils::addAndMakeVisible( this, slide1, slide2 );

            slide1.showTitle( true );
            slide1.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );
            slide1.setTitleJustification( juce::Justification::centredTop );
            slide1.setValueJustification( juce::Justification::centredBottom );
            slide1.setValueFontHeight( 18 );
            slide1.setTitleFontHeight( 16 );
            slide1.setNumDecimalPlacesToDisplay( 0 );

            slide2.showTitle( true );
            slide2.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );
            slide2.setTitleJustification( juce::Justification::centredTop );
            slide2.setValueJustification( juce::Justification::centredBottom );
            slide2.setValueFontHeight( 18 );
            slide2.setTitleFontHeight( 16 );
            slide2.setNumDecimalPlacesToDisplay( 0 );

            slide1.setTitle( isInsert ? "No of bars" : "From" );
            slide2.setTitle( isInsert ? "Before bar" : "To" );

            auto& ae      = mt::getAudioInterface();
            auto  seqData = ae.getSequenceData( ae.getSelectedSequence() );

            slide1.setRange( isInsert ? 1 : 0, isInsert ? 32 : (int)seqData->length.get(), 1 );
            slide2.setRange( 1, seqData->length.get() + 1, 1 );
        };

        const int getNoBars( void )
        {
            noBars = (int)( isInsert ? slide1.getValue() : ( slide2.getValue() - slide1.getValue() ) );
            return noBars;
        }
        const int getPos( void )
        {
            pos = (int)( isInsert ? slide2.getValue() - 1 : juce::jmin( slide2.getValue() + 1, slide2.getMaximum() ) );
            return pos;
        }

    private:
        bool                    isInsert;
        ui::KickSliderWithTitle slide1{ "No of bars" };
        ui::KickSliderWithTitle slide2{ "Before bar" };
        int                     noBars{ 0 }, pos{ 0 };

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 1 ) );
        };
        void resized() override
        {
            slide1.setBoundsRelative( .05f, .05f, .40f, 0.3f );
            slide2.setBoundsRelative( .55f, .05f, .40f, 0.3f );
        };
    };

    class AppCopyEvents : public juce::Component
    {
    public:
        AppCopyEvents()
        {
            mt::utils::addAndMakeVisible( this, source, destination, sourceTrack, destinationTrack, pads, events, bar, beat, tick, dbar, dbeat, dtick, ebar, ebeat, etick );

            source.onSelectionChanged = [&]( int row, const juce::String& ) {
                ae.setSelectedSequence( row );
                trackList.clear();
                trackList = ae.getTrackList( ae.getSelectedSequence() );
                sourceTrack.setListOptions( trackList, ae.getSelectedSequence() );
            };
            destination.onSelectionChanged = [&]( int row, const juce::String& ) {
                ae.setSelectedSequence( row );
                trackList.clear();
                trackList = ae.getTrackList( ae.getSelectedSequence() );
                destinationTrack.setListOptions( trackList, ae.getSelectedSequence() );
            };

            sourceTrack.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
                ae.setSelectedTrack( Index );
            };

            updateList();
            events.setListOptions( { "Copy All Events", "Copy Selected Events" } );
            std::vector<juce::String> padsList;
            padsList.push_back( "All Pads" );
            int letterAsciiCode = 65;

            for( int i = 0; i <= kick_Constants::totalPianoKeys; i += 16 )
            {
                for( int j = 0; j < 16; j++ )
                {
                    juce::String noteLabel;
                    noteLabel = noteLabel.formatted( "%c%02d", letterAsciiCode, ( j + 1 ) );
                    padsList.push_back( noteLabel );
                }
                letterAsciiCode++;
            }
            pads.setListOptions( padsList );
            auto numBars = ae.getSequenceLength( ae.getSelectedSequence() );
            ebar.setValue( numBars + 1 );
            ebar.setRange( 1, numBars + 1 );
            dbar.setValue( numBars + 1 );
            dbar.setRange( 1, numBars + 1 );
        }
        int  getSourceTrack() { return sourceTrack.getSelectedIndex(); };
        int  getDestinationTrack() { return destinationTrack.getSelectedIndex(); };
        int  getDestinationSequence() { return destination.getSelectedIndex(); };
        int  getSourceSequence() { return source.getSelectedIndex(); };
        int  getSelectedPad() { return pads.getSelectedIndex() - 1; };
        bool copySelection() { return events.getSelectedIndex() == 1; };
        int  getStartBar() const { return bar.getValue(); }
        int  getStartBeat() const { return beat.getValue(); }
        int  getStartTick() const { return tick.getValue(); }
        int  getEndBar() const { return ebar.getValue(); }
        int  getEndBeat() const { return ebeat.getValue(); }
        int  getEndTick() const { return etick.getValue(); }
        int  getDestBar() const { return dbar.getValue(); }
        int  getDestBeat() const { return dbeat.getValue(); }
        int  getDestTick() const { return dtick.getValue(); }

    private:
        mt::MTAudioInterface&     ae{ mt::getAudioInterface() };
        ui::KickListBox           source{ "SEQUENCE" };
        ui::KickListBox           sourceTrack{ "TRACK" };
        ui::KickListBox           destination{ "SEQUENCE" };
        ui::KickListBox           destinationTrack{ "TRACK" };
        ui::KickListBox           pads{ "PADS" };
        ui::KickListBox           events{ "EVENTS" };
        ui::KickNumber            bar{ "BAR", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            beat{ "BEAT", BAR::MIN, BAR::MAX };
        ui::KickNumber            tick{ "TICK" };
        ui::KickNumber            dbar{ "BAR", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            dbeat{ "BEAT", BAR::MIN, BAR::MAX };
        ui::KickNumber            dtick{ "TICK" };
        ui::KickNumber            ebar{ "BAR", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            ebeat{ "BEAT", BAR::MIN, BAR::MAX };
        ui::KickNumber            etick{ "TICK" };
        std::vector<juce::String> seqList{};
        std::vector<juce::String> trackList{};

        void resized() override
        {
            source.setBounds( 20, 10, 240, 60 );
            sourceTrack.setBounds( 360, 10, 240, 60 );
            pads.setBounds( 20, 80, 240, 60 );
            events.setBounds( 360, 80, 240, 60 );
            bar.setBounds( 20, 160, 64, 64 );
            beat.setBounds( 104, 160, 64, 64 );
            tick.setBounds( 188, 160, 64, 64 );
            ebar.setBounds( 360, 160, 64, 64 );
            ebeat.setBounds( 444, 160, 64, 64 );
            etick.setBounds( 528, 160, 64, 64 );
            destination.setBounds( 20, 244, 240, 60 );
            destinationTrack.setBounds( 360, 244, 240, 60 );
            dbar.setBounds( 200, 324, 64, 64 );
            dbeat.setBounds( 284, 324, 64, 64 );
            dtick.setBounds( 368, 324, 64, 64 );
        }

        void updateList()
        {
            seqList.clear();
            trackList.clear();
            seqList   = ae.getSequenceList();
            trackList = ae.getTrackList( ae.getSelectedSequence() );

            source.setNumColumns( ae.getNumSequences() );
            source.setListOptions( seqList, ae.getSelectedSequence() );

            sourceTrack.setNumColumns( ae.getNumSequences() );
            sourceTrack.setListOptions( trackList, ae.getSelectedSequence() );

            destination.setNumColumns( ae.getNumSequences() );
            destination.setListOptions( seqList, ae.getSelectedSequence() );

            destinationTrack.setNumColumns( ae.getNumSequences() );
            destinationTrack.setListOptions( trackList, ae.getSelectedTrack() );
        }
    };

    class APPSequenceOptionCopyBars : public juce::Component
    {
    public:
        APPSequenceOptionCopyBars( const juce::String Title )
        {
            setName( Title );
            mt::utils::addAndMakeVisible( this, fromBar, toBar, afterBar, replaceMerge, fromSequence, toSequence );

            fromBar.showTitle( true );
            fromBar.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );
            fromBar.setTitleJustification( juce::Justification::centredTop );
            fromBar.setValueJustification( juce::Justification::centredBottom );
            fromBar.setValueFontHeight( 18 );
            fromBar.setTitleFontHeight( 16 );
            fromBar.setNumDecimalPlacesToDisplay( 0 );

            toBar.showTitle( true );
            toBar.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );
            toBar.setTitleJustification( juce::Justification::centredTop );
            toBar.setValueJustification( juce::Justification::centredBottom );
            toBar.setValueFontHeight( 18 );
            toBar.setTitleFontHeight( 16 );
            toBar.setNumDecimalPlacesToDisplay( 0 );

            afterBar.showTitle( true );
            afterBar.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );
            afterBar.setTitleJustification( juce::Justification::centredTop );
            afterBar.setValueJustification( juce::Justification::centredBottom );
            afterBar.setValueFontHeight( 18 );
            afterBar.setTitleFontHeight( 16 );
            afterBar.setNumDecimalPlacesToDisplay( 0 );

            auto& ae      = mt::getAudioInterface();
            auto  seqData = ae.getSequenceData( ae.getSelectedSequence() );

            fromBar.setRange( 0, seqData->length.get(), 1 );
            toBar.setRange( 0, seqData->length.get(), 1 );
            afterBar.setRange( 0, seqData->length.get(), 1 );

            fromBar.setValue( seqData->length.get() );
            toBar.setValue( seqData->length.get() );
            afterBar.setValue( seqData->length.get() );

            fromSequence.setListOptions( ae.getSequenceList(), 0 );
            toSequence.setListOptions( ae.getSequenceList(), 0 );

            toBar.onValueChanged = [&]( int value ) {
                if( value < fromBar.getValue() )
                {
                    fromBar.setRange( 0, value, 1 );
                    fromBar.setValue( value );
                }
            };

            fromSequence.onSelectionChanged = [&]( int sequenceIndex, juce::String ) {
                auto seqLength = ae.getSequenceLength( sequenceIndex );
                fromBar.setRange( 0, seqLength, 1 );
                toBar.setRange( 0, seqLength, 1 );
                fromBar.setValue( seqLength );
                toBar.setValue( seqLength );
            };

            toSequence.onSelectionChanged = [&]( int sequenceIndex, juce::String ) {
                auto seqLength = ae.getSequenceLength( sequenceIndex );
                afterBar.setRange( 0, seqLength, 1 );
                afterBar.setValue( seqLength );
            };
        };

        const int getFromBar( void )
        {
            fromBarVal = (int)( fromBar.getValue() );
            return fromBarVal;
        }

        const int getToBar( void )
        {
            toBarVal = (int)( toBar.getValue() );
            return toBarVal;
        }

        const int getCopyPos( void )
        {
            copyPosition = (int)( afterBar.getValue() );
            return copyPosition;
        }

        const int getNumOfBars( void )
        {
            return toBar.getValue() - fromBar.getValue() + 1;
        }

        const bool isMerge()
        {
            return replaceMerge.getToggleState();
        }

        const int getFromSequenceIndex()
        {
            return fromSequence.getSelectedIndex();
        }

        const int getToSequenceIndex()
        {
            return toSequence.getSelectedIndex();
        }

    private:
        ui::KickListBox                       fromSequence{ "From", 1 };
        ui::KickListBox                       toSequence{ "To", 1 };
        ui::KickSliderWithTitle               fromBar{ "From bar" };
        ui::KickSliderWithTitle               toBar{ "To bar" };
        ui::KickSliderWithTitle               afterBar{ "After bar" };
        ui::KickToggleButtonWithAttachedValue replaceMerge{ "REPLACE", "MERGE", "REPLACE" };
        int                                   fromBarVal{ 0 }, toBarVal{ 0 }, copyPosition{ 0 };

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 1 ) );
        };

        void resized() override
        {
            fromBar.setBoundsRelative( .05f, .05f, .40f, 0.3f );
            toBar.setBoundsRelative( .55f, .05f, .40f, 0.3f );
            afterBar.setBoundsRelative( .05f, .40f, .40f, 0.3f );
            replaceMerge.setBoundsRelative( .55f, .47f, .42f, 0.2f );
            fromSequence.setBoundsRelative( .05f, .74f, .42f, 0.2f );
            toSequence.setBoundsRelative( .55f, .74f, .42f, 0.2f );
        };
    };

    AppSequenceOptions::AppSequenceOptions( void )
        : seq( 0 )
    {
        setName( "Sequence Options" );
        setVisible( false );

        mt::utils::addAndMakeVisible( this, srNo, seq, opts, close );
        srNo.setInterceptsMouseClicks( false, false );

        close.onClick = [&]() {
            if( onClose )
                onClose();
        };

        opts.onMenuOptionClicked = [&]( const juce::String& /*option*/, int Index, const juce::String& /*group*/ ) {
            auto sel = ae.getSelected();
            switch( Index )
            {
                break;
                case 0:; // Half Length
                    ae.setSequenceLength( ae.getSelectedSequence(), .5f );
                    break;
                case 1:; // Double Length
                    ae.setSequenceLength( ae.getSelectedSequence(), 2.0f );
                    break;
                case 2: // Time Signature
                {
                    if(ae.isSequencePlaying()) { mt::utils::showNotification("Stop playback before changing the time signature"); break; }
                    auto* tempo = ae.getTempoMgr(sel.sequence);
                    if(!tempo) break;
                    auto* dialog = new SequenceMeterOptions(tempo->getNumerator(), tempo->getDenominator());
                    const auto state = ae.getSequenceState(sel.sequence);
                    mt::utils::showOptions("TIME SIGNATURE", dialog, [dialog, state, sel](int result) {
                        auto& audio = mt::getAudioInterface();
                        if(result == UI::APPLY && audio.getSequenceState(sel.sequence) == state)
                            if(audio.setTimeSignature(sel.sequence, dialog->numerator.getValue(), dialog->denominator.getSelected().getIntValue()) != 0)
                                mt::utils::showNotification("Stop playback before changing the time signature");
                        delete dialog;
                    }, {0, 0, 540, 220});
                    break;
                }
                /* case 3: // Bounce to Sample
                {
                    juce::File f( ae.getProjectDir().getChildFile( "test.wav" ) );
                    char       c;
                    f.replaceWithData( &c, 0 );
                    ae.sequenceBounceToSample( ae.getSelectedSequence(), f );
                }
                break;*/
                case 3: // Insert Bars
                {
                    auto p = new APPSequenceOptionDeleteBars( "INSERT BARS", true );
                    mt::utils::showOptions( p->getName(), p, [&, p]( int R ) {
                        if( R == UI::APPLY )
                        {
                            ae.modifySequenceLength( ae.getSelectedSequence(), p->getNoBars(), true, p->getPos() );
                        }

                        if( p ) delete p;
                        seq.updateData();
                    } );
                }
                break;
                case 4: //Copy Events
                {
                    auto p = new AppCopyEvents();
                    mt::utils::showOptions(
                        "COPY EVENTS", p, [this, p, sel]( int R ) {
                            if( R == UI::APPLY )
                            {
                                auto&                     gridData     = GridData::Instance();
                                auto                      track        = p->getDestinationTrack();
                                auto                      strack       = p->getSourceTrack();
                                auto                      seq          = p->getDestinationSequence();
                                auto                      sSeq         = p->getSourceSequence();
                                auto                      pad          = p->getSelectedPad();
                                auto                      copyEvent    = p->copySelection();
                                auto                      startBar     = p->getStartBar();
                                auto                      endBar       = p->getEndBar();
                                auto                      destBar      = p->getDestBar();
                                auto                      startPos     = 0;
                                auto                      destStartPos = 0;
                                mt::track::copyTrackNotes copyNotes;
                                copyNotes.FromSeq    = sSeq;
                                copyNotes.ToTrack    = track;
                                copyNotes.FromTrack  = strack;
                                copyNotes.ToSeq      = seq;
                                copyNotes.CopyEvents = copyEvent;
                                if( pad != -1 )
                                {
                                    copyNotes.SourceRange.noteStart = copyNotes.SourceRange.noteEnd = pad;
                                    copyNotes.DestinationRange.noteStart = copyNotes.DestinationRange.noteEnd = pad;
                                }

                                if( startBar != 1 )
                                    startPos = gridData.getTicksUptoBar( startBar - 1 );
                                auto endPos = gridData.getTicksUptoBar( endBar - 1 );
                                if( destBar != 1 )
                                    destStartPos = gridData.getTicksUptoBar( destBar - 1 );
                                auto range  = endBar - startBar;
                                auto Length = destBar + range - 1;
                                ae.setSequenceLength( seq, Length );
                                copyNotes.SourceRange.startTime      = startPos;
                                copyNotes.SourceRange.endTime        = endPos;
                                copyNotes.DestinationRange.startTime = destStartPos;
                                copyNotes.DestinationRange.endTime   = destStartPos + ( endPos - startPos );
                                ae.copyNotesTo( sel, copyNotes );
                            }
                            if( p ) delete p;
                        } );
                }
                break;
                case 5: // Copy Bar
                {
                    auto p = new APPSequenceOptionCopyBars( "COPY BARS" );
                    mt::utils::showOptions( p->getName(), p, [&, p]( int R ) {
                        mt::MTBarNBeats fromBar{ p->getFromBar(), 0 }, toBar{ p->getToBar(), 0 }, copyPos{ p->getCopyPos(), 0 }, numOfBars{ p->getNumOfBars(), 0 };
                        if( R == UI::APPLY )
                        {
                            GridData&            gridData{ GridData::Instance() };
                            mt::SequenceCopyBars copyBars;
                            copyBars.startBar       = p->getFromBar();
                            copyBars.endBar         = p->getToBar();
                            copyBars.sourceSequence = p->getFromSequenceIndex();
                            copyBars.destSequence   = p->getToSequenceIndex();
                            copyBars.atPositionBar  = p->getCopyPos() + 1;
                            copyBars.isMerge        = p->isMerge();

                            copyBars.noteRange.startTime = gridData.getTicksUptoBar( p->getFromBar() - 1 );
                            copyBars.noteRange.endTime   = gridData.getTicksUptoBar( p->getToBar() );
                            copyBars.numBars             = p->getNumOfBars();
                            ae.copyTrackBars( copyBars );
                            if( refreshSequence )
                                refreshSequence();
                        }
                        if( p ) delete p;
                        seq.updateData();
                    } );
                }
                break;
                case 6:
                {
                    ae.copySequence( ae.getSelectedSequence() );
                    if( onClose )
                        onClose();
                }
                break;
                case 7:
                {
                    auto p = new ui::FilePopup{ "Are You Sure You Want To Clear This Sequence ?", false, false };
                    mt::utils::showOptions(
                        "WARNING!", p, [&, p]( int R ) {
                            if( R == UI::APPLY )
                                ae.clearSequence( ae.getSelectedSequence() );
                            if( p ) delete p;
                        },
                        ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                    if( onClose )
                        onClose();
                }
                break;
            }
            seq.updateData();
        };

        ui::connectTheme( *this, [&]() {
            srNo.setImage( "ID_sequence" );
            close.setImages( "Edit_Close" );
        } );
    }

    void AppSequenceOptions::handleCommand( CPktMgr::HWMsg_t& HwMsg )
    {
        auto fc = ui::getComponentInFocus();
        if( nullptr == fc || &opts == fc )
            opts.handleCommand( HwMsg );
    }

    //==============================================================================
    void AppSequenceOptions::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
        mt::utils::fillRoundedRect( g, { 0, 0, 1000, 386 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 956, 0, 43, 386 }, ui::Theme::backgroundColour( 12 ) );
    }

    void AppSequenceOptions::resized()
    {
        srNo.setBoundsRelative( .0f, .0f, .0429f, .9698f );
        seq.setBoundsRelative( .0f, .0f, .9765f, .2814f );
        opts.setBoundsRelative( .0585f, .2814f, .8544f, .6884f );
        close.setBoundsRelative( .9531f, .0234f, 0.023f, 0.05633f );
    }

    void AppSequenceOptions::visibilityChanged()
    {
        if( isVisible() )
        {
            seq.setSequence( mt::getAudioInterface().getSelectedSequence() );
            srNo.setText( juce::String( seq.getSeqNo() + 1 ) );
        }
        opts.visibilityChanged();
    }

    AppSequenceOptionsMenu::AppSequenceOptionsMenu()
        : KickMenuComponent( "SequenceOptions", 3 )
    {
        addOption( { "HalfLength", "HalfLength" }, "" );
        addOption( { "DoubleLength", "DoubleLength" }, "" );
        addOption( { "Time Signature", "TimeSignature" }, "" );
        //addOption( { "Bounce", "Bounce" }, "" );

        addOption( { "InsertBars", "InsertBar" }, "" );
        addOption( { "CopyEvent", "CopyEvent" }, "" );
        addOption( { "CopyBar", "CopyBar" }, "" );
        addOption( { "CopySequence", "CopySequence" }, "" );

        addOption( { "Clear", "Clear" }, "" );

        setVisible( true );
        setSelectionEnabled( false );
    }

    AppSequenceOptionsMenu::~AppSequenceOptionsMenu()
    {
        setVisible( false );
    }

}
