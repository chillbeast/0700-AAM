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
    class ASliceRenderer
    {
    public:
        ASliceRenderer();
        ~ASliceRenderer() = default;

        void         setSlice( ASoundSlice* Slice );
        ASoundSlice* getSlice() noexcept { return slice; }

        // Duration.
        double getLength()
        {
            if( !slice || !source || source->getSampleRate() <= 0.0 ) return 0.0;
            const int end = juce::jlimit( 0, sourceLength, slice->getEnd() );
            const int begin = juce::jlimit( 0, end, slice->getStart() );
            return (end - begin) / source->getSampleRate();
        }

        // Mute.
        bool isMuted() const noexcept { return mute.load(); }
        void setMute( bool State ) noexcept { mute.store( State ); }

        // Positional.
        bool hasFinished() const noexcept { return finished.load(); }
        bool isRunning() const noexcept { return !finished.load(); }

        inline double getPosition() { return curPos; }
        void          setPosition( double Position );
        void          advance( double PitchRatio, int NumSamples = 1 );

        // Control
        void reset() noexcept;
        void start() noexcept;
        void stop() noexcept { finished.store( true ); }

        // Samples.
        juce::AudioSampleBuffer& getNext( double PitchRatio = 1.0 );
        void                     getNextBlock( juce::AudioSampleBuffer& Block, double PitchRatio = 1.0 );

    private:
        void step( double PitchRatio, int NumSamples = 1 );
        juce::AudioSampleBuffer& silentFrame() noexcept { next.clear(); return next; }

        inline double getStartPosition()
        {
            return !slice ? 0.0 : ( slice->isForward() ? slice->loopStart() : juce::jmax( slice->loopStart(), slice->loopEnd() - 1 ) );
        }

        inline juce::AudioSampleBuffer& interpolateSample()
        {
            next.setNotClear(); // Cached pointers do not update JUCE's cleared-buffer flag.
            *pL = slice->gL * linearInterpolator( alpha, inL[m0], inL[m1] );
            *pR = slice->gR * linearInterpolator( alpha, inR[m0], inR[m1] );
            return next;
        }

    private:
        ASoundSlice* slice{ nullptr };
        ASound::Ptr source; // Keep cached sample pointers alive through source replacement.
        int sourceLength{ 0 };
        uint64_t sourceRevision{ 0 };

        std::atomic<bool> finished{ true };
        std::atomic<bool> mute{ false };

        juce::AudioSampleBuffer next{ 2, 1 };
        float* const            pL{ next.getWritePointer( 0 ) };
        float* const            pR{ next.getWritePointer( 1 ) };

        float* inL{ nullptr };
        float* inR{ nullptr };

        double curPos{ 0.0 };
        double alpha{ 0 };
        int    m0{ 0 };
        int    m1{ 0 };
    };

} // namespace mt
