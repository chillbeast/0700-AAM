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
#ifndef AUDIO_LFO_H_INCLUDE
    #define AUDIO_LFO_H_INCLUDE

    #include <array>

    #include "../Includes/idsp.h"
    #include "../Includes/Utils.h"

namespace AudIO
{
    /// Tune following defines for compile time optimizations...
    #define LOOKUP_TABLE_SIZE ( 1024 ) // Make sure that this value is a power of 2

    enum LFOType
    {
        SINE,
        TRIANGLE,
        SAW_UP,
        SAW_DOWN,
        SQUARE,
        NOISE,
        SAMPLE_HOLD,
    };

    class LFO : public IDSP<float>
    {
    public:
        struct Parameters
        {
            LFOType Type{ LFOType::SINE }; // Oscillator type...
            float   Freq{ 0 }; // Frequency...
            float   Offset{ 0 }; // DC value offset.
            float   Phase{ 0 }; // Starting phase...
            float   Depth{ 1.f }; //
            float   Delay{ 0 }; // Delay (in seconds) after which oscillator starts producing output.
            float   Fade{ 0.001f }; // Fade duration (in seconds)
            float   SHDuration{ 0.25f }; // Sample duration (in seconds) for Sample and Hold mode.
        };

        LFO();

        void setParameters( const Parameters& Params ) noexcept;

        inline float getFrequency() const noexcept { return params_.Freq; }
        void         setFrequency( float Freq, bool Force = false ) noexcept;

        inline LFOType getType() const noexcept { return params_.Type; }
        inline void    setType( LFOType Typ ) { params_.Type = Typ; }

        float getPhase() const noexcept { return ( Math<float>::twoPi * params_.Phase ); }
        void  setPhase( float Phase ) noexcept;

        inline float operator()() noexcept { return advance( 1 ); }
        inline float load() const noexcept { return curValue; }

        void  reset() noexcept override { recalculate(); }
        float process() noexcept override { return advance( 1 ); }
        float process_polyBlep() noexcept { return advance_polyBlep( 1 ); }
        void  process( Buffer<float>& Buffer ) noexcept override;

        float advance( unsigned int NumSamples ) noexcept override { return nextSample_internal( NumSamples ); }
        float advance_polyBlep( unsigned int NumSamples ) noexcept { return nextSample_polyBlep( NumSamples ); }

    private:
        bool configured{false};
        float heldValue{0.f};
        static std::atomic<bool>                    tableInitialized;
        static std::array<float, LOOKUP_TABLE_SIZE> sineTable;
        static std::array<float, LOOKUP_TABLE_SIZE> sawTable;
        static std::array<float, LOOKUP_TABLE_SIZE> triangleTable;

        Parameters   params_;
        float        curValue{ 0.f };
        double       curFade{ 0 };
        double       phase{ 0 };
        double       curPhase{ 0 };
        double       fadeIncrement{ 0 };
        double       phaseIncrement{ 0 };
        int          delaySteps{ 0 };
        unsigned int shSteps{ 0 };
        unsigned int shIncrement{ 0 };

        inline void  recalculate() noexcept override;
        inline float poly_blep( float hp ) const;
        inline void  advance_internal( unsigned int NumSamples ) noexcept;

        // Defined in lfo.cpp and called by advance() from other translation units.
        float nextSample_internal( unsigned int NumSamples ) noexcept;
        inline float nextSample_polyBlep( unsigned int NumSamples ) noexcept;
    };

};

#endif // AUDIO_LFO_H_INCLUDE
