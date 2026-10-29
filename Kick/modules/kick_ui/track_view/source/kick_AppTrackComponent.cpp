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

//==============================================================================

AppTrackDisplay::AppTrackDisplay( int Seq, int Track )
{
    mt::utils::addAndMakeVisible( this, name, selector, length, transpose, mute, solo, arm, monitor, level, inputmidi, levelMeter, midiPort );

    mt::utils::setValueFontHeight( 16, level, transpose, name );
    mt::utils::setTitleFontHeight( 16, level, transpose, length, inputmidi, midiPort );
    mt::utils::setValueFontHeight( 24, inputmidi, midiPort );

    length.setTitleProportion( .36f );

    name.setTitleJustification( juce::Justification::centredLeft );
    name.setValueJustification( juce::Justification::topLeft );
    name.setTitleFontHeight( 24 );

    length.setTitleJustification( juce::Justification::centredTop );
    inputmidi.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    midiPort.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    midiPort.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    length.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    inputmidi.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    transpose.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    length.setAlternateText( true, "Seq" );

    level.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
    level.setNumDecimalPlacesToDisplay( 0 );
    level.setTitleJustification( juce::Justification::centredTop );
    transpose.setTitleJustification( juce::Justification::centredTop );

    transpose.setRange( TRANSPOSE::MIN, TRANSPOSE::MAX, TRANSPOSE::STEP );
    transpose.setCentered( true );
    transpose.setNumDecimalPlacesToDisplay( 0 );

    arm.setComponentID("main-track-arm"); monitor.setComponentID("main-track-monitor");
    arm.setClickingTogglesState(true); monitor.setClickingTogglesState(true);
    arm.onClick = [this] {
        auto* data = ae.getTrackData(seq, track); if(!data || !ae.isProjectLoaded()) return;
        ae.getUndoManager().beginNewTransaction("Arm track");
        if(auto* audio = dynamic_cast<mt::MTAudioProgram*>(ae.getProgramForTrack(seq, track))) audio->armed = arm.getToggleState();
        else data->armed = arm.getToggleState();
    };
    monitor.onClick = [this] {
        auto* data = ae.getTrackData(seq, track); if(!data || !ae.isProjectLoaded()) return;
        ae.getUndoManager().beginNewTransaction("Monitor track");
        if(auto* audio = dynamic_cast<mt::MTAudioProgram*>(ae.getProgramForTrack(seq, track))) audio->monitor = monitor.getToggleState();
        else data->monitor = monitor.getToggleState();
    };

    name.onTextChange = [&]() {
        ae.renameTrack( seq, track, name.getText() );
    };

    selector.onSelectionChanged = [&]( const juce::String& Program, const mt::MTProgramType Type ) {
        ae.setProgramToTrack( Program, ae.getSelectedSequence(), ae.getSelectedTrack() );
    };

    length.onValueChanged = [&]( int Value ) {
        if(!pTrack) return;
        if( Value > 0 && pTrack )
            pTrack->isSeqLength = false;
        else if( Value == 0 && pTrack )
            pTrack->isSeqLength = true;
        auto seqLength = ae.getSequenceLength( seq );
        if( Value > seqLength )
            length.setValue( seqLength );
        else
            ae.setTrackLength( seq, track, Value );
    };
    mute.onClick = [&]() {
        if( !pTrack ) return;

        if( soloData )
            mute.setToggleState( false, juce::dontSendNotification );
        else if( muteDueToSoloData )
            mute.setToggleState( true, juce::dontSendNotification );
        else
        {
            muteData     = mute.getToggleState();
            pTrack->mute = muteData;
        }
    };

    solo.onClick = [&]() {
        soloData = solo.getToggleState();
        ae.setTrackSolo( seq, track, soloData );
        onSoloMuteChange();
    };

    midiPort.onValueChanged = [&]( const juce::String& Port ) {
        if( pTrack )
            pTrack->midiPort = Port;
    };

    inputmidi.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        if( pTrack )
            pTrack->midiChannel = Index;
    };

    level.onValueChanged = [&]( double Value ) {
        if( pTrack )
            pTrack->level = Value;
    };

    transpose.onValueChanged = [&]( double Value ) {
        if( pTrack )
            pTrack->transpose = Value;
    };

    ui::connectTheme( *this, [&]() {
        updateThemeImages( true );
    } );

    setTrack( Seq, Track );
    setInterceptsMouseClicks( true, true );
}

