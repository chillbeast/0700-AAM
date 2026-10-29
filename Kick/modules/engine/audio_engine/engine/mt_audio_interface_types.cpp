/*
 * Copyright (c) 2024 MUSIC Tribe
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

namespace mt
{
    ///
    /// Cached Value interfaces...
    ///

    ///
    /// MTSimultaneousAction
    ///
    MTSimultaneousAction::MTSimultaneousAction( juce::ValueTree VT, juce::UndoManager* UM )
    {
        for( int i = 0; i < MAX_SIMUL_ACTIONS; ++i )
        {
            juce::Identifier acPropertyId( "ac" + juce::String( i + 1 ) );
            ac[i].referTo( VT, acPropertyId, UM, -1 );
        }
    }

    void MTSimultaneousAction::setActions( const std::vector<int>& Simul )
    {
        for( auto i = 0; i < Simul.size(); ++i )
            ac[i] = juce::jlimit( -1, 127, Simul[i] );
    }

    std::array<int, MAX_SIMUL_ACTIONS> MTSimultaneousAction::getActions()
    {
        std::array<int, MAX_SIMUL_ACTIONS> actions;
        for( int i = 0; i < MAX_SIMUL_ACTIONS; ++i )
            actions[i] = ac[i].get();
        return actions;
    }

    ///
    /// MTSoundRandomizationData
    ///
    MTSoundRandomizationData::MTSoundRandomizationData( juce::ValueTree& VT, juce::UndoManager* UM )
    {
        rPitch.referTo( VT, IDs::pitch, UM, MIDI::DEFAULT );
        rOffset.referTo( VT, IDs::offset, UM, MIDI::DEFAULT );
        rLevel.referTo( VT, IDs::level, UM, MIDI::DEFAULT );
        rPan.referTo( VT, IDs::pan, UM, MIDI::DEFAULT );
    }

    ///
    /// MTISource
    ///
    MTISource::MTISource( juce::ValueTree& VT, juce::UndoManager* UM )
    {
        auto sliceSource = [&]() {
            auto& assetMgr = getAudioInterface().getAssetManager();
            src            = assetMgr.getAssetSlice( source.get(), slice.get() ); // Source
        };

        source.onValueChange.connect( sliceSource );
        slice.onValueChange.connect( sliceSource );

        source.referTo( VT, IDs::source, UM, "" );
        slice.referTo( VT, IDs::slice, UM, 0 );

        // Trigger source changed.
        source.onValueChange;
    }

    ///
    /// MTILayer
    ///
    MTILayer::MTILayer( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTISource( VT, UM )
        , MTSoundRandomizationData( VT, UM )
    {
        layerTree = VT;

        velStart.referTo( VT, IDs::velocityStart, UM, MIDI::MIN );
        velEnd.referTo( VT, IDs::velocityEnd, UM, MIDI::MAX );
    }

    void MTILayer::setSource( const juce::String& Source, int Slice )
    {
        source = Source;
        slice  = Slice;
    }

    void MTILayer::clearSource() { setSource( "", 0 ); }

    ///
    /// MTISample
    ///
    MTISample::MTISample( juce::ValueTree& VT, juce::UndoManager* UM )
        : vTree( VT )
        , um( UM )
        , sPlay( VT.getOrCreateChildWithName( IDs::SPLAY, UM ), UM )
        , sMute( VT.getOrCreateChildWithName( IDs::SMUTE, UM ), UM )
        , cents( 0 )
        , gL( 1.0 )
        , gR( 1.0 )
    {
        assert( VT.hasType( IDs::CLIP ) );

        auto centsCalculator = [&]() { cents.store( 100 * semi.get() + fine.get() ); };
        auto gainCalculator  = [&]() {
            AudIO::PannerFloat panner( pan.get(), gain.get() );
            gL.store( panner.getLeft() );
            gR.store( panner.getRight() );
        };

        semi.onValueChange.connect( centsCalculator );
        fine.onValueChange.connect( centsCalculator );
        gain.onValueChange.connect(gainCalculator);
        pan.onValueChange.connect(gainCalculator);

        solo.referTo( vTree, IDs::solo, UM, false );
        mute.referTo( vTree, IDs::mute, UM, false );
        keyNote.referTo( vTree, IDs::keyNote, UM, MIDI::MIDDLE_C );
        minNote.referTo( vTree, IDs::minNote, UM, MIDI::MIN );
        maxNote.referTo( vTree, IDs::maxNote, UM, MIDI::MAX );
        muteGroup.referTo( vTree, IDs::muteGroup, UM, MUTEGROUP::DEFAULT );
        polyPhony.referTo( vTree, IDs::poly, UM, POLYPHONY::DEFAULT );

        semi.referTo( vTree, IDs::semi, UM, SEMI::DEFAULT );
        fine.referTo( vTree, IDs::fine, UM, FINE::DEFAULT );

        gain.referTo( vTree, IDs::gain, UM, GAIN::DEFAULT );
        pan.referTo( vTree, IDs::pan, UM, PAN::DEFAULT );

        // Force update
        fine.onValueChange();
        pan.onValueChange();

        // Ensure there are sufficient layers.
        auto layerVTree  = VT.getOrCreateChildWithName( IDs::LAYERS, nullptr );
        auto numChildren = layerVTree.getNumChildren();
        if( numChildren < MAX_LAYERS )
        {
            const auto childrenToAdd = MAX_LAYERS - numChildren;
            for( auto i = 0; i < childrenToAdd; ++i )
                layerVTree.appendChild( juce::ValueTree{ IDs::LAYER }, nullptr );
        }

        // Connect MTILayer
        for( auto i = 0; i < MAX_LAYERS; ++i )
        {
            auto childVT = layerVTree.getChild( i );
            layers[i]    = std::make_unique<MTILayer>( childVT, UM );
        }
    }

    void MTISample::removeSource( const juce::String& Source )
    {
        for( auto& it : layers )
        {
            if( 0 == Source.compare( it->source.get() ) )
            {
                it->source = "";
                it->slice  = 0;
            }
        }
    }

    juce::String MTISample::getSource( int Layer )
    {
        return ( juce::isPositiveAndBelow( Layer, MAX_LAYERS ) ) ? layers[Layer]->source.get() : EMPTY::String;
    }

    ASoundSlice* MTISample::getSourceSlice( int Layer )
    {
        return ( juce::isPositiveAndBelow( Layer, MAX_LAYERS ) ) ? layers[Layer]->src : nullptr;
    }

    std::array<juce::String, MAX_LAYERS> MTISample::getSources()
    {
        std::array<juce::String, MAX_LAYERS> sources;
        for( auto i = 0; i < MAX_LAYERS; ++i )
            sources[i] = layers[i]->source.get();

        return sources;
    }

    ///
    /// MTILFO
    ///
    MTILFO::MTILFO( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        assert( VT.hasType( IDs::LFO ) );
        lfoIsEnabled.referTo( VT, IDs::isEnabled, UM, false );
        lfoType.referTo( VT, IDs::type, UM, 0 );
        lfoSync.referTo( VT, IDs::sync, UM, 0 );
        lfoFrequency.referTo( VT, IDs::freq, UM, 1 );
        lfoFilter.referTo( VT, IDs::filter, UM, 0 );
        lfoPitch.referTo( VT, IDs::pitch, UM, 0 );
        lfoPan.referTo( VT, IDs::pan, UM, PAN::DEFAULT );
        lfoLevel.referTo( VT, IDs::level, UM, MIDI::DEFAULT );
    }

    ///
    /// MTIPitchEnvelope
    ///
    MTIPitchEnvelope::MTIPitchEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        assert( VT.hasType( IDs::PITCHENV ) );
        pitchtype.referTo( VT, IDs::TYPE, UM, ADSRType::Type_ADSR );
        pitchAttack.referTo( VT, IDs::attack, UM, 0 );
        pitchDecay.referTo( VT, IDs::decay, UM, 0 );
        pitchDepth.referTo( VT, IDs::depth, UM, 0 );
        pitchHold.referTo( VT, IDs::hold, UM, 0 );
        pitchSustain.referTo( VT, IDs::sustain, UM, 127 );
    }

    ///
    /// MTIFilterEnvelope
    ///
    MTIFilterEnvelope::MTIFilterEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        assert( VT.hasType( IDs::FILTERENV ) );

        filtertype.referTo( VT, IDs::TYPE, UM, ADSRType::Type_ADSR );
        filterAttack.referTo( VT, IDs::attack, UM, 0 );
        filterDecay.referTo( VT, IDs::decay, UM, 0 );
        filterDepth.referTo( VT, IDs::depth, UM, 0 );
        filterHold.referTo( VT, IDs::hold, UM, 0 );
        filterSustain.referTo( VT, IDs::sustain, UM, 127 );
    }

    ///
    /// MTIAmpEnvelope
    ///
    MTIAmpEnvelope::MTIAmpEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        assert( VT.hasType( IDs::AMPENV ) );

        amptype.referTo( VT, IDs::TYPE, UM, ADSRType::Type_ADSR );
        ampAttack.referTo( VT, IDs::attack, UM, 0 );
        ampDecay.referTo( VT, IDs::decay, UM, 0 );
        ampDepth.referTo( VT, IDs::depth, UM, 127 );
        ampHold.referTo( VT, IDs::hold, UM, 0 );
        ampSustain.referTo( VT, IDs::sustain, UM, 127 );
    }

    ///
    /// MTI
    ///

    ///
    /// MTI
    ///

    ///
    /// MTIClip
    ///
    MTIClip::MTIClip( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTISample( VT, UM )
    {
        padPlay.referTo( vTree, IDs::padPlay, UM, ClipPlayType::CLIP_ONE_SHOT );
    }

    ///
    /// MTIDrum
    ///
    MTICommonDrumKeys::MTICommonDrumKeys( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTISample( VT, UM )
        , MTILFO( VT.getOrCreateChildWithName( IDs::LFO, nullptr ), UM )
        , MTIPitchEnvelope( VT.getOrCreateChildWithName( IDs::PITCHENV, nullptr ), UM )
        , MTIFilterEnvelope( VT.getOrCreateChildWithName( IDs::FILTERENV, nullptr ), UM )
        , MTIAmpEnvelope( VT.getOrCreateChildWithName( IDs::AMPENV, nullptr ), UM )
    {
    }

    ///
    /// MTIDrum
    ///
    MTIDrum::MTIDrum( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTICommonDrumKeys( VT, UM )
    {
        samplePlay.referTo( VT, IDs::samplePlay, UM, SamplePlayType::ONE_SHOT );
        layerPlay.referTo( VT, IDs::layerPlay, UM, LayerPlayType::VELOCITY );
    }

    ///
    /// MTIDrum
    ///
    MTIKeys::MTIKeys( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTICommonDrumKeys( VT, UM )
    {
        keyTracking.referTo(VT, "keyTracking", UM, true);
        samplePlay.referTo( VT, IDs::samplePlay, UM, SamplePlayType::ONE_SHOT );
        layerPlay.referTo( VT, IDs::layerPlay, UM, LayerPlayType::VELOCITY );
    }

    MTIAudio::MTIAudio( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTISource( VT, UM ), vTree( VT )
    {
        startTime.referTo( VT, IDs::startTime, UM, 0 );
        level.referTo( VT, IDs::level, UM, 0 );
        bpm.referTo( VT, IDs::bpm, UM, 0 );
        start.referTo( VT, IDs::start, UM, 0 );
        end.referTo( VT, IDs::end, UM, 0 );
        mute.referTo( VT, IDs::mute, UM, false );
        reverse.referTo( VT, IDs::reverse, UM, false );
        warp.referTo( VT, IDs::warp, UM, false );
        length.referTo( VT, IDs::length, UM, 0 );
    }

    void MTIAudio::setSource( const juce::String& Source, int Slice, double Start, double Length )
    {
        source    = Source;
        slice     = Slice;
        startTime = Start;
        length    = Length;
    }

    void MTIAudio::clearSource() { setSource( "", 0, 0, 0 ); }

    void MTIAudio::setLoopPoints( double Start, double End )
    {
        start = Start;
        end   = End;
    }

    // MTILoopData::
    MTILoopData::MTILoopData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::SEQUENCE ) || V.hasType( IDs::PROGRAM ) || V.hasType( IDs::TRACK ) );
        juce::ValueTree state = V;
        isLooping.referTo( state, IDs::loop, UM, true );
        loopStart.referTo( state, IDs::loopStart, UM, 0 );
        loopEnd.referTo( state, IDs::loopEnd, UM, BAR::DEFAULT );
    }

    void MTILoopData::forceUpdateOfCachedValues()
    {
        isLooping.forceUpdateOfCachedValue();
        loopStart.forceUpdateOfCachedValue();
        loopEnd.forceUpdateOfCachedValue();
    }

    /// MTIPadColors
    MTIPadColors::MTIPadColors( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::COLOURS ) );
        juce::ValueTree state = V;

        auto getDefaultPadColour = []( const juce::ValueTree& V, int Pad ) -> int {
            auto        prog        = V.getParent();
            auto        type        = magic_enum::enum_cast<MTProgramType>( (int)prog.getProperty( IDs::type, 7 ) ).value();
            const auto& progColours = MTProgramType::Drum == type ? defaultDrumColours :
                                      MTProgramType::Clip == type ? defaultClipColours :
                                                                    defaultPadColours;

            return ( Pad < MIDI::NUMERIC_MAX ) ? progColours[Pad] : 0;
        };

        colourMode.referTo( state, IDs::mode, UM, PadColorMode::DIM_VELOCITY );
        emptyPadColourMode.referTo( state, IDs::emptyColourmode, UM, EmptyPadColorMode::Off );
        for( auto pad = MIDI::MIN; pad < MIDI::NUMERIC_MAX; ++pad )
        {
            juce::Identifier padId( juce::String( "clr" + juce::String( pad ) ) );
            padColour[pad].referTo( state, padId, UM, getDefaultPadColour( state, pad ) );
        }
    }

    // MTIPadPerformanceData
    MTIPadPerformanceData::MTIPadPerformanceData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::PERFORMANCE ) );

        juce::ValueTree performanceState = V;
        performMode.referTo( performanceState, IDs::mode, UM, PerformanceMode::CHROMATIC );
        performScale.referTo( performanceState, IDs::scale, UM, "Major" );
        performRootNote.referTo( performanceState, IDs::rootNote, UM, "C" );
        performOctave.referTo( performanceState, IDs::octave, UM, "-2" );
        performChords.referTo( performanceState, IDs::chord, UM, "1-3-5" );
        performBanks.referTo( performanceState, IDs::bank, UM, "Continuous" );
    }

    /// MTIProgramData
    MTIProgramData::MTIProgramData( juce::ValueTree& V, juce::UndoManager* UM )
        : MTIPadColors( V.getOrCreateChildWithName( IDs::COLOURS, nullptr ), UM )
        , MTIPadPerformanceData( V.getOrCreateChildWithName( IDs::PERFORMANCE, nullptr ), UM )
    {
        jassert( V.hasType( IDs::PROGRAM ) );
        juce::ValueTree state = V;
        programState          = V;

        type.referTo( state, IDs::type, UM, MTProgramType::Unknown );
        id.referTo( state, IDs::id, UM, "" );
        name.referTo( state, IDs::name, UM, "" );
        output.referTo( state, IDs::OUT, UM, "Out-1,2" );
        mute.referTo( state, IDs::mute, UM, false );
        solo.referTo( state, IDs::solo, UM, false );
        level.referTo( state, IDs::level, UM, MIDI::LEVEL_DEFAULT );
        semi.referTo( state, IDs::semi, UM, SEMI::DEFAULT );
        fine.referTo( state, IDs::fine, UM, FINE::DEFAULT );
        transpose.referTo( state, IDs::transpose, UM, TRANSPOSE::DEFAULT );
        polyphony.referTo( state, IDs::poly, UM, POLYPHONY::DEFAULT );
        pan.referTo( state, IDs::pan, UM, PAN::DEFAULT );

        auto sendState = state.getOrCreateChildWithName( IDs::SEND, nullptr );
        send1.referTo( sendState, IDs::sg0, UM, GAIN::MIN );
        send2.referTo( sendState, IDs::sg1, UM, GAIN::MIN );
        send3.referTo( sendState, IDs::sg2, UM, GAIN::MIN );
        send4.referTo( sendState, IDs::sg3, UM, GAIN::MIN );
    }

    // MTIClipProgramData::
    MTIClipProgramData::MTIClipProgramData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::PROGRAM ) );
        juce::ValueTree state = V;
        launchQuantize.referTo( state, IDs::quantize, UM, QUANTIZE::DEFAULT );
    }

    // MTICvProgramData::
    MTICvProgramData::MTICvProgramData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::PROGRAM ) );
        juce::ValueTree state = V;

        cvNote.referTo( state, IDs::CVNote, UM, "Last" );
        cvPort.referTo( state, IDs::CVPort, UM, 0 );
        cvGate.referTo( state, IDs::CVGate, UM, 0 );
        cvWheel.referTo( state, IDs::CVModWheel, UM, 0 );
        cvVelocity.referTo( state, IDs::CVVelocity, UM, 0 );
    }

    //MTIMidiProgramData::
    MTIMidiProgramData::MTIMidiProgramData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::PROGRAM ) );
        juce::ValueTree state = V;

        midiPort.referTo( state, IDs::MIDIPort, UM, "" );
        programCh.referTo( state, IDs::ProgCH, UM, 0 );
        bankMsb.referTo( state, IDs::BankMSB, UM, 0 );
        bankLsb.referTo( state, IDs::BankLSB, UM, 0 );
        midiCh.referTo( state, IDs::midiCh, UM, MIDI_CH::DEFAULT );
    }
    //MTAudioProgramData
    MTIAudioProgramData::MTIAudioProgramData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::PROGRAM ) );
        juce::ValueTree state = V;
        input.referTo(state, "audioInput", UM, "None");
        armed.referTo(state, "audioArmed", UM, false);
        monitor.referTo(state, "audioMonitor", UM, false);
    }

    // MTISequenceData::
    MTISequenceData::MTISequenceData( const juce::ValueTree& V, juce::UndoManager* UM )
        : MTILoopData( V, UM )
    {
        jassert( V.hasType( IDs::SEQUENCE ) );
        juce::ValueTree state = V;

        isMasterBPM.referTo( state, IDs::isMasterBPM, UM, true );
        id.referTo( state, IDs::id, nullptr );
        name.referTo( state, IDs::name, UM, "" );
        length.referTo( state, IDs::length, UM, 4 );
        transpose.referTo( state, IDs::transpose, UM, TRANSPOSE::DEFAULT );

        tempo.setDefault( BPM::DEFAULT );
    }

    void MTISequenceData::forceUpdateOfCachedValues()
    {
        isMasterBPM.forceUpdateOfCachedValue();
        id.forceUpdateOfCachedValue();
        name.forceUpdateOfCachedValue();
        length.forceUpdateOfCachedValue();
        transpose.forceUpdateOfCachedValue();

        MTILoopData::forceUpdateOfCachedValues();
    }

    // MTITrackData::
    MTITrackData::MTITrackData( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        programId.onValueChange.connect( [&]() {
            prog = getAudioInterface().getProgramDataForProgramID( programId.get() );
        } );

        jassert( V.hasType( IDs::TRACK ) );
        juce::ValueTree state = V;

        name.referTo( state, IDs::name, UM, "" );
        id.referTo( state, IDs::id, UM, "" );
        programId.referTo( state, IDs::program, UM, "" );
        armed.referTo(state, "recordArmed", UM, true);
        monitor.referTo(state, "inputMonitor", UM, true);
        isSeqLength.referTo( state, IDs::usingSequenceLength, UM, false ); // By default use track length.
        mute.referTo( state, IDs::mute, UM, false );
        solo.referTo( state, IDs::solo, UM, false );
        pan.referTo( state, IDs::pan, UM, PAN::DEFAULT );
        transpose.referTo( state, IDs::transpose, UM, TRANSPOSE::DEFAULT );
        muteGroup.referTo( state, IDs::muteGroup, UM, MUTEGROUP::DEFAULT );
        length.referTo( state, IDs::length, UM, MIDI::MIN );
        level.referTo( state, IDs::level, UM, MIDI::LEVEL_DEFAULT );
        midiPort.referTo( state, IDs::MIDIPort, UM, "" );
        midiChannel.referTo( state, IDs::midiCh, UM, MIDI_CH::DEFAULT );
        midiOutChannel.referTo( state, IDs::midiChOut, UM, MIDI_CH::DEFAULT );
        route.referTo( state, IDs::route, UM, "" );
    }

    // MTNoteRO::
    MTNoteRO::MTNoteRO( const juce::ValueTree& V )
        : vt( V ) {}

    bool MTNoteRO::isValid() const { return ( vt.hasType( IDs::NOTE ) && 0 <= number() && 128 > number() ); }
    bool MTNoteRO::mute() const { return (bool)vt.getProperty( IDs::mute, false ); }
    int  MTNoteRO::start() const { return (int)vt.getProperty( IDs::start, 0 ); }
    int  MTNoteRO::length() const { return (int)vt.getProperty( IDs::length, 16 ); }
    int  MTNoteRO::number() const { return (int)vt.getProperty( IDs::no, -1 ); }
    int  MTNoteRO::velocity() const { return (int)vt.getProperty( IDs::velocity, MIDI::MAX ); }
    int  MTNoteRO::probability() const { return (int)vt.getProperty( IDs::probability, PROBABILITY::DEFAULT ); }
    int  MTNoteRO::ratchet() const { return (int)vt.getProperty( IDs::ratchet, RATCHET::DEFAULT ); }

    // MTNote::
    bool MTNote::isValid() const { return ( 0 <= id ); }
    bool MTNote::operator<( const MTNote& rhs ) const { return start.get() < rhs.start.get(); }
    bool MTNote::operator()( MTNote const& lhs, MTNote const& rhs ) const { return lhs.start.get() < rhs.start.get(); }
    bool MTNote::operator==( const MTNote& rhs ) const { return ( number.get() == rhs.number.get() && start.get() == rhs.start.get() && length.get() == rhs.length.get() ); }

    MTNote::MTNote( const juce::ValueTree& V, juce::UndoManager* UM ) { referTo( V, UM ); }
    MTNote::MTNote( const MTNote& rhs ) { referTo( rhs.state, rhs.um ); }
    juce::ValueTree& MTNote::getState() { return state; }
    void             MTNote::referTo( const juce::ValueTree& V, juce::UndoManager* UM )
    {
        jassert( V.hasType( IDs::NOTE ) );

        state = V;
        um    = UM;

        id.referTo( state, IDs::id, um, -1 );
        mute.referTo( state, IDs::mute, um, false );
        number.referTo( state, IDs::no, um, MIDI::NOTE_DEFAULT );
        velocity.referTo( state, IDs::velocity, um, MIDI::LEVEL_DEFAULT );
        // Position
        start.referTo( state, IDs::start, um, 0 );
        length.referTo( state, IDs::length, um, 16 );
        probability.referTo( state, IDs::probability, um, PROBABILITY::DEFAULT );
        ratchet.referTo( state, IDs::ratchet, um, RATCHET::DEFAULT );
    }
}
