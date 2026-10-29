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

namespace mt
{

    MTTempoSettings::MTTempoSettings( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        vTree = VT;
        um    = UM;

        num.referTo( vTree, IDs::num, UM, defaultNumerator );
        length.referTo( vTree, IDs::length, UM, -1 );
        tempo.referTo( vTree, IDs::bpm, UM, defaultTempo );
    }

    int MTTempoSettings::getDenominator() const { return vTree.getProperty( IDs::den, magic_enum::enum_integer<MTTimeBase>( defaultTimeBase ) ); }
    int MTTempoSettings::getQuantization() const { return vTree.getProperty( IDs::quantize, magic_enum::enum_integer<MTTimeBase>( defaultQuantize ) ); }

    MTTimeBase  MTTempoSettings::getTimeBase() const { return magic_enum::enum_cast<MTTimeBase>( getDenominator() ).value(); }
    MTTimeBase  MTTempoSettings::getQuantizationLevel() const { return magic_enum::enum_cast<MTTimeBase>( getQuantization() ).value(); }
    MTBarNBeats MTTempoSettings::getLengthBnB() const { return MTBarNBeats{ getLength() }; }

    unsigned int MTTempoSettings::toTicks( double TimeInSeconds ) const
    {
        return AudIO::secondsToTicks( TimeInSeconds, getBPM(), getDenominator(), defaultPPQN );
    }

    double MTTempoSettings::toSeconds( unsigned int TimeInTicks ) const
    {
        return AudIO::ticksToSeconds( TimeInTicks, getBPM(), getDenominator(), defaultPPQN );
    }

    MTBarNBeats MTTempoSettings::toBnB( unsigned int TimeInTicks ) const
    {
        return MTBarNBeats::fromTicks( TimeInTicks, getNumerator(), getDenominator(), defaultPPQN );
    }

    MTBarNBeats MTTempoSettings::toBnB( double TimeInSeconds ) const
    {
        return MTBarNBeats::fromSeconds( TimeInSeconds, getBPM(), getNumerator(), getDenominator(), defaultPPQN );
    }

    void MTTempoSettings::setBPM( const float BPM, bool internalSource )
    {
        if( !internalSource ) tempo = BPM;
    }

    void MTTempoSettings::setLength( const int Bar, bool internalSource )
    {
        if( !internalSource ) length = Bar;
    }

    void MTTempoSettings::setNumerator( const int Numerator, bool internalSource )
    {
        if( !internalSource ) num = Numerator;
    }

    void MTTempoSettings::setTimebase( MTTimeBase Timebase, bool internalSource )
    {
        if( !internalSource ) vTree.setProperty( IDs::den, magic_enum::enum_integer<MTTimeBase>( Timebase ), um );
    }

    void MTTempoSettings::setQuantization( MTTimeBase Quantization, bool internalSource )
    {
        if( !internalSource ) vTree.setProperty( IDs::quantize, magic_enum::enum_integer<MTTimeBase>( Quantization ), um );
    }

    void MTTempoSettings::setLengthBnB( const MTBarNBeats& BnB, bool internalSource )
    {
        setLength( BnB.bar, internalSource );
    }

    int MTTempoSettings::getTicksPerBeat() const
    {
        return AudIO::ticksPerBeat( getDenominator(), defaultPPQN );
    }

    int MTTempoSettings::getTicksPerBar() const
    {
        return AudIO::ticksPerBar( getNumerator(), getDenominator(), defaultPPQN );
    }

    int MTTempoSettings::getTicksPerTimebase( MTTimeBase TimeBase ) const
    {
        return AudIO::ticksPerBeat( magic_enum::enum_integer<MTTimeBase>( TimeBase ), defaultPPQN );
    }

    int MTTempoSettings::getTicksPerQuantization() const
    {
        return getQuantizationLevel() == MTTimeBase::_1 ? getTicksPerBar() : AudIO::ticksPerBeat(getQuantization(), defaultPPQN);
    }

    int MTTempoSettings::getLengthInTicks() const
    {
        return ( 0 > getLength() ) ? -1 : getLength() * getTicksPerBar();
    }

