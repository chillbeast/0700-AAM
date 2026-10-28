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
    ///
    /// ASound ASliceRenderer
    ///

    ASliceRenderer::ASliceRenderer()
    {
        next.clear();
    }

    void ASliceRenderer::setSlice( ASoundSlice* Slice )
    {
        finished.store( true );
        slice = Slice;
        reset();
    }

    void ASliceRenderer::reset() noexcept
    {
        inL = inR = nullptr;
        source = slice ? slice->getPlaybackSource( sourceRevision ) : nullptr;
        sourceLength = source ? juce::jmin( source->getNumSamplesPerChannel(), source->getData().getNumSamples() ) : 0;
        if( sourceLength > 0 )
        {
            inL = (float*)source->getData().getReadPointer( 0 );
            inR = (float*)source->getData().getReadPointer( 1 );
        }

        curPos = getStartPosition();
        setMute( false );
        start();
    }

    void ASliceRenderer::start() noexcept
    {
        finished.store( !( slice && inL && inR && slice->getEnd() > slice->getStart() && sourceLength > 0
            && slice->getSourceRevision() == sourceRevision ) );
    }

    void ASliceRenderer::setPosition( double Position )
    {
        if( !slice || slice->getSourceRevision() != sourceRevision || !std::isfinite( Position ) )
        {
            curPos = 0.0;
            finished = true;
            return;
        }

        const auto loopEnd   = juce::jlimit( 0.0, (double)sourceLength, (double)slice->loopEnd() );
        const auto loopStart = juce::jlimit( 0.0, loopEnd, (double)slice->loopStart() );
        const auto isForward = slice->isForward();
        const auto isLooping = slice->isLooping();

        if( loopEnd <= loopStart )
        {
            finished = true;
            curPos = loopStart;
            return;
        }
        if( !isLooping )
        {
            auto end = isForward ? ( Position >= loopEnd ) : ( Position < loopStart );
            if( !finished.load() )
                finished.store( end );
            curPos = std::clamp( Position, loopStart, loopEnd );
        }
        else
        {
            if( Position >= loopStart && Position < loopEnd )
                curPos = Position;
            else
            {
                const auto length = loopEnd - loopStart;
                auto offset = std::fmod( Position - loopStart, length );
                if( offset < 0.0 ) offset += length;
                curPos = loopStart + offset;
            }
        }
    }

    void ASliceRenderer::advance( double PitchRatio, int NumSamples )
    {
        step( PitchRatio, NumSamples );
    }

    juce::AudioSampleBuffer& ASliceRenderer::getNext( double PitchRatio )
    {
        if( finished.load() || !slice || !inL || !inR ) return silentFrame();
        if( slice->getSourceRevision() != sourceRevision ) { finished = true; return silentFrame(); }
        const int end = juce::jlimit( 0, sourceLength, slice->getEnd() );
        const int begin = juce::jlimit( 0, juce::jmax( 0, end ), slice->getStart() );
        if( end <= begin ) { finished = true; return silentFrame(); }
        curPos = juce::jlimit( (double)begin, std::nextafter((double)end, (double)begin), curPos );
        m0 = juce::jlimit( begin, end - 1, (int)std::floor( curPos ) );
        m1 = m0 + 1 < end ? m0 + 1 : ( slice->isLooping() ? begin : end - 1 );
        alpha = curPos - m0;
        const bool muted = isMuted() || slice->isMuted();
        auto& result = muted ? silentFrame() : interpolateSample();
        step( PitchRatio );
        return result;
    }

    void ASliceRenderer::getNextBlock( juce::AudioSampleBuffer& Block, double PitchRatio )
    {
        if( finished.load() || Block.getNumChannels() == 0 ) { Block.clear(); return; }

        const auto numSamples = Block.getNumSamples();
        const auto muted      = isMuted() || slice->isMuted();
        if( muted )
        {
            advance( PitchRatio, numSamples );
            Block.clear();
            return;
        }

        auto out0 = Block.getWritePointer( 0 );
        auto out1 = Block.getNumChannels() > 1 ? Block.getWritePointer( 1 ) : nullptr;

        if( out1 )
            for( auto i = 0; i < numSamples; ++i )
            {
                auto& sample = getNext( PitchRatio );
                // Copy to Bock.
                *out0++ = sample.getSample( 0, 0 );
                *out1++ = sample.getSample( 1, 0 );
            }
        else
            for( auto i = 0; i < numSamples; ++i )
            {
                auto& sample = getNext( PitchRatio );
                // Copy to Bock.
                *out0++ = sample.getSample( 0, 0 );
            }
    }

    void ASliceRenderer::step( double PitchRatio, int NumSamples )
    {
        if( !slice || finished.load() ) return;
        const auto distance = slice->getEffectiveResamplerRate() * PitchRatio * NumSamples;
        setPosition( curPos + ( slice->isForward() ? distance : -distance ) );
    }

} // namespace mt
