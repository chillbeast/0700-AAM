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

#include "mt_audio_midi_program.h"

namespace mt
{
    MTMidiProgram::MTMidiProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgram( VT, UM )
        , MTIMidiProgramData( VT, UM ) {};

    std::optional<juce::MidiMessage> MTMidiProgram::controllerMessage(const juce::Identifier& property) const
    {
        const auto text = property.toString();
        if(!text.startsWith("CC")) return {};
        const auto number = text.substring(2);
        if(number.isEmpty() || !number.containsOnly("0123456789")) return {};
        const int cc = number.getIntValue();
        if(!juce::isPositiveAndBelow(cc, 128) || text != "CC" + juce::String(cc)) return {};
        return juce::MidiMessage::controllerEvent(getMidiChannel(), cc, juce::jlimit(0, 127, (int)vTree.getProperty(property, 0)));
    }

    void MTMidiProgram::valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& Property )
    {
        auto port = getPort();

        if( vTree == treeWhosePropertyHasChanged )
        {
            if(auto msg = controllerMessage(Property))
            { // CC messages...
                ae.sendMidiMessage(port, *msg);

                reloadMidiMeter( 1.f );
                outMidiMeter.reload(MIDI::SCALER * msg->getControllerValue());
            }
            else if( Property.toString().containsIgnoreCase( "BankLSB" ) || Property.toString().containsIgnoreCase( "BankMSB" ) )
            {
                juce::MidiMessage CCmsg;
                const auto        ch = getMidiChannel();

                // Send the Bank Select MSB message
                CCmsg = juce::MidiMessage::controllerEvent(
                    ch,
                    0, // MSB controller number
                    getBankMSB() );
                ae.sendMidiMessage( port, CCmsg );

                // Send the Bank Select LSB message
                CCmsg = juce::MidiMessage::controllerEvent(
                    ch,
                    32, // LSB controller number
                    getBankLSB() );
                ae.sendMidiMessage( port, CCmsg );

                // Send the program change message
                CCmsg = juce::MidiMessage::programChange( ch, getProgramChannel() );
                ae.sendMidiMessage( port, CCmsg );

                reloadMidiMeter( 1.f );
                outMidiMeter.reload( ( MIDI::SCALER * getProgramChannel() ) );
            }
            else if( Property.toString().containsIgnoreCase( "ProgCH" ) )
            {
                juce::MidiMessage CCmsg;

                // Send the program change message
                CCmsg = juce::MidiMessage::programChange( getMidiChannel(), getProgramChannel() );
                ae.sendMidiMessage( port, CCmsg );

                reloadMidiMeter( 1.f );
                outMidiMeter.reload( ( MIDI::SCALER * getProgramChannel() ) );
            }
        }
        else
            MTIProgram::valueTreePropertyChanged( treeWhosePropertyHasChanged, Property );
    }

    // this is not a reentrant function, calling from multiple threads simultaneously may result in incorrect results.
    void MTMidiProgram::noteProcess( juce::Array<juce::MidiMessage>& Msgs )
    {
        if( isProgramEffectivelyMuted() )
            return;

        auto progGain = MIDI::SCALER * level.get();
        for( auto& it : Msgs )
        {
            auto noteLevel = MIDI::SCALER * ( it.isNoteOnOrOff() ? it.getVelocity() : 127 );
            // In Meter...
            reloadMidiMeter( noteLevel );

            if( !isNoteEffectivelyMuted( it.getNoteNumber() ) )
            {
                auto gainLevel = progGain * noteLevel;

                if( it.isNoteOnOrOff() )
                    it.setVelocity( gainLevel );

                // Out Meter.
                outMidiMeter.reload( gainLevel );

                it.setChannel( midiCh.get() );
                ae.sendMidiMessage( midiPort.get(), it );
                //DBG( "Midi: " << midiPort.get() << "  " << it.getDescription() );
            }
        }
    }

    void MTMidiProgram::processMidiMeter()
    {
        MTIProgram::processMidiMeter();
        outMidiMeter.process();
    }

} // namespace MT
