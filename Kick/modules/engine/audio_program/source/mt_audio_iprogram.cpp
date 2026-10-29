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

namespace mt
{
    // Static variables..
    static std::atomic_int __progCounter = 0;
    std::atomic<int> MTIProgram::soloCount{0};
    std::atomic<uint64_t> MTIProgram::soloEpoch{1};

    void MTIProgram::resetSoloingPrograms()
    {
        soloEpoch.fetch_add(1); soloCount.store(0); programSoloChanged();
    }

    void MTIProgram::soloChangeHandler(bool solo)
    {
        const auto epoch = soloEpoch.load();
        if(soloEpochSeen.exchange(epoch) != epoch) soloMember.store(false);
        if(soloMember.exchange(solo) != solo) soloCount.fetch_add(solo ? 1 : -1);
        programSoloChanged();
    }

    ///
    /// MTI Program...
    ///
    MTIProgram::MTIProgram( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgramData( VT, UM )
        , vTree( VT )
        , um( UM )
    {
        ++__progCounter;

        vTree.addListener( this );

        if( vTree.getProperty( IDs::id ).toString().isEmpty() )
            vTree.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        if( vTree.getProperty( IDs::name ).toString().isEmpty() )
            vTree.setProperty( IDs::name, "Program " + juce::String( __progCounter ).paddedLeft( '0', 2 ), um );

        soloConnection = programSoloChanged.connect([this] { soloStateChanged(isMutedDueToSolo()); });
        soloChangeHandler( MTIProgramData::solo.get() );

        iSampleVTree = vTree.getOrCreateChildWithName( IDs::CLIPS, nullptr );
    }

    MTIProgram::~MTIProgram()
    {
        soloConnection.disconnect(); soloChangeHandler(false);
        vTree.removeListener( this );
    }

    void MTIProgram::valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& Property )
    {
        auto send = vTree.getChildWithName( IDs::SEND );
        if( send == treeWhosePropertyHasChanged )
        {
            if( Property == IDs::sg0 ) onSendGainChanged( 0, send1.get() );
            else if( Property == IDs::sg1 ) onSendGainChanged( 1, send2.get() );
            else if( Property == IDs::sg2 ) onSendGainChanged( 2, send3.get() );
            else if( Property == IDs::sg3 ) onSendGainChanged( 3, send4.get() );
        }
        else if( vTree == treeWhosePropertyHasChanged )
        {
            if( Property == IDs::name ) onNameChanged();
            else if( Property == IDs::gain ) onGainChanged();
            else if( Property == IDs::pan ) onPanChanged();
            else if( Property == IDs::mute ) onMuteChanged();
            else if( Property == IDs::OUT ) onOutChanged();
            else if( Property == IDs::solo ) soloChangeHandler( MTIProgramData::solo.get() );
        }
    }

    void MTIProgram::rebuildSampleMap()
    {
        // Rebuild cache.
        iSampleMap.clear();

        for( const auto& it : iSampleVTree )
        {
            auto& vt = (juce::ValueTree&)it;
            if( auto clip = iSampleCreator( vt, um ) )
                iSampleMap[clip->keyNote.get()] = std::move( clip );
        }
    }

    MTISample* MTIProgram::createSample( int Note )
    {
        if( auto clip = iSampleCreator( Note ) )
        {
            // Add to project value tree.
            iSampleVTree.addChild( clip->vTree, -1, getUndoManager() );
            // Add to clip Map for quick reference.
            iSampleMap[Note] = std::move( clip );
        }
        // return if note is created.
        return getSample( Note );
    }

    void MTIProgram::removeSample( int Note )
    {
        if( auto p = getSample( Note ) )
        {
            iSampleVTree.removeChild( p->vTree, nullptr );
            iSampleMap.erase( Note );
        }
    }

    MTISample* MTIProgram::getSample( int Note )
    {
        auto it = iSampleMap.find( Note );
        return ( it != iSampleMap.end() ) ? it->second.get() : nullptr;
    }

    MTISample* MTIProgram::getOrCreate( int Note )
    {
        auto p = getSample( Note );
        return p ? p : createSample( Note );
    }

    MTILayer* MTIProgram::getLayer( int Note, int Layer )
    {
        auto pLayer = getOrCreate( Note );
        return pLayer ? pLayer->layers[Layer].get() : nullptr;
    }

    ASoundSlice* MTIProgram::getLayerSourceSlice( int Note, int Layer )
    {
        auto s = getLayer( Note, Layer );
        return s ? s->src : nullptr;
    }