AppTrackDisplay::~AppTrackDisplay()
{
    cancelPendingUpdate(); recordingScope.reset(); scope.reset();
}

void AppTrackDisplay::updateThemeImages( bool loadOrUnload )
{
    if( loadOrUnload )
    {
        mute.setImages( "Mute_normal", "Mute_active" );
        solo.setImages( "Solo_normal", "Solo_active" );
        monitor.setImages( "Monitor_normal", "Monitor_active" );
    }
    else
    {
    }
}

void AppTrackDisplay::setTrack( int Seq, int Track )
{
    recordingScope.reset(); scope.reset(); trkMeter = nullptr;
    track  = Track;
    seq    = Seq;
    pTrack = ae.getTrackData( seq, track );
    setEnabled(pTrack != nullptr);

    auto muteValueHandler        = [&]() { muteData = pTrack->mute; };
    auto soloValueHandler        = [&]() { soloData = pTrack->solo; };
    auto levelValueHandler       = [&]() { level.setValue( pTrack ? (double)pTrack->level.get() : 0.0, juce::dontSendNotification ); };
    auto nameValueHandler        = [&]() { name.setText( pTrack ? pTrack->name.get() : "", juce::dontSendNotification ); };
    auto lengthValueHandler      = [&]() { length.setValue( pTrack ? (double)pTrack->length.get() : BAR::MIN, juce::dontSendNotification ); };
    auto midiChannelValueHandler = [&]() { inputmidi.setSelected( pTrack ? (int)pTrack->midiChannel.get() : 0, juce::dontSendNotification ); };
    auto transposeValueHandler   = [&]() { transpose.setValue( pTrack ? (double)pTrack->transpose.get() : TRANSPOSE::DEFAULT, juce::dontSendNotification ); };
    auto programValueHandler     = [&]() {
        auto pProg = ae.getProgramDataForTrack( seq, track );
        name.setLine2Text( pProg ? pProg->name.get() : "" );
        // Setup selector
        selector.setValues( pProg ? pProg->name.get() : "", pProg ? pProg->type.get() : mt::MTProgramType::Unknown, juce::dontSendNotification );
        refreshRecordingControls();
    };
    auto midiPortValueHandler = [&]() {
        selMidiPort = pTrack ? pTrack->midiPort.get() : "";
        midiPort.setSelected( selMidiPort, juce::dontSendNotification );
    };

    scope.reset();
    if( pTrack )
    {
        scope = std::make_shared<mt::Kick::ScopeTracker>();

        trkMeter = &( ae.getTrackMeter( seq, track ) );

        pTrack->name.onValueChange.connect( ( nameValueHandler ), scope );
        pTrack->programId.onValueChange.connect( ( programValueHandler ), scope );
        pTrack->mute.onValueChange.connect( ( muteValueHandler ), scope );
        pTrack->solo.onValueChange.connect( ( soloValueHandler ), scope );
        pTrack->level.onValueChange.connect( ( levelValueHandler ), scope );
        pTrack->midiChannel.onValueChange.connect( ( midiChannelValueHandler ), scope );
        pTrack->midiPort.onValueChange.connect( ( midiPortValueHandler ), scope );
        pTrack->length.onValueChange.connect( ( lengthValueHandler ), scope );
        pTrack->transpose.onValueChange.connect( ( transposeValueHandler ), scope );

        // Force refresh of values.
        pTrack->name.onValueChange();
        pTrack->programId.onValueChange();
        pTrack->mute.onValueChange();
        pTrack->solo.onValueChange();
        pTrack->level.onValueChange();
        pTrack->midiPort.onValueChange();
        pTrack->midiChannel.onValueChange();
        pTrack->length.onValueChange();
        pTrack->transpose.onValueChange();
    }

    if( onTrackChange ) onTrackChange( track );
}

void AppTrackDisplay::refreshRecordingControls()
{
    recordingScope.reset();
    auto update = [this] {
        auto* data = ae.getTrackData(seq, track);
        auto* audio = dynamic_cast<mt::MTAudioProgram*>(ae.getProgramForTrack(seq, track));
        arm.setToggleState(audio ? audio->armed.get() : data && data->armed.get(), juce::dontSendNotification);
        monitor.setToggleState(audio ? audio->monitor.get() : data && data->monitor.get(), juce::dontSendNotification);
    };
    if(auto* data = ae.getTrackData(seq, track))
    {
        recordingScope = std::make_shared<mt::Kick::ScopeTracker>();
        data->armed.onValueChange.connect(update, recordingScope); data->monitor.onValueChange.connect(update, recordingScope);
        if(auto* audio = dynamic_cast<mt::MTAudioProgram*>(ae.getProgramForTrack(seq, track)))
        {
            audio->armed.onValueChange.connect(update, recordingScope); audio->monitor.onValueChange.connect(update, recordingScope);
        }
    }
    update();
}

