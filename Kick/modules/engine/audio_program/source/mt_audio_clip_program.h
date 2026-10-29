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

#include "mt_audio_iprogram.h"

namespace mt
{
    class MTClipProgram
        : public MTIProgram
        , public MTIClipProgramData
    {
    public:
        explicit MTClipProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        ~MTClipProgram();

        /// Program parameter API's
        float getQuantize();
        void  setQuantize( float QuantizeTimebase );

        ///
        /// Clip parameter API's
        ///
        MTIClip* getClip( int Note );

        /// Setters
        void setClipSamplePlay( int Note, ClipPlayType Type );

        /// Getters
        ClipPlayType getClipSamplePlay( int Note );

        SamplePlayType getNoteSamplePlay( int Note ) override { return SamplePlayType::ONE_SHOT; };

        /// Metering....
        void               enableOutputMetering( bool Enable ) override { pClip->enableOutputMetering( Enable ); }
        const MTMeterData& readOutMeter() const override { return pClip->readOutMeter(); };

        /// IMidiMsgProcessor
        void noteOn( int Note, int Velocity ) override { pClip->noteOn( Note, Velocity ); };
        void noteOff( int Note ) override { pClip->noteOff( Note ); };
        void noteOff( std::vector<int>& Notes ) override { pClip->noteOff( Notes ); };
        void noteOffAll() override { MTIProgram::noteOffAll(); pClip->noteOffAll(); };
        void noteMute( int Note, bool Mute ) override { pClip->noteMute( Note, Mute ); };
        void noteReset( int Note ) override { pClip->noteReset( Note ); };
        void noteReset( std::vector<int>& Notes ) override { pClip->noteReset( Notes ); };
        void noteLoop( int Note, bool Loop ) override { pClip->noteLoop( Note, Loop ); };

    private:
        void refresh();
        ///
        /// Creators
        ///
        std::unique_ptr<MTISample> iSampleCreator( int Note ) override;
        std::unique_ptr<MTISample> iSampleCreator( juce::ValueTree& VT, juce::UndoManager* UM ) override;

    private:
        AudIO::DawEngine& engine;
        ClipProgram*      pClip;

        //std::map<int, std::unique_ptr<MTIClip>> clipMap;
    };
} // namespace MT
