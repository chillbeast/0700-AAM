
#include "DawBus.h"
#include <algorithm>

namespace AudIO
{
    const MeterData   _dummyMeter{ 2 };
    const std::string returnBusPrefix = "Return-";

    std::vector<std::unique_ptr<DawDevice>> DawBusManager::inDevices_;
    std::vector<std::unique_ptr<DawDevice>> DawBusManager::outDevices_;
    std::vector<std::unique_ptr<DawBus>>    DawBusManager::buses_;
    std::map<uint32_t, DawBus*>             DawBusManager::busIdMap_;

#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
    std::map<std::string, DawBus*>  DawBusManager::busNameMap_;
    std::map<std::string, uint32_t> DawBusManager::busNameIdMap_;
    std::vector<std::string>        DawBusManager::inNames;
    std::vector<std::string>        DawBusManager::outNames;
    std::vector<std::string>        DawBusManager::sendsNames;
    std::vector<std::string>        DawBusManager::returnsNames;
    std::vector<std::string>        DawBusManager::submixNames;
    std::vector<std::string>        DawBusManager::auxNames;
#endif

    class DawDevice
    {
    public:
        DawDevice( Design& Design_, DeviceType Type, const std::string& Name )
            : design_( Design_ ), type_( Type )
        {
            id_ = ( DeviceType::In == type_ ) ? design_.createChain( true ) : design_.createBus();
            design_.setBypass( id_, true );
            design_.setName( id_, Name );
        }

        inline uint32_t getId() const { return id_; }

        void connectInputTo( uint32_t MonoL, uint32_t MonoR, uint32_t Stereo )
        {
            if( DeviceType::In == type_ )
            {
                // Remove existing patches..
                design_.unpatch( id_, inL );
                design_.unpatch( id_, inR );
                design_.unpatch( id_, inLR );
                inL  = 0;
                inR  = 0;
                inLR = 0;
                // Create patch...
                if( design_.patch( id_, MonoL ) )
                    inL = MonoL;
                if( design_.patch( id_, MonoR ) )
                    inR = MonoR;
                if( design_.patch( id_, Stereo ) )
                    inLR = Stereo;
            }
        }

        void connectOutputTo( uint32_t MonoL, uint32_t MonoR, uint32_t Stereo )
        {
            if( DeviceType::Out == type_ )
            {
                // Remove existing connections...
                design_.removeFromBus( id_, outL );
                design_.removeFromBus( id_, outR );
                design_.removeFromBus( id_, outLR );
                outL  = 0;
                outR  = 0;
                outLR = 0;
                // Send inputs to out bus.
                if( design_.sendToBus( id_, MonoL, 1.f ) )
                    outL = MonoL;
                if( design_.sendToBus( id_, MonoR, 1.f ) )
                    outR = MonoR;
                if( design_.sendToBus( id_, Stereo, 1.f ) )
                    outLR = Stereo;
            }
        }

    private:
        Design&          design_;
        const DeviceType type_;
        uint32_t         id_;
        uint32_t         inL{ 0 };
        uint32_t         inR{ 0 };
        uint32_t         inLR{ 0 };
        uint32_t         outL{ 0 };
        uint32_t         outR{ 0 };
        uint32_t         outLR{ 0 };
    };

    //
    // Class: DawBus.
    //
    DawBus::DawBus( Design& Design_, const std::string& Name, BusType Type, BusWidth Width )
        : DawRouting( Design_, Type, Width )
    {
        DawRouting::id_ = ( BusType::In == Type ) ? Design_.createChain( true ) : Design_.createBus();
        setName( Name );
        initAudioRouting();
    }

    void DawBus::reset()
    {
        setBypass( true ); // Temporarily bypass bus to prevent any audio glitch,

        removeAllEffects();
        setMute( false );
        setSolo( false );
        setLimiter(false);
        setOutputGain( 1.f );
        setOutputPan( 0.f );

        DawRouting::resetRouting();
        setBypass( false );
    }

