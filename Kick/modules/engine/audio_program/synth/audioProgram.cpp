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

#include "audio_voice.h"

namespace mt
{

    /// @brief
    AudioProgram::AudioProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PClip )
        : ISynthObject( D, 1, AudIO::BusType::Object, 2 )
    {
        setName( Name.toStdString() );
        setProgram( PClip );

    }

    AudioProgram::~AudioProgram()
    {
        setBypass( true );
    }

    void AudioProgram::setProgram( MTIProgram* PAudio )
    {
        // call base class method...
        ISynthObject::setProgram( PAudio );

        noteOffAll();
        pAudio = dynamic_cast<MTAudioProgram*>( PAudio );
        setBypass( ( pAudio == nullptr ) );
    }

    void AudioProgram::noteOn( int Note, int Velocity )
    {
        // Audio regions follow the track timeline, independently of pad MIDI.
    }

    void AudioProgram::initialize(uint32_t rate, uint32_t block)
    {
        inputBuffer.setSize(2, (int)block); inputBuffer.clear();
    }

    void AudioProgram::processInput(AudIO::Buffer<float>& buffer)
    {
        const int count = (int)buffer.getBlockSize();
        const bool connected = inputConnected.load();
        auto* left = connected ? buffer.getInputBuffer(0) : nullptr;
        auto* right = connected ? buffer.getInputBuffer(1) : nullptr;
        timeline.record(left, right, count, getSampleRate());
        inputBuffer.clear();
        if(left && right && count <= inputBuffer.getNumSamples())
        {
            inputMeter.processIn(buffer);
            if(monitor.load())
            {
                inputBuffer.copyFrom(0, 0, left, count);
                inputBuffer.copyFrom(1, 0, right, count);
            }
        }
    }

    void AudioProgram::preProcess(juce::AudioBuffer<float>& output)
    {
        if(output.getNumSamples() <= inputBuffer.getNumSamples())
            for(int channel = 0; channel < juce::jmin(2, output.getNumChannels()); ++channel)
                output.addFrom(channel, 0, inputBuffer, channel, 0, output.getNumSamples());
        timeline.render(output, getSampleRate(), master.pitchRatio.load());
    }

};
