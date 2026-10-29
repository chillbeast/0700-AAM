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
    ///
    /// ISynthVoice
    ///
    ISynthVoice::ISynthVoice( const MTIAtomicProgramMaster& Master, ASoundSlice* Slice )
        : master( Master )
    {
        ASliceRenderer::setSlice( Slice );
    }

    void ISynthVoice::prepareToPlay( double SampleRate, int Blocksize )
    {
        sampleRate = SampleRate;
        if( auto t = ASliceRenderer::getSlice() )
            t->setReSampleRate( SampleRate );

        gain.reset( SampleRate, 0.002 );
    }

    void ISynthVoice::noteOn( int Note, int Velocity )
    {
        state.store( State::SETUP );
        noteNo = Note;
        if(auto* slice = ASliceRenderer::getSlice()) slice->setReSampleRate(sampleRate);

        gain.setCurrentAndTargetValue( 0.0 );
        gain.setTargetValue( ( (double)Velocity / 127.0 ) ); // Linear scale...

        state.store( State::RUNNING );
        ASliceRenderer::start();
    }

    void ISynthVoice::noteOff()
    {
        if( isRunning() )
        {
            state.store( State::STOPPING );
            ASliceRenderer::stop();
        }
    }

    void ISynthVoice::noteMute( int Note, bool Mute )
    {
        if( isPlayingNote( Note ) )
            setMute( Mute );
    }

    void ISynthVoice::noteReset( int Note )
    {
        if( isRunning() ) reset();
    }

    void ISynthVoice::noteOff( int Note )
    {
        if( isPlayingNote( Note ) )
            noteOff();
    }

    void ISynthVoice::process( juce::AudioBuffer<float>& Buffer )
    {
        if( isRunning() )
        {
            //const auto numSamples = juce::jmin( AUDIO_BLOCK_SIZE, Buffer.getNumSamples() );
            const auto numSamples = Buffer.getNumSamples();

            // Render next block
            ASliceRenderer::getNextBlock( tempBuffer, master.pitchRatio.load() );

            // Apply voice gain...
            gain.applyGain( tempBuffer, numSamples );

            // Add to the `Buffer`
            Buffer.addFrom( 0, 0, tempBuffer, 0, 0, numSamples );
            Buffer.addFrom( 1, 0, tempBuffer, 1, 0, numSamples );

            if( hasFinished() )
                makeFree();
        }
    }

} // namespace mt
