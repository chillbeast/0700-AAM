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
    class ISynthVoice
        : public ASliceRenderer
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<ISynthVoice>;

        explicit ISynthVoice( const MTIAtomicProgramMaster& Master, ASoundSlice* Slice = nullptr );

        // Voices are owned as std::unique_ptr<ISynthVoice> (see ISynthObject), so the
        // derived destructor must run: DrumVoice's parameter atomics listen to the
        // program's ValueTree and would otherwise be left dangling.
        virtual ~ISynthVoice() = default;

        // Getters and Setters
        inline bool isFree() { return State::FREE == state.load(); }
        inline bool isSettingup() { return State::SETUP == state.load(); }
        bool isStopping() const { return state.load() == State::STOPPING; }
        inline bool isRunning() { return ( State::RUNNING == state.load() ) || ( State::STOPPING == state.load() ); }
        inline bool isPlayingNote( int Note ) { return isRunning() && ( noteNo == Note ); }

        inline void makeRunning() { state = State::RUNNING; }
        inline void makeFree()
        {
            state  = State::FREE;
            noteNo = -1;
        }
        inline ISynthVoice* changeToSettingupAndReturn()
        {
            state = State::SETUP;
            return this;
        }

        // NOTE...
        inline int getNote() { return noteNo; }
        uint64_t getStartOrder() const { return startOrder; }
        void setStartOrder(uint64_t Order) { startOrder = Order; }

        virtual void noteOff();
        virtual void noteOn( int Note, int Velocity );
        void         noteMute( int Note, bool Mute );
        void         noteReset( int Note );
        void         noteOff( int Note );

        virtual void prepareToPlay( double SampleRate, int blocksize );
        virtual void process( juce::AudioBuffer<float>& Buffer );

    protected:
        enum class State
        {
            FREE,
            SETUP,
            RUNNING,
            STOPPING,
        };

        juce::AudioBuffer<float>   tempBuffer{ 2, AUDIO_BLOCK_SIZE };
        juce::SmoothedValue<float> gain{ 1.0 };

        const MTIAtomicProgramMaster& master;

        int    noteNo{ 0 };
        double sampleRate{ 48000 };

        std::atomic<State> state{ State::FREE };
        uint64_t startOrder{0};
    };

} // namespace mt
