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

#include "mt_audio_iprogram.h"
#include <optional>

namespace mt
{
    class MTMidiProgram
        : public MTIProgram
        , public MTIMidiProgramData
    {
    public:
        explicit MTMidiProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        virtual ~MTMidiProgram() = default;

        inline juce::String getPort() const { return midiPort.get(); }
        inline int          getBankLSB() const { return bankLsb.get(); }
        inline int          getBankMSB() const { return bankMsb.get(); }
        inline int          getProgramChannel() const { return programCh.get(); }
        inline int          getMidiChannel() const { return midiCh.get(); }
        std::optional<juce::MidiMessage> controllerMessage(const juce::Identifier& Property) const;

        // IMidiMsgProcessor
        void noteProcess( juce::Array<juce::MidiMessage>& Msgs ) override;

        const MTMeterData& readOutMeter() const override { return outMidiMeter.getMeter(); };
        void               processMidiMeter() override;

    protected:
    private:
        void valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& Property ) override;

    private:
        MTAudioInterface& ae{ getAudioInterface() };
        const MTSelected& aeSel{ ae.getSelected() };

        MTMidiMeter outMidiMeter{ 40 };
    };
} // namespace MT