    void DawBus::setName( const std::string& Name )
    {
        design_.setName( id_, Name );
    }

    const std::string& DawBus::getName()
    {
        return design_.getName( id_ );
    }

    // Control Parameters
    bool DawBus::isMuted() const { return _isMuted; }
    bool DawBus::isBypassed() const { return design_.isBypassed( id_ ); }

    void DawBus::setMute( bool NewState )
    {
        _isMuted = NewState;
        setEffectiveMute();
    }
    void DawBus::setBypass( bool NewState ) { design_.setParameter( id_, BusOrChain::kParamBypass, NewState ? 1.f : 0.f ); }

    void DawBus::setSolo( bool NewState )
    {
        auto        busType     = getType();
        const auto& busGroupIDs = DawRouting::getBusIDs( busType );

        if((BusType::SendReturn == busType) || (BusType::Submix == busType) || (BusType::Out == busType))
        {
            // Solo membership is additive within each bus group. Explicit mute
            // remains separate, so clearing solo restores prior mute choices.
            setSoloInternal(NewState);
            bool anySolo = false;
            for(auto bus : busGroupIDs)
                if(auto* p = DawBusManager::getBus(bus)) anySolo |= p->isSolo();
            for( auto& bus : busGroupIDs )
            {
                if( auto p = DawBusManager::getBus( bus ) )
                {
                    auto muteStatus = anySolo && !p->isSolo();
                    p->setMutedDueToSolo( muteStatus );
                }
            }
        }
    }

    void DawBus::setSoloInternal( bool NewState ) { _isSolo = NewState; }
    bool DawBus::isMutedDueToSolo() const { return _isSoloMuted; }
    void DawBus::setMutedDueToSolo( bool NewState )
    {
        _isSoloMuted = NewState;
        setEffectiveMute();
    }

    void DawBus::setEffectiveMute()
    {
        auto mute = isEffectivelyMuted();
        design_.setParameter( id_, BusOrChain::kParamMute, mute ? 1.f : 0.f );
    }

    std::pair<float, float> DawBus::getBusLevels() const { return design_.getLevelAndPan( id_ ); }

    float DawBus::getInputGain( uint32_t InPos ) const { return 0.f; }
    void  DawBus::setInputGain( uint32_t InPos, float Gain ) const {}

    float DawBus::getOutputGain() const { return design_.getParameter( id_, BusOrChain::kParamGain ); }
    void  DawBus::setOutputGain( float Gain ) { design_.setParameter( id_, BusOrChain::kParamGain, Gain ); }

    float DawBus::getOutputPan() const { return design_.getParameter( id_, BusOrChain::kParamPan ); }
    void  DawBus::setOutputPan( float Pan ) { design_.setParameter( id_, BusOrChain::kParamPan, Pan ); }
    void DawBus::setLimiter(bool enabled) { design_.setParameter(id_, BusOrChain::kParamLimiter, enabled ? 1.f : 0.f); }

    // Bus Status...
    BusStatus DawBus::getStatus() const
    {
        BusStatus s;
        s.isMuted    = _isMuted;
        s.isBypassed = design_.isBypassed( id_ );
        s.Gain       = design_.getGain( id_ );
        s.Pan        = design_.getPan( id_ );
        return s;
    }

    void DawBus::enableInputMetering( bool Enable )
    {
        design_.enableInputMetering( id_, Enable );
    }

    void DawBus::enableOutputMetering( bool Enable )
    {
        design_.enableOutputMetering( id_, Enable );
    }

    MeterData& DawBus::readOutMeter()
    {
        return design_.readOutMeter( id_ );
    }

    MeterData& DawBus::readInMeter()
    {
        return design_.readInMeter( id_ );
    }

    //
    // Class: DawBusManager.
    //