    void MTIProgram::forAllSamples( const std::function<void( MTISample* )>& func )
    {
        if( func )
            for( auto& it : iSampleMap )
                func( it.second.get() );
    }

    void MTIProgram::getSamples( std::vector<MTISample*>& Samples )
    {
        Samples.clear();
        for( auto& it : iSampleMap )
            Samples.push_back( it.second.get() );
    }

    int MTIProgram::setName( const juce::String& newName )
    {
        if( !newName.isEmpty() )
        {
            MTIProgramData::name = newName;
            onNameChanged();
            programNameChanged( this );
            return 0;
        }
        return -1;
    }

    void MTIProgram::setMute( bool Mute )
    {
        MTIProgramData::mute = Mute;
        onMuteChanged();
    }

    void MTIProgram::setSolo( bool Solo )
    {
        MTIProgramData::solo = Solo;
    }

    void MTIProgram::setLevel( int Level )
    {
        MTIProgramData::level = Level;
        onGainChanged();
    }

    void MTIProgram::setPan( double Pan )
    {
        MTIProgramData::pan = Pan;
        onPanChanged();
    }

    void MTIProgram::setSemi( int Semi ) { MTIProgramData::semi = Semi; }
    void MTIProgram::setFine( int Fine ) { MTIProgramData::fine = Fine; }
    void MTIProgram::setTranspose( int Transpose ) { MTIProgramData::transpose = Transpose; }
    void MTIProgram::setPolyphony( int Value ) { MTIProgramData::polyphony = Value; }
    void MTIProgram::setPitch( int Semi, int Fine )
    {
        setSemi( Semi );
        setFine( Fine );
    }

    float MTIProgram::getSendGain( int SendChannel )
    {
        switch( SendChannel )
        {
            case 0: return MTIProgramData::send1.get();
            case 1: return MTIProgramData::send2.get();
            case 2: return MTIProgramData::send3.get();
            case 3: return MTIProgramData::send4.get();
        }
        return 0.f;
    }

    std::vector<float> MTIProgram::getSendGains()
    {
        return { (float)MTIProgramData::send1.get(), (float)MTIProgramData::send2.get(),
                 (float)MTIProgramData::send3.get(), (float)MTIProgramData::send4.get() };
    }

    void MTIProgram::setOutput( const juce::String& outName )
    {
        MTIProgramData::output = outName;
        onOutChanged();
    }

    void MTIProgram::setSendGain( int SendChannel, const double Gain )
    {
        switch( SendChannel )
        {
            case 0: MTIProgramData::send1 = Gain; break;
            case 1: MTIProgramData::send2 = Gain; break;
            case 2: MTIProgramData::send3 = Gain; break;
            case 3: MTIProgramData::send4 = Gain; break;
        }
        onSendGainChanged( SendChannel, getSendGain( SendChannel ) );
    }

    void MTIProgram::setSendGains( const std::vector<double>& Gains )
    {
        int i = 0;
        for( auto gain : Gains )
            setSendGain( i++, gain );
    }

    int MTIProgram::setColorMode( PadColorMode Mode )
    {
        MTIProgramData::colourMode = Mode;
        return 0;
    }

    int MTIProgram::setEmptyColorMode( EmptyPadColorMode Mode )
    {
        MTIProgramData::emptyPadColourMode = Mode;
        return 0;
    }

    int MTIProgram::setPadColor( int Color, int Pad )
    {
        if( juce::isPositiveAndBelow( Pad, MIDI::NUMERIC_MAX ) )
            MTIProgramData::padColour[Pad] = Color;
        else if( Pad == -1 )
            for( auto i = MIDI::MIN; i < MIDI::NUMERIC_MAX; ++i )
                MTIProgramData::padColour[i] = Color;
        signals::padColourChanged( Pad, Color );
        return 0;
    }

