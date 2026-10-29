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

#pragma once

namespace mt
{
    class MTNoteList;
    class MTTrack;

    static sigslot::signal<MTTrack*> trackNameChanged;

    class MTTrack
        : public MTITrackData
        , public MTNoteList
        , public ICMidiClockListener
        , public juce::ValueTree::Listener
    {
    public:
        explicit MTTrack( MTAudioManager& M, MTSequence& Seq, juce::UndoManager* UM, juce::ValueTree& VT );
        ~MTTrack() { if(audioClock) audioClock->stop(); programDeletionConnection.disconnect(); vTree.removeListener( this ); trackPlayhead.clear(); };

        juce::ValueTree& getVT() { return vTree; };
        juce::ValueTree& getState() { return vTree; };

        /// The track's playhead and loop length in ticks. The sequencer thread advances the
        /// position; read elsewhere it is good to within a tick.
        unsigned int getTickPosition() const noexcept { return tickPos; }
        unsigned int getLengthInTicks() const noexcept { return lengthInTicks.load(); }

        // Index API's
        void       setIndex( int Index ) { MTITrackData::index = Index; };
        inline int getIndex() const { return MTITrackData::index; }

        juce::String getBank() const { return bank; }
        void         setBank( const juce::String& Bank )
        {
            auto b   = Bank.toUpperCase();
            auto idx = (int)( *b.getCharPointer() - 'A' );
            if( 0 <= idx && idx < 8 )
                bank = Bank;
        }

        void setName( const juce::String& Name );
        void setLevel( int Velocity );
        void setPan( const float Pan );
        void setLength( int Length );
        void setUsingSeqLength( bool UseSeqLength );
        void setMute( bool Muted );
        void setTranspose( int Transpose );
        void setMidiPort( const juce::String& MidiPort );
        void setMidiChannel( int MidiChannel );
        void setMidiOutChannel( int MidiChannel );
        void setMuteGroup( int Group );

        inline juce::String getId() const noexcept { return MTITrackData::id.get(); }
        inline juce::String getName() const noexcept { return MTITrackData::name.get(); }
        inline juce::String getMidiPort() const noexcept { return MTITrackData::midiPort.get(); }
        inline bool         isUsingSeqLength() const noexcept { return MTITrackData::isSeqLength.get(); };
        inline bool         isMute() const noexcept { return MTITrackData::mute.get(); };
        inline bool         isSolo() const noexcept { return MTITrackData::solo.get(); };
        inline int          getTranspose() const noexcept { return MTITrackData::transpose.get(); };
        inline int          getMidiChannel() const noexcept { return MTITrackData::midiChannel.get(); };
        inline int          getMidiOutChannel() const noexcept { return MTITrackData::midiOutChannel.get(); };
        inline int          getMuteGroup() const noexcept { return MTITrackData::muteGroup.get(); }
        inline int          getLength() const noexcept { return MTITrackData::isSeqLength.get() ? seq.getLength() : (int)MTITrackData::length.get(); };
        inline int          getLevel() const noexcept { return MTITrackData::level.get(); }
        inline float        getPan() const noexcept { return MTITrackData::pan.get(); }

        bool isMidiInAllowed( const juce::String& Source, int Channel ) const noexcept;

        MTIProgram* getProgram() const;
        std::shared_ptr<AudioPlaybackClock> getAudioPlaybackClock() const { return audioClock; }
        int         setProgram( MTIProgram* p );

        void play();
        void resume();
        void pause();
        void stop();
        void seek( int posInTicks );
        void refreshTiming() { lengthChangeHandler(); }
        juce::Range<int> getPlaybackLoopRange() const;
        void addRecordedNote(int Note, int Start, int Length, int Velocity, int GuardTicks);

        void onTick( unsigned int Tick ) override;
        void onBeat();
        void onBar();

        /*void addMidiNote( int NoteNumber, int Velocity );
        void stopMidiNote( int NoteNumber );*/
        void stopAllMidiNotes();
        int  processIncomingMidiMessage( const juce::MidiMessage& Msg );
        int  processIncomingMidiMessage( const MTMidiMessage& Msg );

        const MTMeterData& getMeter() const { return midiMeter.getMeter(); }
        inline void        processMidiMeter() { midiMeter.process(); }

        // Status
        inline bool              isTrackEffectivelyMuted() { return ( !MTITrackData::solo.get() && MTITrackData::mute.get() || soloMuted.load() ); }
        inline int               getEffectiveNoteNo( int Note ) { return Note + seq.getTranspose() + getTranspose(); };
        inline int               getEffectiveNoteVelocity( int Velocity ) { return (int)( Velocity * levelScaler ); };
        inline const MTPlayhead& playhead() { return trackPlayhead; }

        // Solo Mute; don't call this API's directly without careful analysis.
        inline void setSoloMute( bool Mute ) { soloMuted = Mute; }

    protected:
        inline void reloadMidiMeter( float Level ) { midiMeter.reload( Level ); };

    private:
        friend struct TrackAuditAccess;
        void        progChangeHandler();
        void        lengthChangeHandler();
        void        recalculateIndex( unsigned int Tick );
        void        valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property ) override;
        void        handleAsyncUpdate() override;
        inline void updateTrackPlayhead( unsigned tickpos ) { trackPlayhead.normized = lengthFactor * tickPos; };
        void        fillMidiMessages( unsigned int tick, unsigned int prevTick, int midiChannel );

        MTAudioManager&          manager;
        MTSequence&              seq;
        juce::UndoManager*       undoManager{ nullptr };
        std::atomic<MTIProgram*> program{ nullptr };
        MTPlayhead               trackPlayhead;

        juce::ValueTree                vTree{ IDs::TRACK };
        juce::Array<juce::MidiMessage> midiMsgs;
        juce::MidiMessageSequence      midiSeq;
        juce::String                   bank{ "A" };
        std::atomic<bool>              isPlaying{ false };

        MTMidiMeter midiMeter{ 40 };

        std::multimap<unsigned int, juce::MidiMessage> queuedNotes;
        std::map<std::pair<int, int>, uint64_t> suppressRecordedUntil;

        int                   noteIdx{ 0 };
        unsigned int          prevTick{ 0 };
        unsigned int          tickCtr{ 0 };
        std::atomic<unsigned int> tickPos{ 0 };
        double                levelScaler{ 1.f };
        struct LiveDestination { int note{ -1 }, channel{ 1 }; };
        std::array<std::array<LiveDestination, 128>, 16> liveDestinations{};
        int64_t tickOffset{0};
        bool scheduleStart{ true };
        juce::CriticalSection lock;
        std::shared_ptr<AudioPlaybackClock> audioClock;
        bool audioClockReset{true};
        sigslot::scoped_connection programDeletionConnection;

        std::atomic<bool>         soloMuted{ false };
        std::atomic<unsigned int> lengthInTicks{ 0 };
        std::atomic<double>       lengthFactor{ 0 };
        //unsigned int              count;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTTrack )
    }; // MTTrack

} // namespace MT