    DawBusManager::DawBusManager( Design& Design_, int NumMonoIns, int NumMonoOuts, int NumSendReturns, int NumSubmix )
        : design_( Design_ )
    {
        std::string name;
        std::string nameL;
        std::string nameR;

        int numStereoIns  = ( 1 + NumMonoIns ) >> 1;
        int numStereoOuts = ( 1 + NumMonoOuts ) >> 1;

        // Devices.
        inDevices_.clear();
        outDevices_.clear();
        // Buses.
        buses_.clear();
        busIdMap_.clear();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
        busNameMap_.clear();
        busNameIdMap_.clear();
        inNames.clear();
        outNames.clear();
        sendsNames.clear();
        returnsNames.clear();
        submixNames.clear();
        auxNames.clear();
#endif

        // Create Input ports...
        for( auto i = 0; i < numStereoIns; ++i )
        {
            auto twoX = ( i * 2 );
            name      = "InPort-" + std::to_string( 1 + twoX ) + "," + std::to_string( 2 + twoX );
            inDevices_.emplace_back( std::make_unique<DawDevice>( Design_, DeviceType::In, name ) );
        }

        // Create Output ports...
        for( auto i = 0; i < numStereoIns; ++i )
        {
            auto twoX = ( i * 2 );
            name      = "OutPort-" + std::to_string( 1 + twoX ) + "," + std::to_string( 2 + twoX );
            outDevices_.emplace_back( std::make_unique<DawDevice>( Design_, DeviceType::Out, name ) );
        }

        // Create Input buses...
        for( int i = 0; i < numStereoIns; ++i )
        {
            auto twoX = ( i * 2 );
            name      = "In-" + std::to_string( 1 + twoX ) + "," + std::to_string( ( 2 + twoX ) );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::In, BusWidth::Stereo ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            inNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif

            name = "In-" + std::to_string( 1 + twoX );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::In, BusWidth::Mono_L ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            inNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif

            name = "In-" + std::to_string( 2 + twoX );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::In, BusWidth::Mono_R ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            inNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif
        }

        // Create Output buses...
        for( int i = 0; i < numStereoOuts; ++i )
        {
            auto twoX = ( i * 2 );

            name = "Out-" + std::to_string( 1 + twoX ) + "," + std::to_string( ( 2 + twoX ) );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::Out, BusWidth::Stereo ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            outNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif

            name = "Out-" + std::to_string( 1 + twoX );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::Out, BusWidth::Mono_L ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            outNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif

            name = "Out-" + std::to_string( 2 + twoX );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::Out, BusWidth::Mono_R ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            outNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif
        }

        // Create bus for Sends and Returns...
        for( int i = 0; i < NumSendReturns; ++i )
        {
            name = returnBusPrefix + std::to_string( 1 + i );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::SendReturn, BusWidth::Stereo ) );

            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            returnsNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
            name.replace( 0, returnBusPrefix.length(), "Send-" );
            sendsNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif
        }

        // Create bus for Sub mixes...
        for( int i = 0; i < NumSubmix; ++i )
        {
            name = "Submix-" + std::to_string( 1 + i );
            buses_.emplace_back( std::make_unique<DawBus>( Design_, name, BusType::Submix, BusWidth::Stereo ) );
            busIdMap_[buses_.back()->getId()] = buses_.back().get();
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
            submixNames.push_back( name );
            busNameMap_[name]   = buses_.back().get();
            busNameIdMap_[name] = buses_.back()->getId();
#endif
        }

        // Make connections to Input and Output devices...
        for( int i = 0; i < numStereoIns; ++i )
        {
            auto monoIdx = i * 2;
            inDevices_[i]->connectInputTo( DawRouting::getMonoInID( monoIdx ), DawRouting::getMonoInID( monoIdx + 1 ), DawRouting::getStereoInID( i ) );
        }
        for( int i = 0; i < numStereoOuts; ++i )
        {
            auto monoIdx = i * 2;
            outDevices_[i]->connectOutputTo( DawRouting::getMonoOutID( monoIdx ), DawRouting::getMonoOutID( monoIdx + 1 ), DawRouting::getStereoOutID( i ) );
        }
    }

    DawBusManager::~DawBusManager()
    {
        buses_.clear();
    }

    void DawBusManager::reset() const
    {
        for( const auto& [Id, pBus] : busIdMap_ )
        {
            if( pBus )
                pBus->reset();
        }
    }

    //
    // Default Bus.
    //
    uint32_t DawBusManager::getDefaultStereoInputDevice() const { return getStereoInputDevice( 0 ); }
    uint32_t DawBusManager::getDefaultStereoOutputDevice() const { return getStereoOutputDevice( 0 ); }
    uint32_t DawBusManager::getStereoInputDevice( uint32_t Pos ) const { return ( Pos < inDevices_.size() ) ? inDevices_[Pos]->getId() : 0; }
    uint32_t DawBusManager::getStereoOutputDevice( uint32_t Pos ) const { return ( Pos < outDevices_.size() ) ? outDevices_[Pos]->getId() : 0; }

    DawBus* DawBusManager::getBus( uint32_t Id )
    {
        auto it = busIdMap_.find( Id );
        return ( it != busIdMap_.end() ) ? it->second : nullptr;
    }

    void DawBusManager::forEachBusNames( const std::vector<std::string>& BusNames, const std::function<void( DawBus* )>& Func ) const
    {
        if( Func )
            for( const auto& it : BusNames )
                Func( busNameMap_[it] );
    }

    ///
    /// String helper functions...
    ///
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )

    DawBus* DawBusManager::getBus( const std::string& Name )
    {
        auto it = busNameMap_.find( Name );
        return ( it != busNameMap_.end() ) ? it->second : nullptr;
    }

    uint32_t DawBusManager::getBusId( const std::string& Name )
    {
        auto it = busNameIdMap_.find( Name );
        return ( it != busNameIdMap_.end() ) ? it->second : 0;
    }

    std::string DawBusManager::getBusName( uint32_t Id )
    {
        auto it = busIdMap_.find( Id );
        return ( it != busIdMap_.end() ) ? it->second->getName() : "";
    }

    float DawBusManager::getBusOutGain( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->getOutputGain();
        return 0.f;
    }

    float DawBusManager::getBusOutPan( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->getOutputPan();
        return 0.f;
    }

    bool DawBusManager::isBusMuted( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->isMuted();
        return false;
    }

    bool DawBusManager::isBusBypassed( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->isBypassed();
        return false;
    }

    bool DawBusManager::isBusSolo( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->isSolo();
        return false;
    }

    void DawBusManager::setBusOutGain( const std::string& Bus, float Gain ) const
    {
        if( auto p = getBus( Bus ) )
            p->setOutputGain( Gain );
    }

    void DawBusManager::setBusOutPan( const std::string& Bus, float Pan ) const
    {
        if( auto p = getBus( Bus ) )
            p->setOutputPan( Pan );
    }

    void DawBusManager::setBusOutMute( const std::string& Bus, bool Mute ) const
    {
        if( auto p = getBus( Bus ) )
            p->setMute( Mute );
    }

    void DawBusManager::setBusOutBypass( const std::string& Bus, bool Bypass ) const
    {
        if( auto p = getBus( Bus ) )
            p->setBypass( Bypass );
    }

    void DawBusManager::setBusOutSolo( const std::string& Bus, bool Solo ) const
    {
        if( auto p = getBus( Bus ) )
            p->setSolo( Solo );
    }

    void DawBusManager::enableInputMetering( const std::string& Bus, bool Enable ) const
    {
        if( auto p = getBus( Bus ) )
            p->enableInputMetering( Enable );
    }

    void DawBusManager::enableOutputMetering( const std::string& Bus, bool Enable ) const
    {
        if( auto p = getBus( Bus ) )
            p->enableOutputMetering( Enable );
    }

    const MeterData& DawBusManager::readInMeter( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->readInMeter();
        return _dummyMeter;
    }

    const MeterData& DawBusManager::readOutMeter( const std::string& Bus ) const
    {
        if( auto p = getBus( Bus ) )
            return p->readOutMeter();
        return _dummyMeter;
    }

#endif

};
