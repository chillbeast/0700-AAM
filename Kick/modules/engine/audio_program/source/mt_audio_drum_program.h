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

    class MTDrumProgram : public MTCommonProgram
    {
    public:
        explicit MTDrumProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        ~MTDrumProgram();

        MTIDrum* getDrumClip( int Note );

        LayerPlayType  getNoteLayerPlay( int Note );
        SamplePlayType getNoteSamplePlay( int Note );

        void setNoteLayerPlay( int Note, LayerPlayType Value );
        void setNoteSamplePlay( int Note, SamplePlayType Value );

        // Metering....
        void               enableOutputMetering( bool Enable ) override { drum->enableOutputMetering( Enable ); }
        const MTMeterData& readOutMeter() const override { return drum->readOutMeter(); };

        // IMidiMsgProcessor
        void noteOn( int Note, int Velocity ) override { drum->noteOn( Note, Velocity ); };
        void noteOff( int Note ) override { drum->noteOff( Note ); };
        void noteOff( std::vector<int>& Notes ) override { drum->noteOff( Notes ); };
        void noteOffAll() override { MTIProgram::noteOffAll(); drum->noteOffAll(); };
        void noteMute( int Note, bool Mute ) override { drum->noteMute( Note, Mute ); };
        void noteReset( int Note ) override { drum->noteReset( Note ); };
        void noteReset( std::vector<int>& Notes ) override { drum->noteReset( Notes ); };
        void noteLoop( int Note, bool Loop ) override { drum->noteLoop( Note, Loop ); };

    protected:
    private:
        /// Creators
        std::unique_ptr<MTISample> iSampleCreator( int Note ) override;
        std::unique_ptr<MTISample> iSampleCreator( juce::ValueTree& VT, juce::UndoManager* UM ) override;

    private:
        AudIO::DawEngine& engine;
        DrumProgram*      drum{ nullptr };
    };

} // namespace MT
