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

namespace mt
{
    //////////////////////////////////////////////////////////////////
    ///  MTCommonProgram
    //////////////////////////////////////////////////////////////////

    MTCommonProgram::MTCommonProgram( juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgram( VT, UM ) {};

    ///////////////////////////////////////////////////////////////////////////////////////
    // Private methods...
    ///////////////////////////////////////////////////////////////////////////////////////
    void MTCommonProgram::refresh()
    {
        MTIProgram::refresh();
    }

    juce::ValueTree MTCommonProgram::getNoteVT( int Note )
    {
        auto p = getSample( Note );
        return p ? p->vTree : EMPTY::VTState;
    }

    MTICommonDrumKeys* MTCommonProgram::getCommonData( int Note )
    {
        return dynamic_cast<MTICommonDrumKeys*>( getSample( Note ) );
    }

    juce::ValueTree MTCommonProgram::createSample( int Note, const juce::String& Source, int Layer )
    {
        if( auto p = getOrCreate( Note ) )
        {
            if( p->layers[Layer] )
            {
                p->layers[Layer]->setSource( Source, 0 );
                return p->vTree;
            }
        }
        return EMPTY::VTState;
    }

    juce::ValueTree MTCommonProgram::getEnvelopeState(int note, EnvelopeType type)
    {
        if(auto* data = getCommonData(note))
            return data->vTree.getChildWithName(type == AMPLITUDE ? IDs::AMPENV : type == FILTER ? IDs::FILTERENV : IDs::PITCHENV);
        return {};
    }
    void MTCommonProgram::setEnvelopeEnabled(int note, EnvelopeType type, bool enabled) { auto tree = getEnvelopeState(note, type); if(tree.isValid()) tree.setProperty(IDs::isEnabled, enabled, um); }
    bool MTCommonProgram::getEnvelopeEnabled(int note, EnvelopeType type) { return getEnvelopeState(note, type).getProperty(IDs::isEnabled, false); }
    void MTCommonProgram::setEnvelopeRelease(int note, EnvelopeType type, int value) { auto tree = getEnvelopeState(note, type); if(tree.isValid()) tree.setProperty(IDs::release, juce::jlimit(0, 127, value), um); }
    int MTCommonProgram::getEnvelopeRelease(int note, EnvelopeType type) { return getEnvelopeState(note, type).getProperty(IDs::release, 0); }
    void MTCommonProgram::setFilterType(int note, int type) { auto tree = getEnvelopeState(note, FILTER); if(tree.isValid()) tree.setProperty("filterType", juce::jlimit(-1, 26, type), um); }
    int MTCommonProgram::getFilterType(int note) { return getEnvelopeState(note, FILTER).getProperty("filterType", -1); }
    void MTCommonProgram::setFilterCutoff(int note, double hertz) { auto tree = getEnvelopeState(note, FILTER); if(tree.isValid()) tree.setProperty(IDs::freq, juce::jlimit(20.0, 20000.0, hertz), um); }
    double MTCommonProgram::getFilterCutoff(int note) { return getEnvelopeState(note, FILTER).getProperty(IDs::freq, 20000.0); }
    void MTCommonProgram::setFilterResonance(int note, double amount) { auto tree = getEnvelopeState(note, FILTER); if(tree.isValid()) tree.setProperty(IDs::resonance, juce::jlimit(0.0, 97.0, amount), um); }
    double MTCommonProgram::getFilterResonance(int note) { return getEnvelopeState(note, FILTER).getProperty(IDs::resonance, 0.0); }
    void MTCommonProgram::setLFOEnabled(int note, bool enabled) { if(auto* data = getCommonData(note)) data->lfoIsEnabled = enabled; }
    bool MTCommonProgram::getLFOEnabled(int note) { auto* data = getCommonData(note); return data && data->lfoIsEnabled.get(); }

    // LFO
    void MTCommonProgram::setLFOType( int Note, int Type )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoType = Type;
    }

