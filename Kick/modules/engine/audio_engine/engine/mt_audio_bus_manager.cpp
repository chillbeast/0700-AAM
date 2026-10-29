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
    static juce::CriticalSection _bmLock;

    juce::ValueTree _getBus_( juce::ValueTree& VT, const juce::String& BusName )
    {
        for( auto it : VT )
        {
            if( BusName.equalsIgnoreCase( it.getProperty( IDs::name ) ) )
                return it;
        }
        juce::ValueTree vt = createVTree( IDs::BUS,
                                          IDs::name, BusName );
        VT.addChild( vt, -1, nullptr );
        return vt;
    }

    ///
    /// MTIBus
    ///
    MTIBus::MTIBus( MTBus* Bus )
        : state( juce::ValueTree{ IDs::BUS } )
    {
        setBus( Bus );
        state.addListener( this );

        mute.onValueChange.connect( [&]() { if( bus ) bus->setMute( mute.get() ); } );
        bypass.onValueChange.connect( [&]() { if( bus ) bus->setBypass( bypass.get() ); } );
        solo.onValueChange.connect([&]() { if(bus) bus->setSolo(solo.get()); });
        limiter.onValueChange.connect([&]() { if(bus) bus->setLimiter(limiter.get()); });
        gain.onValueChange.connect( [&]() { if( bus ) bus->setOutputGain( gain.get() ); } );
        pan.onValueChange.connect( [&]() { if( bus ) bus->setOutputPan( pan.get() ); } );
        routedTo.onValueChange.connect( [&]() {
            if( bus && !bus->isInOrOut() )
            {
                auto id = AudIO::DawBusManager::getBusId( routedTo.get().toStdString() );
                if( !bus->isConnectedTo( id ) )
                    bus->connectOutputTo( id );
            }
        } );
    }

    MTIBus::~MTIBus()
    {
        state.removeListener( this );
    }

    void MTIBus::setBus( MTBus* Bus )
    {
        bus = Bus;
    }

    void MTIBus::setup( juce::ValueTree VT, juce::UndoManager* UM )
    {
        jassert( VT.hasType( IDs::BUS ) );

        // Stop proceessing until we setup.
        if( bus ) bus->setBypass( true );
        if( bus ) name = bus->getName();

        state = VT;

        mute.referTo( state, IDs::mute, UM, false );
        solo.referTo(state, IDs::solo, UM, false);
        limiter.referTo(state, "limiter", UM, false);
        gain.referTo( state, IDs::gain, UM, GAIN::DEFAULT );
        pan.referTo( state, IDs::pan, UM, PAN::DEFAULT );
        routedTo.referTo( state, IDs::route, UM, AudIO::DawBusManager::getDefaultStereoOutBusName() );

        auto insertsVT = state.getOrCreateChildWithName( IDs::INSERTS, nullptr );
        MTIInsertsInterface::setup( insertsVT, UM, bus ? bus->getInsertsInterface() : nullptr );

        // Do this at the last to restore processing.
        bypass.referTo( state, IDs::bypass, UM, false );
        if(bus) { bus->setSolo(solo.get()); bus->setLimiter(limiter.get()); }
    }

    void MTIBus::valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property )
    {
        /*
        if( bus && ( treeWhosePropertyHasChanged == state ) )
        {
            if( property == IDs::mute )
                bus->setMute( mute.get() );
            else if( property == IDs::bypass )
                bus->setBypass( bypass.get() );
            else if( property == IDs::gain )
                bus->setOutputGain( gain.get() );
            else if( property == IDs::pan )
                bus->setOutputPan( pan.get() );
            else if( property == IDs::route )
            {
                if( !bus->isInOrOut() )
                {
                    auto id = AudIO::DawBusManager::getBusId( routedTo.get().toStdString() );
                    if( !bus->isConnectedTo( id ) )
                        bus->connectOutputTo( id );
                }
            }
        }
        */
    }

    void MTIBus::enableInMetering( bool Enable )
    {
        if( bus ) bus->enableInputMetering( Enable );
    }

    void MTIBus::enableOutMetering( bool Enable )
    {
        if( bus ) bus->enableOutputMetering( Enable );
    }

    const MTMeterData& MTIBus::readInMeter() const { return bus ? bus->readInMeter() : AudIO::_emptyMeterData; }
    const MTMeterData& MTIBus::readOutMeter() const { return bus ? bus->readOutMeter() : AudIO::_emptyMeterData; }

    ///
    /// MTBusManager
    ///
    MTBusManager::MTBusManager( AudIO::DawBusManager& BusMgr )
        : busMgr( BusMgr )
    {
        auto busAdder = [&]( AudIO::DawBus* Bus ) {
            const auto& stdName = Bus->getName();
            auto        busName = juce::String( stdName );
            busMap[busName]     = std::make_unique<MTIBus>( Bus );
        };

        busMgr.forEachOutputBus( busAdder );
        busMgr.forEachReturnsBus( busAdder );
        busMgr.forEachSubmixBus( busAdder );
        busMgr.forEachAuxBus( busAdder );
    }

    void MTBusManager::reset()
    {
        busMgr.reset();
    }

    void MTBusManager::setup( juce::ValueTree VT, juce::UndoManager* UM )
    {
        reset();
        juce::ScopedLock l( _bmLock );
        um    = UM;
        state = std::move( VT );

        if( !state.isValid() )
            state = juce::ValueTree( IDs::BUSES );

        // Initialize Buses....
        for( auto& it : busMap )
        {
            if( it.first.length() )
            {
                auto busVT = _getBus_( state, it.first );
                it.second->setup( busVT, UM );
            }
        }
    }

    int MTBusManager::getNumIns() const { return busMgr.getNumIns(); }
    int MTBusManager::getNumMonoIns() const { return busMgr.getNumMonoIns(); }
    int MTBusManager::getNumStereoIns() const { return busMgr.getNumStereoIns(); }
    int MTBusManager::getNumOuts() const { return busMgr.getNumOuts(); }
    int MTBusManager::getNumMonoOuts() const { return busMgr.getNumMonoOuts(); }
    int MTBusManager::getNumStereoOuts() const { return busMgr.getNumStereoOuts(); }
    int MTBusManager::getNumSends() const { return busMgr.getNumSends(); }
    int MTBusManager::getNumReturns() const { return busMgr.getNumReturns(); }
    int MTBusManager::getNumSubmixes() const { return busMgr.getNumSubmixes(); }
    int MTBusManager::getNumAuxes() const { return busMgr.getNumAuxes(); }
    int MTBusManager::getNumObjects() const { return busMgr.getNumObjects(); }
    int MTBusManager::getNumOthers() const { return busMgr.getNumOthers(); }

    uint32_t     MTBusManager::getBusId( const juce::String& Name ) const { return busMgr.getBusId( Name.toStdString() ); };
    juce::String MTBusManager::getBusName( uint32_t Id ) const { return busMgr.getBusName( Id ); };

    uint32_t MTBusManager::getSendsBusID( uint32_t Position ) const { return busMgr.getSendReturnID( Position ); }
    uint32_t MTBusManager::getReturnsBusID( uint32_t Position ) const { return busMgr.getSendReturnID( Position ); };
    uint32_t MTBusManager::getSubmixBusID( uint32_t Position ) const { return busMgr.getSubmixID( Position ); };
    uint32_t MTBusManager::getOutBusID( uint32_t Position ) const { return busMgr.getOutID( Position ); };
    uint32_t MTBusManager::getInBusID( uint32_t Position ) const { return busMgr.getInID( Position ); };

    MTIBus* MTBusManager::getBus( const juce::String& Name ) const
    {
        auto it = busMap.find( Name );
        return it != busMap.end() ? it->second.get() : nullptr;
    }
    MTIBus* MTBusManager::getBus( const uint32_t ID ) const { return getBus( getBusName( ID ) ); }

    MTIBus* MTBusManager::getSends( uint32_t Position ) const { return getBus( getSendsBusID( Position ) ); }
    MTIBus* MTBusManager::getReturns( uint32_t Position ) const { return getBus( getReturnsBusID( Position ) ); }
    MTIBus* MTBusManager::getSubmix( uint32_t Position ) const { return getBus( getSubmixBusID( Position ) ); }
    MTIBus* MTBusManager::getOutput( uint32_t Position ) const { return getBus( getOutBusID( Position ) ); }
    MTIBus* MTBusManager::getInput( uint32_t Position ) const { return getBus( getInBusID( Position ) ); }

    juce::String MTBusManager::getSendsBusName( uint32_t Position ) const { return getBusName( getSendsBusID( Position ) ); }
    juce::String MTBusManager::getReturnsBusName( uint32_t Position ) const { return getBusName( getReturnsBusID( Position ) ); }
    juce::String MTBusManager::getSubmixBusName( uint32_t Position ) const { return getBusName( getSubmixBusID( Position ) ); }
    juce::String MTBusManager::getOutBusName( uint32_t Position ) const { return getBusName( getOutBusID( Position ) ); }
    juce::String MTBusManager::getInBusName( uint32_t Position ) const { return getBusName( getInBusID( Position ) ); }

    // Input BUS...
    const std::vector<std::string>& MTBusManager::getInputBusNames() { return busMgr.getInNames(); }
    // Output BUS...
    const std::vector<std::string>& MTBusManager::getOutputBusNames() { return busMgr.getOutNames(); }

    const std::vector<std::string>& MTBusManager::getSendBusNames() { return busMgr.getSendsNames(); }

    const std::vector<std::string>& MTBusManager::getReturnBusNames() { return busMgr.getReturnsNames(); }

    const std::vector<std::string>& MTBusManager::getSubmixBusNames() { return busMgr.getSubmixNames(); }

    float MTBusManager::getBusGain( const juce::String& BusName ) { return busMgr.getBusOutGain( BusName.toStdString() ); }

    float MTBusManager::getBusPan( const juce::String& BusName ) { return busMgr.getBusOutPan( BusName.toStdString() ); }

    bool MTBusManager::isBusMuted( const juce::String& BusName ) { return busMgr.isBusMuted( BusName.toStdString() ); }

    bool MTBusManager::isBusBypassed( const juce::String& BusName ) { return busMgr.isBusBypassed( BusName.toStdString() ); }

    bool MTBusManager::isBusSolo( const juce::String& BusName ) { return busMgr.isBusSolo( BusName.toStdString() ); }

    void MTBusManager::setBusMute( const juce::String& BusName, bool Value )
    {
        juce::ScopedLock l( _bmLock );
        auto             v = _getBus_( state, BusName );
        v.setProperty( IDs::mute, Value, um );
        busMgr.setBusOutMute( BusName.toStdString(), Value );
    }

    void MTBusManager::setBusBypass( const juce::String& BusName, bool Value )
    {
        juce::ScopedLock l( _bmLock );
        auto             v = _getBus_( state, BusName );
        v.setProperty( IDs::bypass, Value, um );
        busMgr.setBusOutBypass( BusName.toStdString(), Value );
    }

    void MTBusManager::setBusSolo( const juce::String& BusName, bool Value )
    {
        if(auto* bus = getBus(BusName)) bus->solo = Value;
    }

    void MTBusManager::setBusGain( const juce::String& BusName, float Value )
    {
        juce::ScopedLock l( _bmLock );
        auto             v = _getBus_( state, BusName );
        v.setProperty( IDs::gain, Value, um );
        busMgr.setBusOutGain( BusName.toStdString(), Value );
    }

    void MTBusManager::setBusPan( const juce::String& BusName, float Value )
    {
        juce::ScopedLock l( _bmLock );
        auto             v = _getBus_( state, BusName );
        v.setProperty( IDs::pan, Value, um );
        busMgr.setBusOutPan( BusName.toStdString(), Value );
    }

    void MTBusManager::enableInMetering( const juce::String& BusName, bool Enable )
    {
        return busMgr.enableInputMetering( BusName.toStdString(), Enable );
    }

    void MTBusManager::enableOutMetering( const juce::String& BusName, bool Enable )
    {
        return busMgr.enableOutputMetering( BusName.toStdString(), Enable );
    }

    const MTMeterData& MTBusManager::readInMeter( const juce::String& BusName ) const
    {
        return busMgr.readInMeter( BusName.toStdString() );
    }

    const MTMeterData& MTBusManager::readOutMeter( const juce::String& BusName ) const
    {
        return busMgr.readOutMeter( BusName.toStdString() );
    }

}