    void MTIProgram::noteProcess( juce::Array<juce::MidiMessage>& Msgs )
    {
        if( isProgramEffectivelyMuted() )
            return;

        float midiLevel;
        for( auto& it : Msgs )
        {
            midiLevel = level.get();
            if( it.isNoteOnOrOff() )
            {
                auto noteNo   = it.getNoteNumber();
                auto playType = getNoteSamplePlay( noteNo );
                midiLevel     = it.getVelocity();

                if( it.isNoteOn() )
                {
                    releaseVelocities[(size_t)noteNo] = (int)midiLevel;
                    switch( playType )
                    {
                        case SamplePlayType::ONE_SHOT:
                        case SamplePlayType::NOTE_ON:
                            noteOn( noteNo, midiLevel );
                            //DBG( "Note On----" << noteNo );
                            break;
                        case SamplePlayType::NOTE_OFF:
                            noteOff( noteNo );
                            //DBG( "Note Off----" << noteNo );
                            break;
                    }
                }
                else if( it.isNoteOff() )
                {
                    switch( playType )
                    {
                        case SamplePlayType::NOTE_ON:
                            noteOff( noteNo );
                            //DBG( "Note Off----" << noteNo );
                        case SamplePlayType::ONE_SHOT: // Nothing to be done here.
                            break;
                        case SamplePlayType::NOTE_OFF:
                            if(releaseVelocities[(size_t)noteNo] > 0)
                                noteOn(noteNo, midiLevel > 0 ? (int)midiLevel : releaseVelocities[(size_t)noteNo]);
                            //DBG( "Note On----" << noteNo );
                            break;
                    }
                    releaseVelocities[(size_t)noteNo] = 0;
                }
            }
            else if( it.isController() )
            {
                int controllerNumber = it.getControllerNumber();
                int controllerValue  = it.getControllerValue();
                // Now you can handle the controller message
                switch( controllerNumber )
                {
                    case 1: // Modulation Wheel
                        modWheel( controllerValue );
                        break;
                    case 120: // All Sound Off
                    case 123: // All Notes Off
                        noteOffAll();
                        break;
                }
            }
            else if( it.isAftertouch() )
            {
            }
            else if( it.isProgramChange() )
            {
            }
            else if( it.isChannelPressure() )
            {
            }
            else if( it.isPitchWheel() )
            {
            }
            reloadMidiMeter( (float)( midiLevel * MIDI::SCALER ) );
        }
    }

    void MTIProgram::forSamplesInSameMuteGroup( int Note, int MG, const std::function<void( int Notes )>& func )
    {
        if( CommonUtils::isValidMidiNote( Note ) && 0 <= MG )
        {
            forAllSamples( [Note, MG, func]( MTISample* S ) {
                if( S && ( MG == S->muteGroup.get() && Note != S->keyNote.get() ) )
                    func( S->keyNote.get() );
            } );
        }
    }

    void MTIProgram::addSample( int KeyNote, const juce::String& Sample, int Slice, int Layer )
    {
        if( auto p = getLayer( KeyNote, Layer ) )
            p->setSource( Sample, Slice );
    }

    void MTIProgram::removeSample( int KeyNote, int Layer )
    {
        if( auto p = getLayer( KeyNote, Layer ) )
            p->clearSource();
    }

    void MTIProgram::removeSample( int KeyNote, const juce::String& Source )
    {
        if( auto p = getOrCreate( KeyNote ) )
            p->removeSource( Source );
    }

    std::vector<juce::String> MTIProgram::getSampleNames()
    {
        std::vector<juce::String> names;

        forAllSamples( [&names]( MTISample* Sample ) {
            if( Sample )
                for( const auto& it : Sample->layers )
                    names.push_back( it->source.get() );
        } );

        return names;
    };

    std::vector<int> MTIProgram::getAssignedNotes()
    {
        std::vector<int> notes;
        forAllSamples( [&notes]( MTISample* Sample ) -> void {
            if( Sample )
                notes.push_back( Sample->keyNote.get() );
        } );
        return notes;
    }

    std::array<int, 4> MTIProgram::getSimultaneousNotePlay( int Note ) { return getOrCreate( Note )->sPlay.getActions(); }
    std::array<int, 4> MTIProgram::getSimultaneousNoteMute( int Note ) { return getOrCreate( Note )->sMute.getActions(); }

    void MTIProgram::setSimultaneousNotePlay( int Note, const std::vector<int>& Simul )
    {
        getOrCreate( Note )->sPlay.setActions( Simul );
    }

    void MTIProgram::setSimultaneousNoteMute( int Note, const std::vector<int>& Simul )
    {
        getOrCreate( Note )->sMute.setActions( Simul );
    }

    ///////////////////////////////////////////////////////////////////////////////////////
    /// Note Level Parameters
    //////////////////////////////////////////////////////////////////////////////////////
    // Getters
    /////////////////////////////////////////////////////////////////////////////////////