void AppTrackDisplay::handleAsyncUpdate()
{
}

void AppTrackDisplay::updateMeter()
{
    if(!ae.isProjectLoaded() || ae.getTrackData(seq, track) != pTrack || !pTrack) return;
    trkMeter = &ae.getTrackMeter(seq, track);
    if( trkMeter && trkMeter->size() >= 2 )
    {
        levelMeter.setSignalStrength( trkMeter->at( 0 ).rms, trkMeter->at( 1 ).rms );
        float rmsValue = trkMeter->at( 0 ).rms;

        // Map rmsValue from [-100, 17] to [0, 1]
        float minOriginal  = -100.0; //Slider Min value
        float maxOriginal  = 17.0; //Slider Max Value
        float shiftedValue = ( rmsValue - minOriginal ) / ( maxOriginal - minOriginal );

        if( shiftedValue > 0 )
        {
            sendPercentValueToHardware( shiftedValue );
        }
    }
}

void AppTrackDisplay::sendPercentValueToHardware( float Value )
{
    auto valueInPercent = (int)( 100.0 * Value );
    contracts::DclSignals::sendCommandToDcl(
        contracts::DclContractsHelper::sendSliderValueInPercentMessage( valueInPercent ) );
}

void AppTrackDisplay::setMidiPorts( const std::vector<juce::String>* MidiPorts )
{
    if( pTrack )
    {
        selMidiPort = pTrack->midiPort.get();
        auto ports  = *MidiPorts;
        if( ( ( std::find( ports.begin(), ports.end(), selMidiPort ) ) != ports.end() ) && ports.size() >= 1 )
        {
            auto it            = std::find( ports.begin(), ports.end(), selMidiPort );
            auto selectedIndex = 0;
            if( it != ports.end() )
                selectedIndex = std::distance( ports.begin(), it );
            midiPort.setListOptions( ports, selectedIndex );
        }
        else if( selMidiPort.isEmpty() && ports.size() )
            midiPort.setListOptions( ports );
    }
}

void AppTrackDisplay::linkRotated( int ID, int Value )
{
    DBG( "AppTrackDisplay linkRotated with ID ---> " << ID << "Value -->  " << Value );
    switch( ID )
    {
        case 0: inputmidi.knobRotated( Value ); break;
        case 1: length.knobRotated( Value ); break;
        case 2: level.knobRotated( Value ); break;
        case 3: transpose.knobRotated( Value ); break;
    }
}

void AppTrackDisplay::resized()
{
    selector.setBoundsRelative( 0.001f, .509f, .042f, .464f );
    name.setBoundsRelative( .065f, .3035f, .16f, .3932f );
    midiPort.setBoundsRelative( .245f, .0892f, .1f, .6071f );
    inputmidi.setBoundsRelative( .364f, .0892f, .1f, .6071f );
    length.setBoundsRelative( .483f, .0892f, .1f, .6071f );
    level.setBoundsRelative( .602f, .0892f, .062f, .8214f );
    transpose.setBoundsRelative( .681f, .0892f, .062f, .8214f );
    arm.setBoundsRelative( .761f, .0892f, .06f, .3932f );
    solo.setBoundsRelative( .826f, .0892f, .06f, .3932f );
    monitor.setBoundsRelative( .762f, .5178f, .06f, .3932f );
    mute.setBoundsRelative( .826f, .5178f, .06f, .3932f );
    levelMeter.setBoundsRelative( .906f, .0892f, .03f, .8214f );
}

void AppTrackDisplay::refreshSoloMute()
{
    if(!ae.isProjectLoaded()) { setTrack(-1, -1); return; }
    if(ae.getTrackData(seq, track) != pTrack) setTrack(seq, track);
    if(!pTrack) return;
    auto isSoloed = ae.getNumSoloTracks( seq ) >= 1 ? true : false;
    mute.setToggleState( soloData ? false : isSoloed ? true :
                                                       muteData,
                         false );
    solo.setToggleState( soloData, juce::dontSendNotification );
    muteDueToSoloData = soloData ? false : isSoloed ? true :
                                                      false;

    pTrack->muteDueToSolo = muteDueToSoloData;
}