    double MTTempoSettings::getSecondsPerBeat() const
    {
        return AudIO::secondsPerBeat( getBPM(), getDenominator() );
    }

    double MTTempoSettings::getSecondsPerBar() const
    {
        return AudIO::secondsPerBar( getBPM(), getNumerator(), getDenominator() );
    }

    double MTTempoSettings::getSecondsPerTick() const
    {
        auto beatSeconds = getSecondsPerBeat();
        return ( beatSeconds / getTicksPerBeat() );
    }

    double MTTempoSettings::getLengthInSeconds() const
    {
        return getLength() * getSecondsPerBar();
    }

    ///
    /// Class MTTempoMgr
    ///

    juce::ValueTree& MTTempoMgr::getTempoMgrState() noexcept { return vTree; }
    juce::ValueTree  MTTempoMgr::getTempoState( int Index ) noexcept { return vTree.getChild( Index ); }

    MTTempoMgr::MTTempoMgr()
        : vTree( juce::ValueTree{ IDs::TEMPOS } )
        , defTempo( juce::ValueTree{ IDs::TEMPO }, nullptr )
    {}

    void MTTempoMgr::setup( const juce::ValueTree& VT, juce::UndoManager* UM )
    {
        vTree = VT;
        um    = UM;
        // Ensure atleast one tempo settings is available in transportManager.
        vTree.getOrCreateChildWithName( IDs::TEMPO, nullptr );
        // Reserve memory for 4 tempos.
        tempos.reserve( 4 );
        // Update tempos.
        updateTempos();
    }

    void MTTempoMgr::updateTempos()
    {
        tempos.clear();
        for( const auto& it : vTree )
        {
            if( it.hasType( IDs::TEMPO ) )
                tempos.emplace_back( std::make_unique<MTTempoSettings>( it, um ) );
        }
    }

    int              MTTempoMgr::getNumTempoSettings() const { return tempos.size(); }
    MTTempoSettings& MTTempoMgr::getTempoSettings( int Index ) { return ( Index < (int)tempos.size() ) ? *tempos[Index].get() : defTempo; }

    MTTempoSettings& MTTempoMgr::getTempoSettingsAtBar( int Bar )
    {
        auto numTempo = getNumTempoSettings();
        if( 1 == numTempo )
        {
            return getTempoSettings( 0 );
        }
        else
        {
            auto accBarLen = 0;
            for( int i = 0; i < numTempo; ++i )
            {
                auto& tempo    = getTempoSettings( i );
                auto  tempoLen = tempo.getLength();

                if( 0 > tempoLen )
                    return tempo; // This is the last tempo setting

                accBarLen += tempoLen;
                if( Bar < accBarLen )
                    return tempo; // Requested BAR is within this tempo setting
            }
        }
        // Should not happen at all...
        return defTempo;
    }

    MTTempoSettings& MTTempoMgr::getTempoSettingsAtTick( unsigned int Ticks )
    {
        auto numTempo = getNumTempoSettings();
        if( 1 == numTempo )
        {
            return getTempoSettings( 0 );
        }
        else
        {
            unsigned int accTicks = 0;
            for( int i = 0; i < numTempo; ++i )
            {
                auto& tempo    = getTempoSettings( i );
                auto  tempoLen = tempo.getLength();

                if( 0 > tempoLen )
                    return tempo; // This is the last tempo setting

                auto tempoTicks = tempo.getLength() * tempo.getTicksPerBar();
                accTicks += tempoTicks;

                if( Ticks < accTicks )
                    return tempo; // Requested ticks is within this tempo setting
            }
        }
        // Should not happen at all...
        return defTempo;
    }

    void MTTempoMgr::addTransport( MTTempoSettings& TempoSetting, int Index )
    {
        vTree.addChild( TempoSetting.getTempoState(), Index, um );
        updateTempos();
    }

    void MTTempoMgr::removeTransport( MTTempoSettings& TempoSetting )
    {
        vTree.removeChild( TempoSetting.getTempoState(), um );
        updateTempos();
    }

    void MTTempoMgr::removeTransport( int Index )
    {
        vTree.removeChild( Index, um );
        updateTempos();
    }

