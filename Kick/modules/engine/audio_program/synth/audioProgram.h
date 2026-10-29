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
#include "audio_timeline.h"

namespace mt
{
    class MTAudioProgram;

    class AudioProgram : public ISynthObject
    {
    public:
        AudioProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PClip );
        ~AudioProgram();

        void setProgram( MTIProgram* PClip ) override;
        void noteOn( int Note, int Velocity ) override;
        void processInput(AudIO::Buffer<float>&) override;
        void preProcess(juce::AudioBuffer<float>&) override;
        void initialize(uint32_t rate, uint32_t block) override;
        AudioTimeline timeline;
        std::atomic<bool> monitor{false}, inputConnected{false};
        const MTMeterData& readInputMeter() { return inputConnected.load() ? inputMeter.getMeterIn(inputMeterData) : AudIO::_dummyMeter; }

    private:
        MTAudioProgram*                           pAudio{ nullptr };
        juce::AudioBuffer<float> inputBuffer;
        AudIO::LevelMeter inputMeter;
        MTMeterData inputMeterData;
    };

};
