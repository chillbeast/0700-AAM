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
    // Shared sample voice for drums and keygroup zones. Parameters attach to
    // the selected note tree while the voice is in SETUP, not while rendering.
    class DrumVoice : public ISynthVoice
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<DrumVoice>;
        explicit DrumVoice(MTIAtomicProgramMaster& Master, ASoundSlice* Slice = nullptr) : ISynthVoice(Master, Slice) {}
        void attachToAndReset(const juce::ValueTree& Tree, juce::UndoManager* Undo = nullptr);
        void setKeygroupParameters(double Ratio, float Left, float Right) { keyRatio = Ratio; keyLeft = Left; keyRight = Right; }
        void noteOn(int Note, int Velocity) final;
        void noteOff() final;
        void process(juce::AudioBuffer<float>& Buffer) final;
        const juce::ValueTree& getZone() const { return zone; }
        SamplePlayType getPlayMode() const { return playMode; }
        bool isTailingOff() const { return state.load() == State::STOPPING; }

    private:
        void updateEnvelopes();
        void configureEnvelope(ADSR& Envelope, const MTIAtomicADSR& Parameters);
        juce::ValueTree zone;
        MTIAtomicLFO pLfo;
        MTIAtomicEnvelopes pEnv;
        ADSR ampEnv, filtEnv, pitchEnv;
        Filter filter;
        AudIO::LFO lfo;
        AudIO::PannerFloat panner{0.f, 1.f};
        SamplePlayType playMode{SamplePlayType::ONE_SHOT};
        double keyRatio{1.0};
        float keyLeft{1.f}, keyRight{1.f};
        bool releasing{false};
    };
}