    void MTTempoMgr::setLength( int Length, bool internalSource, int Index )
    {
        getTempoSettings( Index ).setLength( Length, internalSource );
    }

    void MTTempoMgr::setBPM( float Tempo, bool internalSource, int Index )
    {
        getTempoSettings( Index ).setBPM( Tempo, internalSource );
    }

    void MTTempoMgr::setNumerator( int Numerator, bool internalSource, int Index )
    {
        getTempoSettings( Index ).setNumerator( Numerator, internalSource );
    }

    void MTTempoMgr::setQuantization( MTTimeBase Quantization, bool internalSource, int Index )
    {
        getTempoSettings( Index ).setQuantization( Quantization, internalSource );
    }

    void MTTempoMgr::setTimeBase( MTTimeBase TimeBase, bool internalSource, int Index )
    {
        getTempoSettings( Index ).setTimebase( TimeBase, internalSource );
    }

    int        MTTempoMgr::getLength( int Index ) { return getTempoSettings( Index ).getLength(); }
    int        MTTempoMgr::getBPM( int Index ) { return getTempoSettings( Index ).getBPM(); }
    int        MTTempoMgr::getNumerator( int Index ) { return getTempoSettings( Index ).getNumerator(); }
    int        MTTempoMgr::getDenominator( int Index ) { return getTempoSettings( Index ).getDenominator(); }
    int        MTTempoMgr::getQuantization( int Index ) { return getTempoSettings( Index ).getQuantization(); }
    MTTimeBase MTTempoMgr::getTimeBase( int Index ) { return getTempoSettings( Index ).getTimeBase(); }

    int MTTempoMgr::getBeatsPerBar( int Bar ) { return getTempoSettingsAtBar( Bar ).getNumerator(); }
    int MTTempoMgr::getQuantizationPerBar(int bar)
    {
        const auto& tempo = getTempoSettingsAtBar(bar);
        const int ticks = tempo.getTicksPerBar(), grid = juce::jmax(1, tempo.getTicksPerQuantization());
        return juce::jmax(1, (ticks + grid - 1) / grid);
    }

    int MTTempoMgr::getTicksPerBeat( int Bar ) { return getTempoSettingsAtBar( Bar ).getTicksPerBeat(); }
    int MTTempoMgr::getTicksPerBar( int Bar ) { return getTempoSettingsAtBar( Bar ).getTicksPerBar(); }
    int MTTempoMgr::getTicksPerQuantization( int Bar ) { return getTempoSettingsAtBar( Bar ).getTicksPerQuantization(); }
    int MTTempoMgr::getTicksPerTimebase( int Bar, MTTimeBase Timebase ) { return getTempoSettingsAtBar( Bar ).getTicksPerTimebase( Timebase ); };