    void MTCommonProgram::setLFOFrequency( int Note, double Value )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoFrequency = Value;
    }

    void MTCommonProgram::setLFOPitch( int Note, int Value )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoPitch = Value;
    }

    void MTCommonProgram::setLFOFilter( int Note, int Value )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoFilter = Value;
    }

    void MTCommonProgram::setLFOLevel( int Note, int Value )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoLevel = Value;
    }

    void MTCommonProgram::setLFOPan( int Note, double Value )
    {
        if( auto p = getCommonData( Note ) )
            p->lfoPan = Value;
    }

    int MTCommonProgram::getLFOType( int Note )
    {
        auto p = getCommonData( Note );
        return p ? p->lfoType.get() : 0;
    }

    double MTCommonProgram::getLFOFrequency( int Note )
    {
        auto p = getCommonData( Note );
        return p ? p->lfoFrequency.get() : 1;
    }

    int MTCommonProgram::getLFOPitch( int Note )
    {
        auto p = getCommonData( Note );
        return p ? p->lfoPitch.get() : 0;
    }

    int MTCommonProgram::getLFOFilter( int Note )
    {
        auto p = getCommonData( Note );
        return p ? p->lfoFilter.get() : 0;
    }

    int MTCommonProgram::getLFOLevel( int Note )
    {
        auto p = getCommonData( Note );
        return p ? static_cast<int>( p->lfoLevel.get() ) : static_cast<double>( 0 );
    }

    double MTCommonProgram::getLFOPan( int Note )
    {
        auto p = getCommonData( Note );
        return p ? static_cast<double>( p->lfoPan.get() ) : static_cast<double>( PAN::DEFAULT );
    }

    void MTCommonProgram::setEnvType( int Note, ADSRType Type, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->amptype = Type; break;
                case EnvelopeType::FILTER: p->filtertype = Type; break;
                case EnvelopeType::PITCH: p->pitchtype = Type; break;
                default: break;
            }
        }
    }

    void MTCommonProgram::setEnvelopeAttack( int Note, EnvelopeType EnvType, int Value )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->ampAttack = Value; break;
                case EnvelopeType::FILTER: p->filterAttack = Value; break;
                case EnvelopeType::PITCH: p->pitchAttack = Value; break;
                default: break;
            }
        }
    }

    void MTCommonProgram::setEnvelopeHold( int Note, EnvelopeType EnvType, int Value )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->ampHold = Value; break;
                case EnvelopeType::FILTER: p->filterHold = Value; break;
                case EnvelopeType::PITCH: p->pitchHold = Value; break;
                default: break;
            }
        }
    }

    void MTCommonProgram::setEnvelopeDecay( int Note, EnvelopeType EnvType, int Value )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->ampDecay = Value; break;
                case EnvelopeType::FILTER: p->filterDecay = Value; break;
                case EnvelopeType::PITCH: p->pitchDecay = Value; break;
                default: break;
            }
        }
    }

    void MTCommonProgram::setEnvelopeSustain( int Note, EnvelopeType EnvType, int Value )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->ampSustain = Value; break;
                case EnvelopeType::FILTER: p->filterSustain = Value; break;
                case EnvelopeType::PITCH: p->pitchSustain = Value; break;
                default: break;
            }
        }
    }

    void MTCommonProgram::setEnvelopeDepth( int Note, EnvelopeType EnvType, int Value )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: p->ampDepth = Value; break;
                case EnvelopeType::FILTER: p->filterDepth = Value; break;
                case EnvelopeType::PITCH: p->pitchDepth = Value; break;
                default: break;
            }
        }
    }

    ADSRType MTCommonProgram::getEnvType( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->amptype.get();
                case EnvelopeType::FILTER: return p->filtertype.get();
                case EnvelopeType::PITCH: return p->pitchtype.get();
                default: break;
            }
        }
        return ADSRType::Type_AD;
    }

    int MTCommonProgram::getEnvelopeAttack( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->ampAttack.get();
                case EnvelopeType::FILTER: return p->filterAttack.get();
                case EnvelopeType::PITCH: return p->pitchAttack.get();
                default: break;
            }
        }
        return 0;
    }

    int MTCommonProgram::getEnvelopeHold( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->ampHold.get();
                case EnvelopeType::FILTER: return p->filterHold.get();
                case EnvelopeType::PITCH: return p->pitchHold.get();
                default: break;
            }
        }
        return 0;
    }

    int MTCommonProgram::getEnvelopeDecay( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->ampDecay.get();
                case EnvelopeType::FILTER: return p->filterDecay.get();
                case EnvelopeType::PITCH: return p->pitchDecay.get();
                default: break;
            }
        }
        return 0;
    }

    int MTCommonProgram::getEnvelopeSustain( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->ampSustain.get();
                case EnvelopeType::FILTER: return p->filterSustain.get();
                case EnvelopeType::PITCH: return p->pitchSustain.get();
                default: break;
            }
        }
        return 0;
    }

    int MTCommonProgram::getEnvelopeDepth( int Note, EnvelopeType EnvType )
    {
        if( auto p = getCommonData( Note ) )
        {
            switch( EnvType )
            {
                case EnvelopeType::AMPLITUDE: return p->ampDepth.get();
                case EnvelopeType::FILTER: return p->filterDepth.get();
                case EnvelopeType::PITCH: return p->pitchDepth.get();
                default: break;
            }
        }
        return 0;
    }
}
