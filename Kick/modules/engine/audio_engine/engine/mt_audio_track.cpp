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
    static int gTrackCtr = 0;

    MTTrack::MTTrack( MTAudioManager& M, MTSequence& Seq, juce::UndoManager* UM, juce::ValueTree& VT )
        : MTITrackData( VT, UM )
        , MTNoteList( VT, UM, Seq )
        , manager( M )
        , seq( Seq )
        , undoManager( UM )
        , vTree( VT )
    {
        ++gTrackCtr;

        if( 0 >= vTree.getProperty( IDs::id ).toString().length() )
            vTree.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        if( 0 >= vTree.getProperty( IDs::name ).toString().length() )
            vTree.setProperty( IDs::name, ( "Track " + juce::String( gTrackCtr ) ), nullptr );

        vTree.addListener( this );

        // Locate midi file for this track...
        /*
        {
            auto       projDir  = manager.getAssetManager()->getProjDir();
            auto       seqData  = Seq.getData();
            auto       n        = seqData.Name + juce::File().getSeparatorString() + data.Name + ".mid";
            juce::File midiFile = projDir.getChildFile( n );
            if( !midiFile.exists() )
            {
                midiFile.create();
            }
            {
                // Read Midifile...
                juce::MidiFile mFile;
                auto           stream = juce::FileInputStream( midiFile );
                mFile.readFrom( stream );
                if( auto trk = mFile.getTrack( 0 ) )
                    midiSeq = *trk;

                MTNoteList::setMidiList( &midiSeq );
            }
        }
        */

        // Setup handler for program being deleted
        programDeletionConnection = programBeingDeleted.connect( [this]( MTIProgram* MTIProg ) {
            if( program.load() == MTIProg )
            {
                setProgram( nullptr );
            }
        } );

        // Restore program.
        // Connect handler to length change handler.
        lengthChangeHandler();
        progChangeHandler();
    }

    void MTTrack::setName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            MTITrackData::name = Name;
    }

    void MTTrack::setLevel( int Level ) { MTITrackData::level = Level; }
    void MTTrack::setPan( const float Pan ) { MTITrackData::pan = Pan; }
    void MTTrack::setLength( const int Length )
    {
        auto len             = juce::jmin<int>( seq.length.get(), Length );
        MTITrackData::length = len;
    }
    void MTTrack::setUsingSeqLength( bool UseSeqLength ) { MTITrackData::isSeqLength = UseSeqLength; }
    void MTTrack::setMute( bool Muted )
    {
        MTITrackData::mute = Muted;
        if( 0 < muteGroup.get() )
        {
            seq.forAllTracks( [&, Muted]( MTTrack* t ) {
                if( muteGroup == t->muteGroup )
                {
                    if( t->isMute() != Muted )
                        t->setMute( Muted );
                }
            } );
        }
    }

    void MTTrack::setTranspose( int Transpose ) { MTITrackData::transpose = Transpose; }
    void MTTrack::setMidiPort( const juce::String& MidiPort ) { MTITrackData::midiPort.setValue( MidiPort, undoManager ); }
    void MTTrack::setMidiChannel( int MidiChannel ) { MTITrackData::midiChannel = MidiChannel; }
    void MTTrack::setMidiOutChannel( int MidiChannel ) { MTITrackData::midiOutChannel = MidiChannel; }
    void MTTrack::setMuteGroup( int Group ) { MTITrackData::muteGroup = Group; }

    bool MTTrack::isMidiInAllowed( const juce::String& Source, int Channel ) const noexcept
    {
        auto midiInCh   = MTITrackData::midiChannel.get();
        auto midiInPort = MTITrackData::midiPort.get();

        auto isSrcAllowed     = ( 0 == midiInPort.compareIgnoreCase( "all" ) ) || ( 0 == midiInPort.compareIgnoreCase( Source ) );
        auto isChannelAllowed = ( 0 >= midiInCh ) || ( Channel == midiInCh );
        return ( isSrcAllowed && isChannelAllowed );
    }

    MTIProgram* MTTrack::getProgram() const { return program.load(); }

    int MTTrack::setProgram( MTIProgram* p )
    {
        MTITrackData::programId = p ? p->id.get() : "";
        return 0;
    };

    void MTTrack::progChangeHandler()
    {
        const juce::ScopedLock guard(lock);
        auto newProgram = manager.getProgramById( programId.get() );
        if( newProgram != program.load() )
        {
            if(audioClock) audioClock->stop();
            audioClock.reset(); audioClockReset = true;
            if(auto* audio = dynamic_cast<MTAudioProgram*>(program.load())) audio->playbackStopped();
            stopAllMidiNotes();
            program.store( newProgram );
            if(auto* audio = dynamic_cast<MTAudioProgram*>(newProgram)) audioClock = audio->createPlaybackClock();
            manager.triggerTrackProgramChanged( getIndex(), program.load() );
        }
    }

    void MTTrack::lengthChangeHandler()
    {
        auto trackLength = getLength();
        lengthInTicks    = ( (unsigned int)( seq.getTicksUpto( trackLength ) ) );
        lengthFactor     = ( 1.0 / lengthInTicks );
    }

    void MTTrack::valueTreePropertyChanged( juce::ValueTree& TreeWhosePropertyHasChanged, const juce::Identifier& Property )
    {
        if( vTree == TreeWhosePropertyHasChanged )
        {
            if( Property == IDs::name )
                trackNameChanged( this );
            else if( Property == IDs::level )
                levelScaler = ( level.get() * MIDI::SCALER );
            else if( Property == IDs::solo )
                seq.updateTrackSolo( this );
            else if( Property == IDs::length || Property == IDs::usingSequenceLength )
                lengthChangeHandler();
            else if( Property == IDs::program )
                progChangeHandler();
            else if(Property == juce::Identifier("inputMonitor") && !(bool)TreeWhosePropertyHasChanged.getProperty("inputMonitor", true))
                stopAllMidiNotes();
        }
    }

    void MTTrack::handleAsyncUpdate()
    {
        manager.triggerTrackNotesChanged( manager.getSequenceIndex( &seq ), getIndex() );

        /*
        auto projDir = manager.getAssetManager()->getProjDir();
        auto n = seq.getName() + juce::File().getSeparatorString() + getName() + ".mid";
        juce::File midiFile = projDir.getChildFile( n );
        if( !midiFile.exists() )
        {
            midiFile.create();
        }
        {
            midiFile.replaceWithData( nullptr, 0 );
            juce::MidiFile mFile;
            mFile.setTicksPerQuarterNote( defaultPPQN );
            mFile.addTrack( midiSeq );
            auto stream = juce::FileOutputStream( midiFile );
            mFile.writeTo( stream );
        }
        */
    }

    void MTTrack::play()
    {
        const juce::ScopedLock guard(lock);
        if(audioClock) audioClock->stop(); audioClockReset = true;
        suppressRecordedUntil.clear();
        isPlaying = true;
        // The variables are cleared here and in stop as the workflow may be pause->playstart. if Ui calls stop and resume then it may lead to issues.
        trackPlayhead.clear();
        trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() ) : 0;
        prevTick              = 0;
        scheduleStart         = true;
        tickCtr               = 0;
        tickOffset = seq.isLooping.get() ? seq.getTicksUpto(juce::jmax(0, seq.getLoopStart())) : 0;
        tickPos = (unsigned)tickOffset;
    }

    void MTTrack::resume()
    {
        const juce::ScopedLock guard(lock);
        audioClockReset = true;
        isPlaying = true;
    }

    void MTTrack::pause()
    {
        const juce::ScopedLock guard(lock);
        if(audioClock) audioClock->stop();
        if(auto* audio = dynamic_cast<MTAudioProgram*>(program.load())) audio->playbackStopped();
        isPlaying = false;
        stopAllMidiNotes();
    }

    void MTTrack::stop()
    {
        const juce::ScopedLock guard(lock);
        if(audioClock) audioClock->stop(); audioClockReset = true;
        if(auto* audio = dynamic_cast<MTAudioProgram*>(program.load())) audio->playbackStopped();
        stopAllMidiNotes();
        isPlaying = false;
        trackPlayhead.clear();
        trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() ) : 0;
        prevTick              = 0;
        scheduleStart         = true;
        tickCtr               = 0;
        tickOffset = seq.isLooping.get() ? seq.getTicksUpto(juce::jmax(0, seq.getLoopStart())) : 0;
        tickPos = (unsigned)tickOffset;
    }

    void MTTrack::seek( int posInTicks )
    {
        const juce::ScopedLock guard(lock);
        if(audioClock) audioClock->stop(); audioClockReset = true;
        tickPos = juce::jmax(0, posInTicks);
        tickOffset = int64_t(tickPos) - int64_t(prevTick);
        scheduleStart = true;
        if( seq.isLooping.get() && lengthInTicks.load() > 0 )
            while( tickPos >= lengthInTicks.load() ) // This is cheap modulo
                tickPos = tickPos.load() % lengthInTicks.load();

        //// restrict the position between loop start and sequence length.
        //tickPos  = juce::jlimit<int>( 0, seq.getTicksUpto( getLength() ), posInTicks );
        //auto bnb = seq.toBnB( tickPos );

        //const juce::ScopedLock sl( lock );

        //trackPlayhead.bnb.tick = bnb.tick;
        //trackPlayhead.bnb.bar  = bnb.bar;
        //trackPlayhead.bnb.beat = bnb.beat;

        recalculateIndex( tickPos );
        updateTrackPlayhead( tickPos );
    }

    void MTTrack::recalculateIndex( unsigned int BeginTick )
    {
        noteIdx = 0;

        // If zero
        if( 0 == BeginTick )
            return;

        // Loop to find correct index...
        const auto& vt = getNoteListVT();
        while( noteIdx < vt.getNumChildren() )
        {
            auto     tmp = vt.getChild( noteIdx );
            MTNoteRO note( tmp );
            if( note.start() < (int)BeginTick )
                noteIdx++;
            else
                break;
        }
    }

    void MTTrack::onBeat()
    {
        trackPlayhead.bnb.tick = 0;
        trackPlayhead.bnb.beat++;
    };

    void MTTrack::onBar()
    {
        trackPlayhead.bnb.beat = 0;
        int last               = seq.isLooping.get() ? juce::jmin<int>( getLength(), seq.getLoopEnd() ) : juce::jmin<int>( getLength(), seq.getLength() );
        if( ++trackPlayhead.bnb.bar % last == 0 )
        {
            trackPlayhead.bnb.bar = trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() ) : 0;
            trackPlayhead.loopCount++;
        }
    }

    juce::Range<int> MTTrack::getPlaybackLoopRange() const
    {
        const int length = juce::jmax(1, (int)lengthInTicks.load());
        int start = seq.isLooping.get() ? (int)seq.getTicksUpto(juce::jmax(0, seq.getLoopStart())) : 0;
        if(start >= length) start = 0; // shorter independent tracks keep their own cycle
        const int end = seq.isLooping.get() ? juce::jmin(length, (int)seq.getTicksUpto(seq.getLoopEnd())) : length;
        return {start, juce::jmax(start + 1, end)};
    }

    void MTTrack::fillMidiMessages( unsigned int tick, unsigned int PrevTick, int MidiChannel )
    {
        //juce::Logger::writeToLog( "prevTick, " + juce::String( PrevTick ) + "PresentTick, " + juce::String(tick));

        const auto range = getPlaybackLoopRange();
        const int loopStart = range.getStart(), loopLength = range.getLength();
        for( auto pos = scheduleStart ? PrevTick : PrevTick + 1; pos <= tick; pos++ )
        {
            while(!queuedNotes.empty() && queuedNotes.begin()->first <= pos)
            {
                midiMsgs.add(queuedNotes.begin()->second);
                queuedNotes.erase(queuedNotes.begin());
            }
            const int64_t raw = int64_t(pos) + tickOffset - loopStart;
            int posOnTrack = loopStart + int((raw % loopLength + loopLength) % loopLength);
            for( const auto& it : getNotesAtPos( posOnTrack ) )
            {
                const auto key = std::make_pair(it.number, it.start);
                auto guard = suppressRecordedUntil.find(key);
                if(guard != suppressRecordedUntil.end())
                {
                    if(pos <= guard->second) continue; // don't echo a freshly quantized live hit
                    suppressRecordedUntil.erase(guard);
                }
                int noteNo = getEffectiveNoteNo( it.number );
                if( !isTrackEffectivelyMuted() && CommonUtils::isValidMidiNote( noteNo ) )
                {
                    // Check probability of playback.
                    auto rand = AudIO::utils::random<int>( PROBABILITY::MIN, PROBABILITY::MAX );
                    if( rand <= it.probability )
                    {
                        int noteVelocity = getEffectiveNoteVelocity( it.velocity );

                        // Trigger note now.
                        midiMsgs.insert( -1, juce::MidiMessage::noteOn( MidiChannel, noteNo, (juce::uint8)noteVelocity ) );

                        // Check Ratchet
                        const auto length = juce::jmax(1, it.length);
                        const auto parts = juce::jlimit(1, length, it.ratchet);
                        for(int part = 1; part < parts; ++part)
                        {
                            const auto time = pos + (unsigned)(int64_t(length) * part / parts);
                            queuedNotes.insert({time - 1, juce::MidiMessage::noteOff(MidiChannel, noteNo)});
                            queuedNotes.insert({time, juce::MidiMessage::noteOn(MidiChannel, noteNo, (juce::uint8)noteVelocity)});
                        }
                        // Queue last note off.
                        queuedNotes.insert( { ( pos + length - 1 ), juce::MidiMessage::noteOff( MidiChannel, noteNo, (juce::uint8)noteVelocity ) } ); // Queue note-off
                    }
                }
            }
        }
    }

    void MTTrack::onTick( unsigned int Tick )
    {
        const juce::ScopedLock guard(lock);
        auto pProgram = program.load();
        if( !pProgram || ( 0 == lengthInTicks.load() ) )
            return;
        if(pProgram->isAudio() && !isPlaying.load()) return;

        unsigned int diffTick = Tick - prevTick;

        tickCtr += diffTick;
        tickPos += diffTick;
        trackPlayhead.bnb.tick += diffTick;
        auto isEffectivelyMute   = isTrackEffectivelyMuted();
        auto trackMidiOutChannel = getMidiOutChannel();

        const juce::ScopedLock sl( lock );

        if(!pProgram->isAudio()) fillMidiMessages( Tick, prevTick, trackMidiOutChannel );
        scheduleStart = false;

        prevTick = Tick;

        if( tickPos >= lengthInTicks.load() )
        {
            tickPos = tickPos.load() % lengthInTicks.load();
            noteIdx = 0;
        }

        if(seq.isLooping.get())
        {
            const auto range = getPlaybackLoopRange();
            const int start = range.getStart(), span = range.getLength();
            const auto raw = int64_t(Tick) + tickOffset - start;
            tickPos = start + (unsigned)((raw % span + span) % span);
        }

        updateTrackPlayhead( tickPos );
        if(audioClock)
        {
            audioClock->update(tickPos.load(), seq.getBPM() * defaultPPQN / 60.0, getPlaybackLoopRange(), seq.isLooping.get(),
                isTrackEffectivelyMuted(), getLevel() / 127.f, getPan(), audioClockReset);
            audioClockReset = false;
            if(auto* audio = dynamic_cast<MTAudioProgram*>(pProgram)) audio->applySendAutomation((int)tickPos.load());
        }
        // add logic to compute normalized playhead position using lengthInTicks
        if( !midiMsgs.isEmpty() )
        {
            reloadMidiMeter( levelScaler );
            pProgram->noteProcess( midiMsgs );
            midiMsgs.clearQuick();
        }
    }

    void MTTrack::addRecordedNote(int note, int start, int length, int velocity, int guardTicks)
    {
        const juce::ScopedLock guard(lock);
        addOrModifyNote(note, start, length, velocity);
        suppressRecordedUntil[{note, start}] = uint64_t(prevTick) + juce::jmax(0, guardTicks);
    }

    int MTTrack::processIncomingMidiMessage( const juce::MidiMessage& Msg )
    {
        const juce::ScopedLock guard(lock);
        auto pProgram = program.load();

        // While muted only releases get through: dropping them leaves gated notes
        // (instruments, held keygroups) sounding forever once the track is unmuted.
        const bool isRelease = Msg.isNoteOff() || Msg.isAllNotesOff() || Msg.isAllSoundOff() || Msg.isResetAllControllers()
            || (Msg.isControllerOfType(64) && Msg.getControllerValue() < 64);
        if( pProgram && ( (!isTrackEffectivelyMuted() && monitor.get()) || isRelease ) )
        {
            juce::Array<juce::MidiMessage> list;

            if( Msg.isNoteOnOrOff() )
            {
                auto& destination = liveDestinations[juce::jlimit(0, 15, Msg.getChannel() - 1)][Msg.getNoteNumber()];
                if( Msg.isNoteOn() )
                {
                    if( destination.note >= 0 ) list.add( juce::MidiMessage::noteOff(destination.channel, destination.note) );
                    destination = { getEffectiveNoteNo(Msg.getNoteNumber()), getMidiOutChannel() };
                    if( CommonUtils::isValidMidiNote(destination.note) )
                        list.add( juce::MidiMessage::noteOn(destination.channel, destination.note, (juce::uint8)getEffectiveNoteVelocity(Msg.getVelocity())) );
                    else destination.note = -1;
                }
                else if( destination.note >= 0 )
                {
                    list.add( juce::MidiMessage::noteOff(destination.channel, destination.note) );
                    destination.note = -1;
                }
            }
            else if( Msg.isAftertouch() )
            {
                const auto destination = liveDestinations[juce::jlimit(0, 15, Msg.getChannel() - 1)][Msg.getNoteNumber()];
                if( destination.note >= 0 ) list.add( juce::MidiMessage::aftertouchChange(destination.channel, destination.note, Msg.getAfterTouchValue()) );
            }
            else
                list.insert( -1, Msg );

            pProgram->noteProcess( list );
        }
        if( !isTrackEffectivelyMuted() )
            reloadMidiMeter( levelScaler );

        return 0;
    }

    int MTTrack::processIncomingMidiMessage( const MTMidiMessage& Msg )
    {
        if( isMidiInAllowed( Msg.sourceName, Msg.message.getChannel() ) )
            return processIncomingMidiMessage( Msg.message );
        return -1;
    }

    /*inline void MTTrack::addMidiNote( int NoteNumber, int Velocity )
    {
        auto msg = juce::MidiMessage::noteOn( getMidiOutChannel(), NoteNumber, (juce::uint8)Velocity );
        processIncomingMidiMessage( msg );
    }

    inline void MTTrack::stopMidiNote( int NoteNumber )
    {
        auto msg = juce::MidiMessage::noteOff( getMidiOutChannel(), NoteNumber );
        processIncomingMidiMessage( msg );
    }*/

    inline void MTTrack::stopAllMidiNotes()
    {
        const juce::ScopedLock guard(lock);
        if(auto* target = program.load())
        {
            juce::Array<juce::MidiMessage> releases;
            for(auto& channel : liveDestinations) for(auto& note : channel)
            {
                if(note.note >= 0) releases.add(juce::MidiMessage::noteOff(note.channel, note.note));
                note.note = -1;
            }
            for(int channel = 1; channel <= 16; ++channel) releases.add(juce::MidiMessage::allNotesOff(channel));
            target->noteProcess(releases);
        }
        queuedNotes.clear();
    }