    int MTTempoMgr::getBarAtTick( int Ticks )
    {
        int accTicks = 0;
        int bar      = -1;

        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoLen    = tempo.getLength();
            auto ticksPerBar = tempo.getTicksPerBar();

            auto remBars = ( 0 > tempoLen ) ? BAR::MAX : tempo.getLength();
            while( 0 < remBars )
            {
                --remBars;
                ++bar;
                accTicks += ticksPerBar;
                if( accTicks > Ticks )
                {
                    return false;
                }
            }
            return true;
        } );
        return bar;
    }

    int MTTempoMgr::getBaseTicksAtTempoSettingOfBar( int Bar )
    {
        auto         remBarLen = Bar;
        unsigned int ticks     = 0;
        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoBarLen = tempo.getLength();
            auto tempoTicks  = tempo.getTicksPerBar();

            // isLast = (Last Tempo Setting) OR (Valid Tempo AND remaining length is less than tempo length )
            auto isLast = ( ( 0 > tempoBarLen ) || ( ( tempoBarLen > 0 ) && ( remBarLen < tempoBarLen ) ) );
            if( !isLast )
            {
                ticks += ( tempoTicks * tempoBarLen );
                remBarLen -= tempoBarLen;
            }
            return isLast ? false : true;
        } );
        return ticks;
    }

    int MTTempoMgr::getBaseBarAtTempoSetting( int Ticks )
    {
        int accTicks = 0;
        int bar      = 0;

        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoLen = tempo.getLength();

            if( 0 > tempoLen )
                return false; // This is last tempo setting available.

            accTicks += tempo.getLengthInTicks();
            if( accTicks > Ticks )
                return false; // We will exceed ticks at the end of this tempo setting.

            bar += tempo.getLength();
            return true;
        } );
        return bar;
    }

    int MTTempoMgr::getBaseTicksAtTempoSetting( int Ticks )
    {
        unsigned int accTicks  = 0;
        int          baseTicks = 0;

        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoLen = tempo.getLength();

            if( 0 > tempoLen )
                return false; // This is last tempo setting available.

            auto tempoTicks = tempo.getLengthInTicks();
            if( ( baseTicks + tempoTicks ) > Ticks )
                return false; // We will exceed ticks at the end of this tempo setting.

            baseTicks += tempoTicks;
            return true;
        } );
        return baseTicks;
    }

    double MTTempoMgr::getSecondsPerBeat( int Bar ) { return getTempoSettingsAtBar( Bar ).getSecondsPerBeat(); }
    double MTTempoMgr::getSecondsPerBar( int Bar ) { return getTempoSettingsAtBar( Bar ).getSecondsPerBar(); }

    void MTTempoMgr::forEachTempo( std::function<bool( MTTempoSettings& )> Func )
    {
        if( !Func )
            return;

        auto numTempo = getNumTempoSettings();
        for( int i = 0; i < numTempo; ++i )
        {
            auto& tempo = getTempoSettings( i );
            if( false == Func( tempo ) )
                break;
        }
    }

    unsigned int MTTempoMgr::getTicksUpto( unsigned int Bar, unsigned int Beat, unsigned int Ticks )
    {
        auto         remBarLen = (int)Bar;
        unsigned int ticks     = 0;
        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoBarLen = tempo.getLength();
            auto tempoTicks  = tempo.getTicksPerBar();

            // isLast = (Last Tempo Setting) OR (Valid Tempo AND remaining length is less than tempo length )
            auto isLast = ( ( 0 > tempoBarLen ) || ( ( tempoBarLen > 0 ) && ( remBarLen < tempoBarLen ) ) );

            ticks += ( tempoTicks * ( isLast ? remBarLen : tempoBarLen ) );
            if( isLast )
            {
                ticks += Beat * tempo.getTicksPerBeat();
                ticks += Ticks;
            }
            else
            {
                remBarLen -= tempoBarLen;
            }
            return isLast ? false : true;
        } );
        return ticks;
    }

    unsigned int MTTempoMgr::getTicksUptoSeconds( double TimeInSeconds )
    {
        unsigned int ticks      = 0;
        double       remSeconds = TimeInSeconds;

        auto numTempo = getNumTempoSettings();
        for( int i = 0; i < numTempo; ++i )
        {
            auto& tempo        = getTempoSettings( i );
            auto  tempoSeconds = tempo.getLengthInSeconds();

            auto isLast = ( i == ( numTempo - 1 ) );
            if( ( isLast ) || ( !isLast && ( remSeconds <= tempoSeconds ) ) )
            {
                ticks += tempo.toTicks( remSeconds );
                break;
            }
            else
            {
                ticks += tempo.toTicks( tempoSeconds );
                remSeconds -= tempoSeconds;
            }
        }
        return ticks;
    }

    double MTTempoMgr::getSecondsUpto( unsigned int Bar, unsigned int Beat, unsigned int Ticks )
    {
        auto   remBarLen = (int)Bar;
        double time      = 0;
        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoBarLen  = tempo.getLength();
            auto tempoSeconds = tempo.getSecondsPerBar();

            // isLast = (Last Tempo Setting) OR (Valid Tempo AND remaining length is less than tempo length )
            auto isLast = ( ( 0 > tempoBarLen ) || ( ( tempoBarLen > 0 ) && ( remBarLen < tempoBarLen ) ) );

            time += ( tempoSeconds * ( isLast ? remBarLen : tempoBarLen ) );
            if( isLast )
            {
                time += Beat * tempo.getSecondsPerBeat();
                time += Ticks * tempo.getSecondsPerTick();
            }
            else
            {
                remBarLen -= tempoBarLen;
            }
            return isLast ? false : true;
        } );
        return time;
    }

    double MTTempoMgr::getSecondsUptoTicks( unsigned int Ticks )
    {
        unsigned int remTicks = Ticks;
        double       time     = 0;

        auto numTempo = getNumTempoSettings();
        for( int i = 0; i < numTempo; ++i )
        {
            auto& tempo      = getTempoSettings( i );
            auto  tempoTicks = (unsigned int)tempo.getLengthInTicks();

            auto isLast = ( i == ( numTempo - 1 ) );
            if( ( isLast ) || ( !isLast && ( remTicks < tempoTicks ) ) )
            {
                time += tempo.toSeconds( remTicks );
                break;
            }
            else
            {
                time += tempo.toSeconds( tempoTicks );
                remTicks -= tempoTicks;
            }
        }
        return time;
    }

    MTBarNBeats MTTempoMgr::toBnB( unsigned int TimeInTicks )
    {
        auto        remTicks = TimeInTicks;
        MTBarNBeats bnb{ 0, 0, 0 };

        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoBarLen = tempo.getLength();
            auto tempoTicks  = (unsigned int)tempo.getLengthInTicks();

            // isLast = (Last Tempo Setting) OR (Valid Tempo AND remaining length is less than tempo length )
            auto isLast = ( ( 0 > tempoBarLen ) || ( ( tempoBarLen > 0 ) && ( remTicks < tempoTicks ) ) );

            if( isLast )
            {
                auto tempoBnB = tempo.toBnB( remTicks );

                bnb.tick = tempoBnB.tick;
                bnb.beat = tempoBnB.beat;
                bnb.bar += tempoBnB.bar;
            }
            else
            {
                bnb.bar += tempoBarLen;
                remTicks -= tempoTicks;
            }
            return isLast ? false : true;
        } );

        return bnb;
    }

    MTBarNBeats MTTempoMgr::toBnB( double TimeInSeconds )
    {
        auto        remSeconds = TimeInSeconds;
        MTBarNBeats bnb{ 0, 0, 0 };

        forEachTempo( [&]( MTTempoSettings& tempo ) -> bool {
            auto tempoBarLen  = tempo.getLength();
            auto tempoSeconds = tempo.getLengthInSeconds();

            // isLast = (Last Tempo Setting) OR (Valid Tempo AND remaining length is less than tempo length )
            auto isLast = ( ( 0 > tempoBarLen ) || ( ( tempoBarLen > 0 ) && ( remSeconds < tempoSeconds ) ) );

            if( isLast )
            {
                auto tempoBnB = tempo.toBnB( remSeconds );

                bnb.tick = tempoBnB.tick;
                bnb.beat = tempoBnB.beat;
                bnb.bar += tempoBnB.bar;
            }
            else
            {
                bnb.bar += tempoBarLen;
                remSeconds -= tempoSeconds;
            }
            return isLast ? false : true;
        } );

        return bnb;
    }

    MTBarNBeats MTTempoMgr::toBnBFromNormalized( double Normalized, double MaxLengthInSeconds )
    {
        auto timeInSeconds = Normalized * MaxLengthInSeconds;
        return toBnB( timeInSeconds );
    }

    unsigned int MTTempoMgr::toTicks( MTTimeBase Time, unsigned int TempoIndex )
    {
        return getTempoSettings( TempoIndex ).getTicksPerTimebase( Time );
    }

    unsigned int MTTempoMgr::toTicks( const MTBarNBeats& BnB )
    {
        return getTicksUpto( BnB.bar, BnB.beat, BnB.tick );
    }

    unsigned int MTTempoMgr::toTicksFromNormalized( double Normalized, double MaxLengthInSeconds )
    {
        auto timeInSeconds = Normalized * MaxLengthInSeconds;
        return getTicksUptoSeconds( timeInSeconds );
    }

    double MTTempoMgr::toSeconds( const MTBarNBeats& BnB )
    {
        return getSecondsUpto( BnB.bar, BnB.beat, BnB.tick );
    }

    double MTTempoMgr::toSeconds( unsigned int Ticks )
    {
        return getSecondsUptoTicks( Ticks );
    }

} // namespace MT