void AppTrackDisplay::mouseDown( const juce::MouseEvent& e )
{
    ui::clearFocusComponent();
}

void AppTrackDisplay::visibilityChanged()
{
    bool visible = isVisible();
    updateThemeImages( visible );

    for( auto it : getChildren() )
        it->setVisible( visible );

    if( visible )
        setTrack( seq, track );
    else
        contracts::DclSignals::sendCommandToDcl(
            contracts::DclContractsHelper::sendSliderValueInPercentMessage( 0 ) );
}

//==============================================================================
//  AppTrackComponent
//==============================================================================
AppTrackComponent::AppTrackComponent( int Seq, int Track, juce::String Name )
{
    trackNo = Track;
    setName( Name );
    addMouseListener( this, true );

    mt::utils::addChildComponents( this, srNo, track, options );
    mt::utils::disableMouseInterceptsAndFocus( srNo );
    setInterceptsMouseClicks( true, true );

    srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 6 ) );
    options.setBackgroundColour( ui::Theme::backgroundColour( 1 ) );

    options.onClick = [&]() {
        mt::getAudioInterface().setSelectedSequenceAndTrack( track.getSeqNo(), track.getTrackNo() );
        if( onTrackEdit )
            onTrackEdit();
    };

    track.onTrackChange = [&]( int No ) {
        srNo.setText( juce::String( No + 1 ) );
    };

    track.onSoloMuteChange = [&]() {
        if( onSoloMuteChange )
            onSoloMuteChange();
    };
    setTrack( Seq, Track );

    ui::connectTheme( *this, [&]() {
        updateThemeImages( true );
    } );
}

AppTrackComponent::~AppTrackComponent()
{
    removeMouseListener( this );
}

void AppTrackComponent::updateThemeImages( bool loadOrUnload )
{
    if( loadOrUnload )
    {
        options.setImages( "editOptions" );
    }
    else
    {
    }
}

void AppTrackComponent::setTrack( int Seq, int Track )
{
    auto& ae = mt::getAudioInterface();
    if( 0 <= Seq && 0 <= Track && Track < ae.getNumTracks( Seq ) )
    {
        track.setTrack( Seq, Track );
        mt::utils::makeVisible( srNo, track, options );
    }
    else
    {
        mt::utils::makeHidden( srNo, track, options );
    }
    resized();
}

void AppTrackComponent::resized()
{
    srNo.setBoundsRelative( .001f, .01f, .044f, .98f );
    track.setBoundsRelative( 0.0f, 0.f, 1.f, 1.f );
    options.setBoundsRelative( .957f, .0136f, .04f, .965f );
}

void AppTrackComponent::mouseUp( const juce::MouseEvent& e )
{
    if( e.mouseWasClicked() )
    {
        mt::getAudioInterface().setSelectedTrack( track.getTrackNo() );
    }
}

void AppTrackComponent::setMidiPorts( const std::vector<juce::String>* MidiPorts )
{
    track.setMidiPorts( MidiPorts );
}

void AppTrackComponent::visibilityChanged()
{
    auto visible = isVisible();
    updateThemeImages( visible );
    for( auto& it : getChildren() )
        it->setVisible( visible );
}

/// <summary>
///
/// </summary>

AppTrackHeader::AppTrackHeader()
{
    setInterceptsMouseClicks( false, false );
    setWantsKeyboardFocus( false );

    mt::utils::addAndMakeVisible( this, srNo, track, prog, midiIn, len, trans, vel, opts );
}

void AppTrackHeader::resized()
{
    auto b       = getLocalBounds();
    auto headerW = b.getWidth();
    srNo.setBounds( b.removeFromLeft( (int)( 0.05 * headerW ) ) );
    track.setBounds( b.removeFromLeft( (int)( 0.20 * headerW ) ) );
    prog.setBounds( b.removeFromLeft( (int)( 0.10 * headerW ) ) );
    midiIn.setBounds( b.removeFromLeft( (int)( 0.20 * headerW ) ) );
    len.setBounds( b.removeFromLeft( (int)( 0.10 * headerW ) ) );
    trans.setBounds( b.removeFromLeft( (int)( 0.10 * headerW ) ) );
    vel.setBounds( b.removeFromLeft( (int)( 0.10 * headerW ) ) );
    opts.setBounds( b.removeFromLeft( (int)( 0.10 * headerW ) ) );
}
