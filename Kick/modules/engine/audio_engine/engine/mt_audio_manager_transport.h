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

#pragma once

namespace mt
{
    class MTAudioTransportManager
        : public MTAudioSequenceManager
        , private ICMidiClockListener
    {
    public:
        std::function<void( TransportState State )> transportStateChanged;

        explicit MTAudioTransportManager( MTSelected& Selected, MTAudioManager& Manager );
        virtual ~MTAudioTransportManager();

        void close();
        void detachSequence(MTSequence* Sequence);
        void setup();

        // Getters.
        inline juce::ValueTree& getRuntimeState() noexcept { return runtimeVT; };
        inline MTSequence*      getPlayingSequence() { return playingSequence; }

        // Transport control API's
        TransportState getTransportState();
        void           setTransportState( TransportState TState );

        bool isArmed();
        bool isRecording();

        bool isPlaying() { return TransportState::Playing == getTransportState(); }
        bool isNotPlaying() { return TransportState::Playing != getTransportState(); }
        bool isStopped() { return TransportState::Stopped == getTransportState(); }
        bool isPaused() { return TransportState::Paused == getTransportState(); }
        bool isSongTransportActive() const { return songPlaying; }

        int  play( int Seq, bool FromBeginning, bool Now );
        int  play( int Seq );
        int  resume( int Seq );
        int  pause();
        int  stop();
        void seek( int posInTicks );
        void updatePlayheads();

        void setupSong( MTSongInfo, int );

        void playSequence( int Sequence );
        void stopSequence();
        void setNextSequence( int NextSequence );
        void setNextBarFlag( bool NextBarFlag );

    private:
        void onTick( unsigned int Ticks ) override;
        void advanceTransport();
        void onBeat();
        void onBar();
        void onStart( unsigned int ) override;
        void onLoopBegin( unsigned int );
        void onLoopEnd( unsigned int, unsigned int );
        void processTick();

        void resetSong();

    private:
        std::atomic<bool> barPending{false};
        sigslot::scoped_connection barConnection;
        CMidiClock&     midiClock{ getMidiClock() };
        juce::ValueTree runtimeVT{ IDs::RUNTIME };

        MTSequence* playingSequence{ nullptr };

        struct PlayOnNextBar
        {
            std::atomic<bool> isEnabled{ false };
            std::atomic<bool> fromBegin{ true };
            std::atomic<int>  seq{ 0 };
        };

        struct PlayManager
        {
            int  timeInTicks{ 0 };
            int  seq{ -1 };
            int  repeatCount{ 1 };
            int  nextSeqUpdateTicks{ 0 };
            bool isEnabled{ true };
            bool fromBegin{ true };
        };

        bool songPlaying{false};
        int                     stepIndex{ -1 };
        int                     lastSeqTicks{ 0 };
        int                     nextSequenceUpdateTicks{ -1 };
        int                     playTime{ 0 };
        std::queue<PlayManager> playQueue;

        int                       nextSequenceToPlay{ -1 };
        PlayOnNextBar             _nextBar;
        std::atomic<bool>         stopTickProcessing{ false };
        std::atomic<bool>         threadIsProcessingTick{ false };
        std::atomic<unsigned int> prevTick{ 0 };
        std::atomic<unsigned int> curTick{ 0 };

        std::thread*        tickProcessingThread;
        juce::WaitableEvent onTickEvent;
    };

} // namespace MT
