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
    KeygroupProgram::KeygroupProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PKeys )
        : ISynthObject( D, POLYPHONY::MAX )
    {
        setName( Name.toStdString() );
        setProgram( PKeys );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
        {
            voices.emplace_back( std::make_unique<DrumVoice>( master ) );
        }
        ISynthObject::addVoices( voices );
        releaseTriggers.reserve(128);
    }

    KeygroupProgram::~KeygroupProgram()
    {
        setBypass( true );
    }

    void KeygroupProgram::setProgram( MTIProgram* PKeys )
    {
        // call base class method...
        ISynthObject::setProgram( PKeys );

        noteOffAll();
        pKeys = dynamic_cast<MTKeygroupProgram*>( PKeys );
        setBypass( ( pKeys == nullptr ) );
    }

    void KeygroupProgram::noteOffAll()
    {
        releaseTriggers.clear(); ISynthObject::noteOffAll();
    }

    void KeygroupProgram::processNoteMessage(const juce::MidiMessage& message)
    {
        if(!pKeys || !message.isNoteOnOrOff()) return;
        const int note = message.getNoteNumber(), velocity = message.getVelocity();
        if(message.isNoteOn())
        {
            releaseTriggers.erase(std::remove_if(releaseTriggers.begin(), releaseTriggers.end(), [note](const auto& pending) { return pending.note == note; }), releaseTriggers.end());
            if(pKeys->isProgramEffectivelyMuted()) return;
            pKeys->forEachZone(note, [&](int key, MTIKeys& zone) {
                if(zone.mute.get() || pKeys->isNoteEffectivelyMuted(key)) return;
                if(zone.samplePlay.get() == SamplePlayType::NOTE_OFF)
                {
                    forEachVoice([&](ISynthVoice& v) {
                        auto& voice = static_cast<DrumVoice&>(v);
                        if(voice.isPlayingNote(note) && voice.getZone() == zone.vTree) voice.noteOff();
                    });
                    releaseTriggers.push_back({note, key, velocity, zone.vTree});
                }
                else startZone(key, zone, note, velocity);
            });
        }
        else
        {
            forEachVoice([&](ISynthVoice& v) {
                auto& voice = static_cast<DrumVoice&>(v);
                if(voice.isPlayingNote(note) && voice.getPlayMode() == SamplePlayType::NOTE_ON) voice.noteOff();
            });
            // Move matching releases out before triggering, so a duplicate note
            // off cannot retrigger and project edits cannot retarget a release.
            for(auto it = releaseTriggers.begin(); it != releaseTriggers.end();)
                if(it->note == note)
                {
                    const auto pending = *it; it = releaseTriggers.erase(it);
                    if(auto* zone = pKeys->getKeysClip(pending.key))
                        if(zone->vTree == pending.zone) startZone(pending.key, *zone, note, velocity > 0 ? velocity : pending.velocity);
                }
                else ++it;
        }
    }

    void KeygroupProgram::noteOn(int note, int velocity)
    {
        if(pKeys && CommonUtils::isValidMidiNote(note))
            pKeys->forEachZone(note, [&](int key, MTIKeys& zone) { startZone(key, zone, note, velocity); });
    }

    void KeygroupProgram::startZone(int key, MTIKeys& zone, int note, int velocity)
    {
        if(pKeys->isProgramEffectivelyMuted() || zone.mute.get() || pKeys->isNoteEffectivelyMuted(key)) return;
        std::array<int, MAX_LAYERS> valid{}; int count = 0;
        for(int layer = 0; layer < MAX_LAYERS; ++layer)
            if(auto* slice = zone.getSourceSlice(layer))
                if(slice->source && slice->source->isValid()) valid[count++] = layer;
        if(count == 0) return;
        int layer = -1;
        switch(zone.layerPlay.get())
        {
            case LayerPlayType::CYCLE: layer = valid[nextLayer[(size_t)key]++ % (unsigned)count]; break;
            case LayerPlayType::RANDOM: layer = valid[AudIO::utils::random<int>(0, count - 1)]; break;
            case LayerPlayType::VELOCITY:
                for(int i = 0; i < count; ++i)
                    if(velocity >= zone.layers[valid[i]]->velStart.get() && velocity <= zone.layers[valid[i]]->velEnd.get()) { layer = valid[i]; break; }
                break;
        }
        if(layer < 0) return;
        if(zone.muteGroup.get() > 0)
            forEachVoice([&](ISynthVoice& v) {
                auto& voice = static_cast<DrumVoice&>(v);
                if(!voice.isFree() && (int)voice.getZone().getProperty(IDs::muteGroup, 0) == zone.muteGroup.get()) voice.noteOff();
            });
        // The per-zone limit counts voices from this zone, not other overlapping
        // zones playing the same MIDI pitch.
        ISynthVoice* oldest = nullptr; int playing = 0;
        forEachVoice([&](ISynthVoice& v) {
            auto& voice = static_cast<DrumVoice&>(v);
            if(!voice.isFree() && voice.getZone() == zone.vTree && voice.getNote() == note)
            { ++playing; if(!oldest || voice.getStartOrder() < oldest->getStartOrder()) oldest = &voice; }
        });
        auto* selected = playing >= zone.polyPhony.get() && oldest ? oldest->changeToSettingupAndReturn() : getFreeVoice(note, POLYPHONY::MAX);
        if(auto* voice = dynamic_cast<DrumVoice*>(selected))
        {
            voice->setSlice(zone.getSourceSlice(layer)); voice->attachToAndReset(zone.vTree);
            const int cents = zone.cents.load() + (zone.keyTracking.get() ? 100 * (note - zone.keyNote.get()) : 0);
            voice->setKeygroupParameters(std::exp2(cents / 1200.0), zone.gL.load(), zone.gR.load());
            voice->noteOn(note, velocity); addVoiceForPlayback(voice);
        }
    }
};
