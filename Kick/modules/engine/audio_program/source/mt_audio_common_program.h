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

namespace mt
{

    class MTCommonProgram : public MTIProgram
    {
    public:
        explicit MTCommonProgram( juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        ~MTCommonProgram() = default;

        /////////////
        ///////////// Sample related API's
        /////////////

        virtual juce::ValueTree getNoteVT( int Note );
        MTICommonDrumKeys*      getCommonData( int Note );

        //Envelopes
        //setters
        void setEnvType( int Note, ADSRType Type, EnvelopeType EnvType );
        void setEnvelopeAttack( int Note, EnvelopeType EnvType, int Value );
        void setEnvelopeHold( int Note, EnvelopeType EnvType, int Value );
        void setEnvelopeDecay( int Note, EnvelopeType EnvType, int Value );
        void setEnvelopeSustain( int Note, EnvelopeType EnvType, int Value );
        void setEnvelopeDepth( int Note, EnvelopeType EnvType, int Value );
        juce::ValueTree getEnvelopeState(int Note, EnvelopeType Type);
        void setEnvelopeEnabled(int Note, EnvelopeType Type, bool Enabled);
        bool getEnvelopeEnabled(int Note, EnvelopeType Type);
        void setEnvelopeRelease(int Note, EnvelopeType Type, int Value);
        int getEnvelopeRelease(int Note, EnvelopeType Type);
        void setFilterType(int Note, int Type);
        int getFilterType(int Note);
        void setFilterCutoff(int Note, double Hertz);
        double getFilterCutoff(int Note);
        void setFilterResonance(int Note, double Amount);
        double getFilterResonance(int Note);
        void setLFOEnabled(int Note, bool Enabled);
        bool getLFOEnabled(int Note);
        //getters
        ADSRType getEnvType( int Note, EnvelopeType EnvType );
        int      getEnvelopeAttack( int Note, EnvelopeType EnvType );
        int      getEnvelopeHold( int Note, EnvelopeType EnvType );
        int      getEnvelopeDecay( int Note, EnvelopeType EnvType );
        int      getEnvelopeSustain( int Note, EnvelopeType EnvType );
        int      getEnvelopeDepth( int Note, EnvelopeType EnvType );
        //LFO
        //Setters
        void setLFOType( int, int );
        void setLFOFrequency( int, double );
        void setLFOPitch( int, int );
        void setLFOFilter( int, int );
        void setLFOLevel( int, int );
        void setLFOPan( int, double );
        //gettters
        int    getLFOType( int Note );
        double getLFOFrequency( int Note );
        int    getLFOPitch( int Note );
        int    getLFOFilter( int Note );
        int    getLFOLevel( int Note );
        double getLFOPan( int Note );

        virtual SamplePlayType getNoteSamplePlay( int Note ) override { return SamplePlayType::ONE_SHOT; }

    private:
        void refresh() override;

    protected:
        virtual juce::ValueTree createSample( int Note, const juce::String& Source, int Layer );
    };

} // namespace MT
