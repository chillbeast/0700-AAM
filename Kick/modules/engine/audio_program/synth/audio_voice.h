/*
 * Copyright (c) 2024 MUSIC Tribe
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
    class AVoice : public ISynthVoice
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<AVoice>;

        explicit AVoice( MTIAtomicProgramMaster& Master, ASoundSlice* Slice = nullptr )
            : ISynthVoice( Master, Slice ) {}

        void setKeygroupParameters(double Ratio, float Left, float Right)
        {
            keyRatio = Ratio; keyLeft = Left; keyRight = Right;
        }

        void process(juce::AudioBuffer<float>& buffer) override
        {
            if(!ISynthVoice::isRunning()) return;
            ASliceRenderer::getNextBlock(tempBuffer, master.pitchRatio.load() * keyRatio);
            gain.applyGain(tempBuffer, buffer.getNumSamples());
            buffer.addFrom(0, 0, tempBuffer, 0, 0, buffer.getNumSamples(), keyLeft);
            buffer.addFrom(1, 0, tempBuffer, 1, 0, buffer.getNumSamples(), keyRight);
            if(hasFinished()) makeFree();
        }

    private:
        double keyRatio{1.0};
        float keyLeft{1.0f}, keyRight{1.0f};
    };

} // namespace mt