#if 0
    struct lessNote
    {
        template<typename T>
        bool operator()( const T& l, const T& r ) const
        {
            return l.second < r.second;
        }
    };

    int MTTrack::explode( const MTSelected& /*selection*/, std::vector<MTNoteData>* /*Notes*/ )
    {
        std::multiset < std::pair<std::shared_ptr<MTNoteData>, int>, lessNote > noteList;
        for( auto& it : notes_.getAllNotes() )
        {
            noteList.insert( std::make_pair( it, it->NoteNumber ) );
        }

        int prevNoteNum = -1;
        int trackIndex = -1;
        for( auto& it : noteList )
        {
            if( it.second != prevNoteNum )
            {
                trackIndex = sequence.createTrack( getName() + "_" + std::to_string( it.second ) );
                sequence.getTracks()[(size_t)trackIndex]->setProgram( getProgram() );
                prevNoteNum = it.second;
            }
            sequence.getTracks()[(size_t)trackIndex]->addNote( *it.first );
        }
        return 0;
    }

    int MTTrack::setLengthInBarAndBeats( int Bar, double Beats )
    {
        return setLength( sequence.tempoSequence.barsBeatsToTime( { Bar, Beats } ) );
    }

    double MTTrack::modifyLength( double Length, double Pos, bool Insert )
    {
        if( 0 <= Length )
        {
            // Update new Length...
            auto currLength = getLength();
            length = (Insert ? (currLength + Length) : (currLength - Length));

            for( const auto& itr : track->getClips() )
            {
                if( itr->getName() == KickMidiClipName )
                {
                    auto startBeat = (Insert ? (Pos) : (Pos - Length));
                    auto delta = (Insert ? (Length) : (-Length));

                    for( auto& it : notes_.getAllNotes() )
                    {
                        if( it->StartBeat >= startBeat )
                            notes_.editStartAndLength( &it, (it->StartBeat + delta), it->LengthInBeats );
                    }
                }
                // Set maximum lengths...
                itr->setLength( length, true );
            }
        }
        return getLength();
    }

#endif
}
