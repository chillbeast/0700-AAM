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

namespace mt
{

    /// @brief
    ISynthObject::ISynthObject( AudIO::Design& D, int Polyphony, AudIO::BusType Type, int Inputs )
        : AudIO::Object( Inputs, 2, 0 )
        , AudIO::DawRouting( D, Type, AudIO::BusWidth::Stereo )
        , AudioDesign( D )
    {
        // Limits check...
        numPolyphony = AudIO::utils::limit<int>( 0, POLYPHONY::MAX, Polyphony );

        AudIO::DawRouting::setID( getId() );
    }

    void ISynthObject::addVoices( std::vector<std::unique_ptr<ISynthVoice>>& Voices )
    {
        voices.clear();
        for( auto& it : Voices )
        {
            voices.push_back( std::move( it ) );
        }
    }

    ISynthObject::~ISynthObject()
    {
        AudioDesign.removeMeter( getId() );
        setBypass( true );

        // Free Voices...
        voices.clear();
    }

    void ISynthObject::setProgram( MTIProgram* Program )
    {
        soloConnection.disconnect();
        program = Program;
        master.attachTo( Program ? Program->getState() : juce::ValueTree{ IDs::PROGRAM } );
        soloMuted = Program && Program->isMutedDueToSolo();
        if(Program) soloConnection = Program->soloStateChanged.connect([this](bool muted) { soloMuted = muted; });
    }

    bool ISynthObject::isPlaying( int Note )
    {
        return nullptr != isPlayback( Note );
    }

    int ISynthObject::getNumPlaying( int Note ) const
    {
        int num = 0;
        if( CommonUtils::isValidMidiNote( Note ) )
            for( auto& it : voices )
                if( it->isPlayingNote( Note ) )
                    ++num;

        return num;
    }

    ISynthVoice* ISynthObject::isPlayback( int Note )
    {
        if( CommonUtils::isValidMidiNote( Note ) )
            for( auto& it : voices )
                if( it->isPlayingNote( Note ) )
                    return it.get();

        return nullptr;
    }

    ISynthVoice* ISynthObject::getFreeVoice( int Note, int Polyphony )
    {
        if(!CommonUtils::isValidMidiNote(Note) || Polyphony < 1) return nullptr;
        ISynthVoice* oldest = nullptr;
        const bool sameNoteOnly = getNumPlaying(Note) >= Polyphony;
        int active = 0; for(const auto& voice : voices) if(!voice->isFree()) ++active;
        const bool programFull = active >= juce::jlimit(1, numPolyphony, master.poly());
        for(auto& voice : voices)
        {
            if(!sameNoteOnly && !programFull && voice->isFree()) return voice->changeToSettingupAndReturn();
            if(!voice->isFree() && (!sameNoteOnly || voice->getNote() == Note))
                if(!oldest || (voice->isStopping() && !oldest->isStopping()) ||
                    (voice->isStopping() == oldest->isStopping() && voice->getStartOrder() < oldest->getStartOrder())) oldest = voice.get();
        }
        if(oldest) { oldest->stop(); return oldest->changeToSettingupAndReturn(); }

        return nullptr;
    }

    void ISynthObject::freeVoice( ISynthVoice* Voice )
    {
        if( Voice )
            Voice->makeFree();
    }

    void ISynthObject::playVoice( ISynthVoice* Voice )
    {
        if( Voice )
            Voice->makeRunning();
    }

    void ISynthObject::addVoiceForPlayback( ISynthVoice* Voice )
    {
        if(Voice) Voice->setStartOrder(++nextVoiceOrder);
        playVoice( Voice );
    }

    void ISynthObject::noteMute( int Note, bool Mute )
    {
        if( CommonUtils::isValidMidiNote( Note ) )
            for( auto& it : voices )
                it->noteMute( Note, Mute );
    }

    void ISynthObject::noteReset( int Note )
    {
        if( CommonUtils::isValidMidiNote( Note ) )
            for( auto& it : voices )
                it->noteReset( Note );
    }

    void ISynthObject::noteReset( std::vector<int>& Notes )
    {
        for( auto it : Notes )
            noteReset( it );
    }

    void ISynthObject::noteLoop( int Note, bool Loop )
    {
        //for( const auto& it : playingVoices )
        //    if( it->p.midiNote == Note )
        //        it->setLoop( Loop );
    }

    void ISynthObject::noteOff( int Note )
    {
        if( CommonUtils::isValidMidiNote( Note ) )
            for( auto& it : voices )
                it->noteOff( Note );
    }

    void ISynthObject::noteOff( std::vector<int>& Notes )
    {
        for( auto it : Notes )
            noteOff( it );
    }

    void ISynthObject::noteOffAll()
    {
        for( auto& it : voices )
            it->noteOff();
    }

    void ISynthObject::prepareToPlay( uint32_t samplerate, uint32_t blocksize )
    {
        AudIO::Object::prepareToPlay( samplerate, blocksize );
        AudIO::DawInserts::prepareToPlay( samplerate, blocksize );
        initialize( samplerate, blocksize );
    }

    void ISynthObject::teardown()
    {
        AudIO::Object::teardown();
        shutdown();
    }

    void ISynthObject::process( AudIO::Buffer<float>& buffer )
    {
        processInput(buffer); // Input/output graph buffers may alias; read input first.
        juce::AudioBuffer<float> bufferToFill( (float* const*)buffer.getOutputs(), buffer.getNumOutputChannels(), buffer.getBlockSize() );
        bufferToFill.clear();

        if( isBypassed() )
            return;

        // Do pre-processing...
        preProcess( bufferToFill );

        for( auto& it : voices )
            it->process( bufferToFill );

        // Do processing if required...
        doProcess( bufferToFill );

        // Perform inserts...
        AudIO::DawInserts::process( buffer );

        // Do post-processing...
        postProcess( bufferToFill );

        if( outMeterEnabled_.load() )
            meter.process( buffer );
    }

    void ISynthObject::preProcess( juce::AudioBuffer<float>& buffer ){};

    void ISynthObject::doProcess( juce::AudioBuffer<float>& buffer ){};

    void ISynthObject::postProcess( juce::AudioBuffer<float>& buffer )
    {
        if(soloMuted.load()) { buffer.clear(); return; }
        auto ns = buffer.getNumSamples();
        auto nc = buffer.getNumChannels();

        // Apply Master Gain and Pan.
        if( !juce::approximatelyEqual( master.gainL.load(), float( 1 ) ) )
            buffer.applyGain( 0, 0, ns, master.gainL.load() );
        if( 1 < nc && !juce::approximatelyEqual( master.gainR.load(), float( 1 ) ) )
            buffer.applyGain( 1, 0, ns, master.gainR.load() );
    };

    // Metering...
    MTMeterData& ISynthObject::readOutMeter()
    {
        return meter.getMeter( outMeterData );
    }
};
