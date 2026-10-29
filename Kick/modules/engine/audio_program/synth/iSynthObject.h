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

#include "iSynthVoice.h"

namespace mt
{
    class MTIProgram;

    class ISynthObject : public AudIO::Object
        , public AudIO::DawRouting
        , public AudIO::DawInserts
        , public IMidiMsgProcessor
    {
    public:
        ISynthObject( AudIO::Design& D, int Polyphony, AudIO::BusType Type = AudIO::BusType::Object, int Inputs = 0 );
        ~ISynthObject();

        // Overall program control API's
        inline bool  isOutMeterEnabled() { return outMeterEnabled_.load(); }
        inline bool  isBypassed() { return bypass_.load(); }
        inline float isMuted() { return master.isMuted(); }

        inline void setBypass( bool Bypass ) { bypass_ = Bypass; }

        // Metering.
        inline void  enableOutputMetering( bool Enable ) { outMeterEnabled_ = true; };
        MTMeterData& readOutMeter();

        // Voices.
        void       addVoices( std::vector<std::unique_ptr<ISynthVoice>>& Voices );
        inline int getPolyphony() const { return numPolyphony; }

        // Virtual functions.
        virtual void noteProcess( juce::Array<juce::MidiMessage>& ){};

        virtual void noteOff( int Note );
        virtual void noteOff( std::vector<int>& Notes );
        virtual void noteOffAll();
        virtual void noteMute( int Note, bool Mute );
        virtual void noteReset( int Note );
        virtual void noteReset( std::vector<int>& Notes );
        virtual void noteLoop( int Note, bool Loop );

        // If you are overriding this, make sure to call this base method.
        virtual void setProgram( MTIProgram* Program );

        // Functions related to processing.
        virtual void initialize( uint32_t samplerate, uint32_t blocksize ){};
        virtual void shutdown(){};
        virtual void preProcess( juce::AudioBuffer<float>& buffer );
        virtual void processInput(AudIO::Buffer<float>&) {}
        virtual void doProcess( juce::AudioBuffer<float>& buffer );
        virtual void postProcess( juce::AudioBuffer<float>& buffer );

        bool isPlaying( int Note );
        int  getNumPlaying( int Note ) const;

        ISynthVoice* isPlayback( int Note );
        ISynthVoice* getFreeVoice( int Note, int Polyphony );
        void         freeVoice( ISynthVoice* Voice );
        void         playVoice( ISynthVoice* Voice );
        void         addVoiceForPlayback( ISynthVoice* Voice );

    private:
        void process( AudIO::Buffer<float>& buffer );
        void prepareToPlay( uint32_t samplerate, uint32_t blocksize );
        void teardown();

    protected:
        MTIAtomicProgramMaster master;
        std::atomic<bool> soloMuted{false};
        sigslot::scoped_connection soloConnection;
        template<typename Visitor> void forEachVoice(Visitor&& Visit) { for(auto& voice : voices) Visit(*voice); }

    private:
        std::atomic<bool>                         bypass_{ true };
        std::atomic<bool>                         outMeterEnabled_{ true };
        std::vector<std::unique_ptr<ISynthVoice>> voices;

        AudIO::MeterData  outMeterData;
        AudIO::LevelMeter meter;

        int numPolyphony;
        uint64_t nextVoiceOrder{0};

        AudIO::Design& AudioDesign;
        MTIProgram*    program{ nullptr };
    };

};
