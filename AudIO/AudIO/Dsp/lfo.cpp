/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of AudIO.
 *
 * AudIO is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * AudIO is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with AudIO. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "lfo.h"

/// Tune following defines for compile time optimizations...
#define USE_INTERPOLATION ( 0 ) // [<=0] Do simple linear interpolation between table values. [>0] No Interpolation between values.

namespace AudIO
{

    /// Consts
    constexpr static double tableFactor2x  = ( 2.0 / static_cast<double>( LOOKUP_TABLE_SIZE ) );
    constexpr static double tableFactor2Pi = Math<double>::twoPi / static_cast<double>( LOOKUP_TABLE_SIZE );

    // Static member definitions.
    std::atomic<bool>                    LFO::tableInitialized{ false };
    std::array<float, LOOKUP_TABLE_SIZE> LFO::sineTable;
    std::array<float, LOOKUP_TABLE_SIZE> LFO::sawTable;
    std::array<float, LOOKUP_TABLE_SIZE> LFO::triangleTable;

    LFO::LFO()
    {
        // Create lookup for Noise...
        if( !tableInitialized.load() )
        {
            double value;
            for( int i = 0; i < LOOKUP_TABLE_SIZE; ++i )
            {
                // Sine...
                sineTable[i] = static_cast<float>( std::sin( i * tableFactor2Pi ) );
                // Saw...
                value       = ( i * tableFactor2x ) - 1.0;
                sawTable[i] = static_cast<float>( value );
                // Triangle
                triangleTable[i] = static_cast<float>( 2.0 * ( std::fabs( value ) - 0.5 ) );
            }
            tableInitialized.store( true );
        }
    }

    void LFO::setParameters( const LFO::Parameters& Params ) noexcept
    {
        params_      = Params;
        params_.Freq = utils::clamp<float>( Params.Freq, 0.001f, 20000.f );
        params_.Fade = utils::clamp<float>( Params.Fade, 0.001f, 5.f );
        recalculate();
    }

    void LFO::setFrequency( float Freq, bool Force ) noexcept
    {
        params_.Freq = utils::clamp<float>( Freq, 0.001f, 20000.f );
        params_.SHDuration = 1.f / params_.Freq;
        if(Force || !configured) recalculate();
        else if(this->sampleRate() > 0)
        {
            phaseIncrement = (double)params_.Freq / this->sampleRate();
            shSteps = utils::max<unsigned int>(1, static_cast<unsigned int>(this->sampleRate() * params_.SHDuration));
        }
    }

    void LFO::setPhase( float Phase ) noexcept
    {
        auto prevPhase = params_.Phase;
        params_.Phase  = utils::clamp<float>( Phase / Math<float>::twoPi, 0.f, 1.f );
        // Also update current phase.
        auto diff = ( params_.Phase - prevPhase );
        phase     = std::fmod( ( phase + diff ), 1.f );
    }

    void LFO::recalculate() noexcept
    {
        configured = true;
        phase       = params_.Phase;
        curPhase    = phase;
        curValue    = 0.f;
        shSteps     = utils::max<unsigned int>(1, static_cast<unsigned int>(this->sampleRate() * params_.SHDuration));
        shIncrement = shSteps;

        if( ( params_.Freq > 0.0001f ) && ( this->sampleRate() > 0 ) )
            phaseIncrement = ( (double)params_.Freq / (double)this->sampleRate() );

        delaySteps    = utils::max<unsigned int>( 0, static_cast<unsigned int>( this->sampleRate() * params_.Delay ) );
        fadeIncrement = ( 1.0 / ( this->sampleRate() * params_.Fade ) );
        curFade       = ( params_.Fade <= 0 ) ? 1.f : 0.f;
    }

    float LFO::poly_blep( float hp ) const
    {
        auto t  = hp;
        auto dt = static_cast<float>( phaseIncrement * Math<float>::pi_2 );

        // t-t^2/2 +1/2
        // 0 < t <= 1
        // discontinuities between 0 & 1
        if( t < dt )
        {
            t /= dt;
            return ( t + t - t * t - 1.f );
        }

        // t^2/2 +t +1/2
        // -1 <= t <= 0
        // discontinuities between -1 & 0
        else if( t > 1.f - dt )
        {
            t = ( t - 1.f ) / dt;
            return ( t * t + t + t + 1.f );
        }

        // no discontinuities
        // 0 otherwise
        else
            return 0.f;
    }

