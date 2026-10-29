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

#include "drum_voice.h"

namespace mt
{
    void DrumVoice::attachToAndReset(const juce::ValueTree& tree, juce::UndoManager*)
    {
        zone = tree;
        pLfo.attachTo(tree.getChildWithName(IDs::LFO));
        pEnv.attachTo(tree);
        playMode = (SamplePlayType)(int)tree.getProperty(IDs::samplePlay, (int)SamplePlayType::ONE_SHOT);
    }

    void DrumVoice::configureEnvelope(ADSR& envelope, const MTIAtomicADSR& p)
    {
        const auto type = (ADSRType)juce::jlimit(0, 4, p.adsrType());
        // MIDI 127 represents the unpitched source duration. Release remains
        // independent of pitch, so transposing a held note does not change it.
        const double duration = juce::jmax(0.01, getLength()) * sampleRate / 127.0;
        const bool hold = type == Type_AHD || type == Type_AHDS || type == Type_AHDSR;
        const bool sustain = type == Type_ADSR || type == Type_AHDS || type == Type_AHDSR;
        const bool release = type == Type_ADSR || type == Type_AHDSR;
        envelope.setParameters(p.a() * duration, hold ? p.h() * duration : 0,
            p.d() * duration, release ? p.r() * duration : 0, sustain ? p.s() / 127.0 : 0);
    }

    void DrumVoice::updateEnvelopes()
    {
        configureEnvelope(ampEnv, pEnv.amp);
        configureEnvelope(filtEnv, pEnv.filter);
        configureEnvelope(pitchEnv, pEnv.pitch);
    }

    void DrumVoice::noteOn(int note, int velocity)
    {
        filter.setSampleRate(sampleRate); filter.reset();
        lfo.setSampleRate(sampleRate);
        lfo.setType((LFOType)juce::jlimit(0, 6, pLfo.type()));
        lfo.setFrequency(juce::jlimit(0.1f, 200.f, pLfo.freq()), true); lfo.reset();
        updateEnvelopes(); ampEnv.gate(1); filtEnv.gate(1); pitchEnv.gate(1);
        releasing = false;
        ISynthVoice::noteOn(note, velocity);
    }

    void DrumVoice::noteOff()
    {
        if(!ISynthVoice::isRunning()) return;
        if(pEnv.amp.isEnabled() && pEnv.amp.r() > 0 &&
            (pEnv.amp.adsrType() == Type_ADSR || pEnv.amp.adsrType() == Type_AHDSR))
        {
            if(releasing) return;
            releasing = true; state = State::STOPPING;
            ampEnv.gate(0); filtEnv.gate(0); pitchEnv.gate(0);
        }
        else ISynthVoice::noteOff();
    }

    void DrumVoice::process(juce::AudioBuffer<float>& buffer)
    {
        if(!ISynthVoice::isRunning()) return;
        updateEnvelopes();
        const bool ampOn = pEnv.amp.isEnabled(), filterEnvOn = pEnv.filter.isEnabled(), pitchOn = pEnv.pitch.isEnabled();
        const bool lfoOn = pLfo.isEnabled();
        lfo.setEnabled(lfoOn);
        lfo.setType((LFOType)juce::jlimit(0, 6, pLfo.type()));
        lfo.setFrequency(juce::jlimit(0.1f, 200.f, pLfo.freq()));
        const float ampDepth = juce::jlimit(0, 127, pLfo.ampMod()) / 127.f;
        const float pitchDepth = juce::jlimit(0, 127, pLfo.pitchMod()) / 127.f;
        const float filterDepth = juce::jlimit(0, 127, pLfo.filterMod()) / 127.f;
        const float panDepth = juce::jlimit(-1.f, 1.f, pLfo.panMod());
        const int filterType = pEnv.filter.type();
        if(filterType >= 0) filter.setFilterType((FilterType)juce::jlimit(0, 26, filterType));
        filter.setResonance(pEnv.filter.resonance());
        const auto masterRatio = master.pitchRatio.load();
        auto* left = buffer.getWritePointer(0); auto* right = buffer.getWritePointer(1);
        for(int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wave = lfoOn ? lfo() : 0.f;
            const float amp = ampOn ? ampEnv() : 1.f;
            const float ampAmount = ampOn ? juce::jlimit(0, 127, pEnv.amp.depth()) / 127.f : 1.f;
            const float fEnv = filterEnvOn ? filtEnv() : 0.f;
            const float pEnvValue = pitchOn ? pitchEnv() : 0.f;
            if((releasing && (!ampOn || !ampEnv.isActive())) ||
                (ampOn && amp == 0 && (pEnv.amp.adsrType() == Type_AD || pEnv.amp.adsrType() == Type_AHD)))
            { ASliceRenderer::stop(); makeFree(); break; }
            const double semitones = pEnvValue * pEnv.pitch.depth() * (24.0 / 127.0) + wave * pitchDepth * 2.0;
            auto& frame = getNext(masterRatio * keyRatio * std::exp2(semitones / 12.0));
            if(filterType >= 0)
            {
                const double cutoff = pEnv.filter.cutoff() * std::exp2(fEnv * pEnv.filter.depth() * (4.0 / 127.0) + wave * filterDepth * 2.0);
                filter.setCutoff(Filter::freqRange.convertTo0to1((float)juce::jlimit(20.0, 20000.0, cutoff)));
                filter.process(frame);
            }
            const float tremolo = lfoOn ? 1.f - ampDepth * (0.5f + wave * 0.5f) : 1.f;
            panner.setParameters(lfoOn ? wave * panDepth : 0.f, gain.getNextValue() * amp * ampAmount * tremolo);
            // Program gain/pan are applied once by ISynthObject::postProcess.
            left[i] += frame.getSample(0, 0) * keyLeft * panner.getLeft();
            right[i] += frame.getSample(1, 0) * keyRight * panner.getRight();
            if(hasFinished()) { makeFree(); break; }
        }
    }
}
