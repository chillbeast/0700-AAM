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
    class MTTrack;
    class MTSequence;

    static sigslot::signal<MTSequence*> sequenceNameChanged;

    using seqOnBarFn  = std::function<void()>;
    using seqOnBeatFn = std::function<void()>;

    using seqPlaybackReachedEnd = std::function<void()>;
    using seqPlayHeadChangedFn  = std::function<void( MTBarNBeats&, double )>;
    using seqTempoChangedFn     = std::function<void( float, int, int, int )>;

    class MTSequence
        : public MTISequenceData
        , public MTTempoMgr
        , private juce::ValueTree::Listener
    {
    public:
        explicit MTSequence( MTAudioManager& M, juce::ValueTree& VT, juce::UndoManager* UM, MTPlayhead& GlobalPlayhead );
        virtual ~MTSequence();

        // Callback functions...
        seqPlayHeadChangedFn onPlayPositionChangedFn      = []( MTBarNBeats&, double ) {};
        seqTempoChangedFn    onTempoTimeSignatureChangeFn = []( float, int, int, int ) {};

        inline juce::ValueTree& getVT() { return vTree; }
        inline juce::ValueTree& getState() { return vTree; }

        // Properties
        inline juce::String getID() const { return id.get(); }
        inline juce::String getName() const { return name.get(); }
        inline int          getLength() const { return length.get(); }
        inline int          getLoopStart() const { return loopStart.get(); }
        inline int          getLoopEnd() const { return loopEnd.get(); }
        inline int          getTranspose() const { return transpose.get(); }
        inline bool         isUsingMasterBPM() const { return isMasterBPM.get(); }
        inline MTBarNBeats  getLengthBnB() { return MTBarNBeats{ getLength() }; }
        const int           getBPM();
        MTTimeBase          getTimeBase();

        // Index API's
        void       setIndex( int Index ) { MTISequenceData::index = Index; };
        inline int getIndex() const { return MTISequenceData::index; }

        int setName( const juce::String& Name );
        int setBPM( int BPM );
        int setTimeSignature(int Numerator, int Denominator);
        int setLength( int Bars );
        int setLength( float Multiplier );

        int setLoopStart( int Start );
        int setLoopEnd( int End );
        int setLoopPoints( int Start, int End );
        int setTranspose( int Transpose );
        int useMasterBPM( bool UseMasterBPM );
        int setLooping( bool IsLooping );

        bool getLoopingState() { return isLooping.get(); }
        int  modifySequenceLength( int ToModifyBars, bool Insert, int AtPosition = -1 );
        int  copyTrackBars( int trackIndex, const SequenceCopyBars& copyBars, std::vector<MTNote>& noteList );

        // Time related API's
        unsigned int getLengthInTicks();
        const double getLengthInSeconds();
        const double getLengthInSeconds( const MTBarNBeats& Time );
        const double getLengthInMS();

        inline void     setSelectedTrack( const int Index ) { selectedTrack = juce::jlimit<int>( -1, (int)tracks.size() - 1, Index ); }
        inline int      getSelectedTrack() const { return selectedTrack; }
        inline MTTrack* getSelectedTrackPtr() { return getTrack( selectedTrack ); }

        // Record state API's
        RecordState getRecordState() { return MTISequenceData::rState; };
        void        setRecordState( RecordState State ) { MTISequenceData::rState = State; };
        inline bool isArmed() { return RecordState::Armed == getRecordState(); };
        inline bool isRecording() { return RecordState::Recording == getRecordState(); };
        void setCountInBars(int bars) { pendingCountInBars = juce::jlimit(0, 2, bars); }
        bool isCountingIn() const { return countInRemaining.load() > 0; }
        unsigned int getCountInRemainingTicks() const { return countInRemaining.load(); }
        uint64_t getElapsedTicks() const { return elapsedTicks.load(); }
        bool hasStartedTimeline() const { return timelineStarted.load(); }

        // Playhead...
        inline MTPlayhead& playhead() { return sequencePlayhead; }
        inline void        playheadGlobalToLocal() { sequencePlayhead = globalPlayhead; }
        inline void        playheadLocalToGlobal() { globalPlayhead = sequencePlayhead; }

        // Positioning
        void setBBT( int knob, int value );
        void seek( int posInTicks );
        void seek( const MTBarNBeats& BnB );

        // Track API's...
        int createTrack( const juce::String& Name );
        int createTrack( juce::ValueTree& VT );

        int  deleteTrack( int Index );
        int  deleteTrack( const juce::String& Name );
        void deleteAllTracks();

        int              getTrackIndex( const juce::String& Name );
        int              getTrackIndex( const MTTrack* pTrack );
        MTTrack*         getTrack( int Index );
        MTTrack*         getTrack( const juce::String& Name );
        juce::ValueTree& getTrackState( int Index );
        juce::ValueTree& getTrackState( const juce::String& Name );

        const std::vector<juce::String>& getTrackList() const noexcept { return trackList; };
        const juce::Array<juce::String>  getValidRoutableTrackList( int Track );
        const int                        getNumMidiTracks();
        const juce::Array<int>&          getMidiTrackIndexes();

        int  setTrackSolo( const juce::String& Name, bool Solo );
        int  setTrackSolo( int Index, bool Solo );
        void updateTrackSolo( MTTrack* PTrk );

        inline bool             isTrackSolo( int Index ) const { return soloingTracks[Index]; }
        inline bool             isAnyTrackSolo() const { return !soloingTracks.isZero(); }
        inline bool             isTrackMutedDueToSolo( int Index ) const { return !soloingTracks.isZero() && !soloingTracks[Index]; }
        const juce::BigInteger& getSoloingTracks() const { return soloingTracks; }
        int                     getNumSoloTracks();
        int                     resetTrackSolos();

        int getNumTracks();
        int forAllTracks( std::function<void( MTTrack* T )> func );
        int forAllMidiTracks( std::function<void( MTTrack* T )> func );

        // Playback API's
        void prepareToPlay();
        void play();
        void resume();
        void pause();
        void stop();

        void onStart( unsigned int Ticks );
        void onTick( unsigned int Ticks );
        void onBeat();
        void onBar();
        void onLoopBegin( unsigned int );
        void onLoopEnd( unsigned int, unsigned int );
        void clearCounters();

    private:
        void                          seekSeqenceInternal( int posInTicks );
        void                          updateTrackSoloList();
        void                          updateTrackList();
        void                          lengthChangeHandler();
        void                          valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property ) override;
        void                          setupMidiClock( unsigned int bar );
        void                          initializeTickDataFromTempoSettings( const MTTempoSettings& tempoSettings );
        inline const MTTempoSettings& getSequenceTempoSettings( unsigned int bar )
        {
            return getTempoSettingsAtBar( bar ); // master tempo does not replace the sequence's meter
        };
        //void clearCounters();
        void isCircularRouted( const juce::String&, const MTTrack*, bool& );

        juce::Array<int> midiIndexes;

    private:
        MTAudioManager&    manager;
        juce::UndoManager* undoManager{ nullptr };

        MTPlayhead& globalPlayhead;
        MTPlayhead  sequencePlayhead;

        std::vector<std::unique_ptr<MTTrack>> tracks;
        std::vector<juce::String>             trackList;

        juce::CriticalSection lock;
        sigslot::scoped_connection trackNameConnection;
        juce::ValueTree       vTree{ IDs::SEQUENCE };
        juce::MidiFile        mFile;
        juce::File            seqMidiFile;

        bool             initialized{ false };
        bool             isPlaying{ false };
        int              selectedTrack{ 0 };
        juce::BigInteger soloingTracks;

        std::atomic<unsigned int> ticksPerBeat{ 0 };
        std::atomic<unsigned int> ticksPerBar{ 0 };
        std::atomic<unsigned int> ticksPer1_2{ 0 };
        std::atomic<unsigned int> ticksPer1_4{ 0 };
        std::atomic<unsigned int> ticksPer1_8{ 0 };
        std::atomic<unsigned int> ticksPer1_16{ 0 };

        std::atomic<unsigned int> barPos{ 0 };
        std::atomic<unsigned int> beatPos{ 0 };
        std::atomic<unsigned int> tickPos{ 0 };
        std::atomic<unsigned int> pos1_2{ 0 };
        std::atomic<unsigned int> pos1_4{ 0 };
        std::atomic<unsigned int> pos1_8{ 0 };
        std::atomic<unsigned int> pos1_16{ 0 };
        std::atomic<unsigned int> prevTicks{ 0 };
        int pendingCountInBars{0};
        unsigned int countInOffset{0}, lastCountInBeat{0};
        std::atomic<unsigned int> countInRemaining{0};
        std::atomic<uint64_t> elapsedTicks{0};
        std::atomic<bool> timelineStarted{false};

        double lengthFactor{ 1 };
        int    lengthInTicks{ 1 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTSequence )
    };

} // namespace MT