    void LFO::advance_internal( unsigned int NumSamples ) noexcept
    {
        auto phaseAdvanceSamples = NumSamples;
        // Increment delay...
        if( 0 < delaySteps )
        {
            delaySteps -= NumSamples;
            phaseAdvanceSamples = ( 0 > delaySteps ) ? -delaySteps : 0;
        }

        // Increment Phase...
        if( 0 < phaseAdvanceSamples )
        {
            phase += ( phaseIncrement * phaseAdvanceSamples );
            while( phase >= 1.0 )
                phase -= 1.0;

            auto newCurPhase = std::fmod( phase, 1.f );
            if( newCurPhase < 0 )
                newCurPhase += 1.0;

            curPhase = newCurPhase;

            // Increment fade...
            if( curFade < 1.0 )
                curFade += ( fadeIncrement * (float)phaseAdvanceSamples );
            if( curFade > 1.0 )
                curFade = 1.0;
        }
    }

    float LFO::nextSample_internal( unsigned int NumSamples ) noexcept
    {
        if( !this->isEnabled() )
            return 1.f;

        // Advance phase...
        advance_internal( NumSamples );

        if( this->isBypassed() || ( 0 < delaySteps ) )
            return 0.f;

        // Calculate index in lookup table corrosponding to current phase...
        auto wIdx = curPhase * LOOKUP_TABLE_SIZE;
        auto i0   = static_cast<int>( wIdx );

#if defined( USE_INTERPOLATION ) && ( USE_INTERPOLATION > 0 )
        auto frac = wIdx - i0;
        auto i1   = i0 + 1;
        if( LOOKUP_TABLE_SIZE <= i1 )
            i1 -= LOOKUP_TABLE_SIZE;
#endif

        switch( params_.Type )
        {
            case LFOType::SAW_DOWN:
            {
                // Reversed index in sawDown.
                i0 = ( LOOKUP_TABLE_SIZE - 1 - i0 );
#if defined( USE_INTERPOLATION ) && ( USE_INTERPOLATION > 0 )
                frac = 1.0 - frac;
                i1   = ( LOOKUP_TABLE_SIZE - 1 - i1 );
#endif
            }
            /// Intentionally fall-through; don't break here.
            case LFOType::SINE:
            case LFOType::SAW_UP:
            case LFOType::TRIANGLE:
            {
                // Locate wave table to use...
                auto& wavTable = ( LFOType::SINE == params_.Type ) ? sineTable :
                                                                     ( ( LFOType::TRIANGLE == params_.Type ) ? triangleTable : sawTable );

#if defined( USE_INTERPOLATION ) && ( USE_INTERPOLATION > 0 )
                /// Linear interpolator
                curValue = static_cast<float>( wavTable[i0] + ( ( wavTable[i1] - wavTable[i0] ) * frac ) );
#else
                curValue = static_cast<float>( wavTable[i0] );
#endif
            }
            break;
            case LFOType::SQUARE:
            {
                curValue = static_cast<float>( curPhase < 0.5 ? 1.0 : -1.0 );
            }
            break;
            case LFOType::SAMPLE_HOLD:
            {
                const auto steps = utils::max<unsigned int>(1, shSteps);
                shIncrement += NumSamples;
                if( shIncrement >= steps )
                {
                    shIncrement %= steps;
                    heldValue = utils::random(-1.f, 1.f);
                }
                curValue = heldValue;
            }
            break;
            default: //case LFOType::NOISE:
            {
                curValue = utils::random( -1.f, 1.f );
            }
            break;
        }
        // Apply fade-in
        if( utils::isNotEqual( 1.0, curFade ) )
            curValue = static_cast<float>( curValue * curFade );
        // Apply depth
        if( utils::isNotEqual( 1.f, params_.Depth ) )
            curValue = ( curValue * params_.Depth );
        // Limits.
        curValue = utils::clamp( ( curValue + params_.Offset ), -1.f, 1.f );
        return curValue;
    }

    float LFO::nextSample_polyBlep( unsigned int NumSamples ) noexcept
    {
        nextSample_internal( NumSamples );
        switch( params_.Type )
        {
            case LFOType::SAW_DOWN:
            case LFOType::SAW_UP:
            case LFOType::TRIANGLE:
                curValue -= poly_blep( (float)curPhase ); // Layer output of Poly BLEP on top
                break;
            case LFOType::SQUARE:
                curValue += poly_blep( (float)curPhase ); // Layer output of Poly BLEP on top (flip)
                curValue -= poly_blep( (float)fmod( curPhase + 0.5, 1.0 ) ); // Layer output of Poly BLEP on top (flop)
                break;
            default:
                // Don't care about NOISE, Sample & Hold...
                // SINE: // No harmonics in sine so no aliasing!! No Poly BLEPs needed!
                break;
        }

        return curValue;
    }

    void LFO::process( Buffer<float>& Buffer ) noexcept
    {
        auto bSize  = Buffer.getBlockSize();
        auto nChans = Buffer.getNumInputChannels();

        for( uint32_t channel = 0; channel < nChans; channel++ )
        {
            if( auto out = Buffer.getOutputBuffer( channel ) )
            {
                for( uint32_t sample = 0; sample < bSize; ++sample )
                    *out++ = advance( 1 );
            }
        }
    }
};

#undef NO_INTERPOLATION