    std::vector<int> MTIProgram::getNotesInSameMuteGroup( int Note )
    {
        std::vector<int> notes;
        int              mg = getNoteMuteGroup( Note );
        if( 0 <= mg )
        {
            forAllSamples( [&notes, mg, Note]( MTISample* Sample ) {
                if( Sample )
                    if( ( mg == Sample->muteGroup.get() ) && ( Note != Sample->keyNote.get() ) )
                        notes.push_back( Sample->keyNote.get() );
            } );
        }
        return notes;
    }

    void MTIProgram::setNoteMute( int Note, bool Mute )
    {
        if( auto p = getOrCreate( Note ) )
        {
            auto mg = p->muteGroup.get();
            if( mg >= 0 )
            {
                forAllSamples( [mg, Note, Mute]( MTISample* Sample ) -> void {
                    if( Sample )
                        if( ( mg == Sample->muteGroup.get() ) && Mute != Sample->mute.get() )
                            Sample->mute = Mute;
                } );
            }
            else
                p->mute = Mute;
        }
    }

    bool MTIProgram::toggleNoteMute( int Note )
    {
        bool bMute = isNoteMute( Note );
        setNoteMute( Note, !bMute );
        return mute.get();
    }

    void MTIProgram::setNoteMuteGroup( int Note, int Value )
    {
        getOrCreate( Note )->muteGroup = Value;
    }

    void MTIProgram::setNoteMin( int Note, int Value )
    {
        getOrCreate( Note )->minNote = Value;
    }

    void MTIProgram::setNoteKey( int Note, int Value )
    {
        getOrCreate( Note )->keyNote = Value;
    }

    void MTIProgram::setNoteMax( int Note, int Value )
    {
        getOrCreate( Note )->maxNote = Value;
    }

    void MTIProgram::setNoteSemi( int Note, int Value )
    {
        getOrCreate( Note )->semi = Value;
    }

    void MTIProgram::setNoteFine( int Note, int Value )
    {
        getOrCreate( Note )->fine = Value;
    }

    void MTIProgram::setNoteGain( int Note, double Value )
    {
        getOrCreate( Note )->gain = Value;
    }

    void MTIProgram::setNotePan( int Note, double Value )
    {
        getOrCreate( Note )->pan = Value;
    }

    void MTIProgram::setNotePitch( int Note, int Semi, int Fine )
    {
        setNoteSemi( Note, Semi );
        setNoteFine( Note, Fine );
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Getters
    /////////////////////////////////////////////////////////////////////////////////////

    bool MTIProgram::isNoteMute( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? p->mute : false;
    }

    int MTIProgram::getNoteMuteGroup( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->muteGroup.get() : MUTEGROUP::DEFAULT;
    }

    int MTIProgram::getNoteMin( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->minNote.get() : MIDI::MIN;
    }

    int MTIProgram::getNoteKey( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->keyNote.get() : MIDI::MIDDLE_C;
    }

    int MTIProgram::getNoteMax( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->maxNote.get() : MIDI::MAX;
    }

    int MTIProgram::getNoteSemi( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->semi.get() : SEMI::DEFAULT;
    }

    int MTIProgram::getNoteFine( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (int)p->fine.get() : FINE::DEFAULT;
    }

    double MTIProgram::getNoteGain( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (double)p->gain.get() : GAIN::DEFAULT;
    }

    double MTIProgram::getNotePan( int Note )
    {
        auto p = getOrCreate( Note );
        return p ? (double)p->pan.get() : PAN::DEFAULT;
    }

    int MTIProgram::getNumSoloNotes()
    {
        auto count = 0;

        for( auto note = 0; note < 128; ++note )
        {
            if( isNoteSolo( note ) )
                ++count;
        }
        return count;
    }

    int MTIProgram::resetNotesSolo()
    {
        if( getNumSoloNotes() != 0 )
        {
            soloingNotes.clear();
            return 0;
        }
        return -1;
    }

    /////////////
    ///////////// Sample Layer related API's
    /////////////
    std::array<juce::String, MAX_LAYERS> MTIProgram::getLayerSounds( int Note )
    {
        return getOrCreate( Note )->getSources();
    }

    bool MTIProgram::isLayerMuted( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->isMuted() : true ) : true;
    }

