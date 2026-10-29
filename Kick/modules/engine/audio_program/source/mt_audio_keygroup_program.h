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
    class MTKeygroupProgram : public MTCommonProgram
    {
    public:
        explicit MTKeygroupProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        ~MTKeygroupProgram();

        MTIKeys* getKeysClip( int Note );
        MTIKeys* getZoneForNote(int Note);
        int getZoneKeyForNote(int Note);
        template<typename Visitor> void forEachZone(int Note, Visitor&& Visit)
        {
            for(auto& entry : iSampleMap)
                if(auto* zone = dynamic_cast<MTIKeys*>(entry.second.get()))
                    if(Note >= zone->minNote.get() && Note <= zone->maxNote.get()) Visit(entry.first, *zone);
        }
        void noteProcess(juce::Array<juce::MidiMessage>& Messages) override;

        LayerPlayType  getNoteLayerPlay( int Note );
        SamplePlayType getNoteSamplePlay( int Note ) override;

        void setNoteLayerPlay( int Note, LayerPlayType Value );
        void setNoteSamplePlay( int Note, SamplePlayType Value );

        // Metering....
        void               enableOutputMetering( bool Enable ) override { key->enableOutputMetering( Enable ); }
        const MTMeterData& readOutMeter() const override { return key->readOutMeter(); };

        // IMidiMsgProcessor
        void noteOn( int Note, int Velocity ) override { key->noteOn( Note, Velocity ); };
        void noteOff( int Note ) override { key->noteOff( Note ); };
        void noteOff( std::vector<int>& Notes ) override { key->noteOff( Notes ); };
        void noteOffAll() override { MTIProgram::noteOffAll(); key->noteOffAll(); };
        void noteMute( int Note, bool Mute ) override { key->noteMute( Note, Mute ); };
        void noteReset( int Note ) override { key->noteReset( Note ); };
        void noteReset( std::vector<int>& Notes ) override { key->noteReset( Notes ); };
        void noteLoop( int Note, bool Loop ) override { key->noteLoop( Note, Loop ); };

    protected:
    private:
        /// Creators
        std::unique_ptr<MTISample> iSampleCreator( int Note ) override;
        std::unique_ptr<MTISample> iSampleCreator( juce::ValueTree& VT, juce::UndoManager* UM ) override;

    private:
        AudIO::DawEngine& engine;
        KeygroupProgram*  key;
    };
} // namespace MT
