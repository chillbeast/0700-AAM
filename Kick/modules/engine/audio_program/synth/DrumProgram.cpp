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

// module include files...

#include "drum_voice.h"

namespace mt
{
    /// @brief
    DrumProgram::DrumProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PDrum )
        : ISynthObject( D, POLYPHONY::MAX )
    {
        setName( Name.toStdString() );
        setProgram( PDrum );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
        {
            voices.emplace_back( std::make_unique<DrumVoice>( master ) );
        }
        ISynthObject::addVoices( voices );
    }

    DrumProgram::~DrumProgram()
    {
        setBypass( true );
    }

    void DrumProgram::setProgram( MTIProgram* PDrum )
    {
        // call base class method...
        ISynthObject::setProgram( PDrum );

        noteOffAll();
        pDrum = dynamic_cast<MTDrumProgram*>( PDrum );
        setBypass( ( pDrum == nullptr ) );
    }

    void DrumProgram::noteOn(int note, int velocity)
    {
        if(!pDrum || !CommonUtils::isValidMidiNote(note) || triggering[(size_t)note]) return;
        juce::ScopedValueSetter<bool> guard(triggering[(size_t)note], true);
        if(auto* pad = pDrum->getDrumClip(note))
        {
            for(auto other : pDrum->getNotesInSameMuteGroup(note)) if(CommonUtils::isValidMidiNote(other)) noteOff(other);
            for(auto other : pad->sMute.getActions()) if(CommonUtils::isValidMidiNote(other)) noteOff(other);
            std::array<int, MAX_LAYERS> valid{}; int count = 0;
            for(int layer = 0; layer < MAX_LAYERS; ++layer)
                if(auto* slice = pad->getSourceSlice(layer))
                    if(slice->source && slice->source->isValid()) valid[count++] = layer;
            int layer = -1;
            if(count && !pDrum->isNoteEffectivelyMuted(note))
            {
                switch(pad->layerPlay.get())
                {
                    case LayerPlayType::CYCLE: layer = valid[(noteLayerIdx[note]++ % count)]; break;
                    case LayerPlayType::RANDOM: layer = valid[AudIO::utils::random<int>(0, count - 1)]; break;
                    case LayerPlayType::VELOCITY:
                        for(int i = 0; i < count; ++i)
                            if(velocity >= pad->layers[valid[i]]->velStart.get() && velocity <= pad->layers[valid[i]]->velEnd.get()) { layer = valid[i]; break; }
                        break;
                }
            }
            if(layer >= 0)
                if(auto* voice = dynamic_cast<DrumVoice*>(getFreeVoice(note, pad->polyPhony.get())))
                {
                    voice->setSlice(pad->getSourceSlice(layer));
                    voice->attachToAndReset(pad->vTree);
                    voice->setKeygroupParameters(std::exp2(pad->cents.load() / 1200.0), pad->gL.load(), pad->gR.load());
                    voice->noteOn(note, velocity); addVoiceForPlayback(voice);
                }
            for(auto other : pad->sPlay.getActions()) if(CommonUtils::isValidMidiNote(other)) noteOn(other, velocity);
        }
    }

};
