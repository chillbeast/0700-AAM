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

#include "iSynthObject.h"

namespace mt
{
    class AudioPreview
        : public AudIO::Object
        , public AudIO::DawRouting
    {
    public:
        AudioPreview( AudIO::Design& D, const juce::String& Name = "AudioPreview" );
        ~AudioPreview();

        bool isBypassed() const { return bypass.load(); }
        bool isMuted() const { return player.isMuted(); }
        bool isLooping() const { return player.isLooping(); }
        bool isRunning() const { return player.isRunning(); };

        void setLooping( bool Loop ) { player.setLooping( Loop ); }
        void setMute( bool Mute ) { const juce::ScopedLock lock(renderLock); player.setMute( Mute ); }
        void setBypass( bool Bypass ) { bypass.store( Bypass ); }

        void previewOn( const juce::String& Source );
        void previewOff();

        // Overall program control API's
        double getGain() { const juce::ScopedLock lock(renderLock); return gain.getTargetValue(); };
        double getPan() { const juce::ScopedLock lock(renderLock); return pan.getTargetValue(); };

        bool         isOutMeterEnabled() { return outMeterEnabled.load(); }
        void         enableOutputMetering( bool Enable ) { outMeterEnabled = Enable; };
        MTMeterData& getMeter();

        void setGain( double Gain, bool Force = false );
        void setPan( double Pan, bool Force = false );
        void setGainAndPan( double Gain, double Pan, bool Force = false );

    private:
        void prepareToPlay( uint32_t samplerate, uint32_t blocksize );
        void teardown();
        void process( AudIO::Buffer<float>& buffer );

    private:
        // File loading and UI edits own the lock; the audio callback only tries it.
        juce::CriticalSection renderLock;
        std::atomic<bool> outMeterEnabled{ false };
        std::atomic<bool> bypass{ true };

        AudIO::MeterData  meterData;
        AudIO::LevelMeter meter;

        juce::SmoothedValue<float> gain{ 1.0 };
        juce::SmoothedValue<float> pan{ 0.0 };

        ASoundPlayer player;
    };

};