    bool MTIProgram::isLayerReversed( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->isReversed() : false ) : false;
    }

    bool MTIProgram::isLayerLooping( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->isLooping() : false ) : false;
    }

    bool MTIProgram::isLayerFade( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->isCrossFade() : false ) : false;
    }

    bool MTIProgram::isLayerWarp( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->isWarp() : false ) : false;
    }

    int MTIProgram::getLayerLoopStart( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->loopStart() : 0 ) : 0;
    }

    int MTIProgram::getLayerLoopEnd( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->loopEnd() : INT_MAX ) : INT_MAX;
    }

    double MTIProgram::getLayerPan( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->pan() : PAN::DEFAULT ) : PAN::DEFAULT;
    }

    double MTIProgram::getLayerGain( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->gain() : GAIN::DEFAULT ) : GAIN::DEFAULT;
    }

    int MTIProgram::getLayerSemi( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->semi() : SEMI::DEFAULT ) : SEMI::DEFAULT;
    }

    int MTIProgram::getLayerFine( int Note, int Layer )
    {
        auto p = getLayer( Note, Layer );
        return p ? ( p->src ? p->src->fine() : FINE::DEFAULT ) : FINE::DEFAULT;
    }

    void MTIProgram::setLayerMute( int Note, int Layer, bool Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->setMute( Value );
    }

    void MTIProgram::setLayerReverse( int Note, int Layer, bool Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->setReverse( Value );
    }

    void MTIProgram::setLayerLoop( int Note, int Layer, bool Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->setLooping( Value );
    }

    void MTIProgram::setLayerFade( int Note, int Layer, bool Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->setFade( Value );
    }

    void MTIProgram::setLayerWarp( int Note, int Layer, bool Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->setWarp( Value );
    }

    void MTIProgram::setLayerLoopStart( int Note, int Layer, int Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->loopStart = Value;
    }

    void MTIProgram::setLayerLoopEnd( int Note, int Layer, int Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->loopEnd = Value;
    }

    void MTIProgram::setLayerPan( int Note, int Layer, double Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->pan = Value;
    }

    void MTIProgram::setLayerGain( int Note, int Layer, double Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->gain = Value;
    }

    void MTIProgram::setLayerSemi( int Note, int Layer, int Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->semi = Value;
    }

    void MTIProgram::setLayerFine( int Note, int Layer, int Value )
    {
        if( auto p = getLayerSourceSlice( Note, Layer ) )
            p->fine = Value;
    }

    juce::String MTIProgram::getLayerSource( int Note, int Layer ) { return getLayer( Note, Layer )->source.get(); }
    int          MTIProgram::getLayerSlice( int Note, int Layer ) { return getLayer( Note, Layer )->slice.get(); }
    int          MTIProgram::getLayerOffset( int Note, int Layer ) { return getLayer( Note, Layer )->offset.get(); }
    int          MTIProgram::getLayerTailStart( int Note, int Layer ) { return getLayer( Note, Layer )->tailStart.get(); }
    int          MTIProgram::getLayerTailLength( int Note, int Layer ) { return getLayer( Note, Layer )->tailStart.get(); }
    int          MTIProgram::getLayerVelocityStart( int Note, int Layer ) { return getLayer( Note, Layer )->velStart.get(); }
    int          MTIProgram::getLayerVelocityEnd( int Note, int Layer ) { return getLayer( Note, Layer )->velEnd.get(); }

    void MTIProgram::setLayerSource( int Note, int Layer, const juce::String& Value ) { getLayer( Note, Layer )->source = Value; }
    void MTIProgram::setLayerSlice( int Note, int Layer, int Value ) { getLayer( Note, Layer )->slice = Value; }
    void MTIProgram::setLayerOffset( int Note, int Layer, int Value ) { getLayer( Note, Layer )->offset = Value; }
    void MTIProgram::setLayerVelocityStart(int note, int layer, int value) { if(CommonUtils::isValidMidiNote(note) && juce::isPositiveAndBelow(layer, MAX_LAYERS)) if(auto* data = getLayer(note, layer)) { data->velStart = juce::jlimit(0, 127, value); if(data->velEnd.get() < data->velStart.get()) data->velEnd = data->velStart.get(); } }
    void MTIProgram::setLayerVelocityEnd(int note, int layer, int value) { if(CommonUtils::isValidMidiNote(note) && juce::isPositiveAndBelow(layer, MAX_LAYERS)) if(auto* data = getLayer(note, layer)) { data->velEnd = juce::jlimit(0, 127, value); if(data->velStart.get() > data->velEnd.get()) data->velStart = data->velEnd.get(); } }
    void MTIProgram::setLayerTailStart( int Note, int Layer, int milliSec ) { getLayer( Note, Layer )->tailStart = milliSec; }
    void MTIProgram::setLayerTailLength( int Note, int Layer, int milliSec ) { getLayer( Note, Layer )->tailLength = milliSec; }
}
