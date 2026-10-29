#pragma once

#include "DawRouting.h"
#include "DawInserts.h"

#define ENABLE_STRING_HELPER_FUNCTIONS ( 1 )

#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
    #include <string>
#endif

namespace AudIO
{

    constexpr int          MAX_INSERTS{ 4 };
    extern const MeterData _dummyMeter;

    struct BusStatus
    {
        bool  isMuted{ false };
        bool  isBypassed{ false };
        bool  isSolo{ false };
        float Gain{ 0.f };
        float Pan{ 0.f };
    };

    class DawBus
        : public DawRouting
        , public DawInserts
    {
    public:
        DawBus( Design& Design_, const std::string& Name, BusType Type = BusType::Out, BusWidth Width = BusWidth::Stereo );
        ~DawBus() = default;

        // Callbacks...
        std::function<void( bool OldState, bool NewState )> onSoloChanged;

        void reset();

        // Name.
        void               setName( const std::string& Name );
        const std::string& getName();

        // Id.
        uint32_t getId() const { return DawRouting::id_; };

        // Control Parameters
        bool isMuted() const;
        bool isEffectivelyMuted() const { return _isMuted || _isSoloMuted; }
        bool isBypassed() const;

        void setMute( bool NewState );
        void setBypass( bool NewState );

        std::pair<float, float> getBusLevels() const;

        float getInputGain( uint32_t InPos ) const;
        void  setInputGain( uint32_t InPos, float Gain ) const;

        float getOutputGain() const;
        void  setOutputGain( float Gain );

        float getOutputPan() const;
        void  setOutputPan( float Pan );
        void setLimiter(bool Enabled);

        BusStatus   getStatus() const;
        DawInserts* getInsertsInterface() { return dynamic_cast<AudIO::DawInserts*>( this ); }

        void       enableInputMetering( bool Enable );
        void       enableOutputMetering( bool Enable );
        MeterData& readOutMeter();
        MeterData& readInMeter();

        // To facilitate solo...
        void setSolo( bool NewState );
        bool isSolo() const { return _isSolo; };

    private:
        void setSoloInternal( bool NewState );
        bool isMutedDueToSolo() const;
        void setMutedDueToSolo( bool NewState );
        void setEffectiveMute();

    private:
        bool _isMuted{ false };
        bool _isSolo{ false };
        bool _isSoloMuted{ false };
    };

    class DawDevice;
    class DawBusManager
    {
    public:
        DawBusManager( Design& Design_, int NumMonoIns, int NumMonoOuts, int NumSendReturns, int NumSubmix );
        ~DawBusManager();

        Design& getDesign() { return design_; };
        void    reset() const;

        //
        // Default Bus.
        //
        uint32_t getDefaultStereoInputDevice() const;
        uint32_t getDefaultStereoOutputDevice() const;
        uint32_t getStereoInputDevice( uint32_t Position ) const;
        uint32_t getStereoOutputDevice( uint32_t Position ) const;

        static uint32_t inline getDefaultStereoInputID() { return DawRouting::getDefaultStereoInID(); }
        static uint32_t inline getDefaultStereoOutputID() { return DawRouting::getDefaultStereoOutID(); }
        static uint32_t inline getStereoInputID( uint32_t Position ) { return DawRouting::getStereoInID( Position ); }
        static uint32_t inline getStereoOutputID( uint32_t Position ) { return DawRouting::getStereoOutID( Position ); }

        static uint32_t inline getDefaultMonoInputID() { return DawRouting::getDefaultMonoInID(); }
        static uint32_t inline getDefaultMonoOutputID() { return DawRouting::getDefaultMonoOutID(); }
        static uint32_t inline getMonoInputID( uint32_t Position ) { return DawRouting::getMonoInID( Position ); }
        static uint32_t inline getMonoOutputID( uint32_t Position ) { return DawRouting::getMonoOutID( Position ); }

        static uint32_t inline getSendReturnID( uint32_t Position ) { return DawRouting::getSendReturnID( Position ); }
        static uint32_t inline getSubmixID( uint32_t Position ) { return DawRouting::getSubmixID( Position ); }
        static uint32_t inline getAuxID( uint32_t Position ) { return DawRouting::getAuxID( Position ); }
        static uint32_t inline getOutID( uint32_t Position ) { return DawRouting::getOutID( Position ); }
        static uint32_t inline getInID( uint32_t Position ) { return DawRouting::getInID( Position ); }

        static int inline getNumIns() { return DawRouting::getNumIns(); }
        static int inline getNumMonoIns() { return DawRouting::getNumMonoIns(); }
        static int inline getNumStereoIns() { return DawRouting::getNumStereoIns(); }
        static int inline getNumOuts() { return DawRouting::getNumOuts(); }
        static int inline getNumMonoOuts() { return DawRouting::getNumMonoOuts(); }
        static int inline getNumStereoOuts() { return DawRouting::getNumStereoOuts(); }
        static int inline getNumSends() { return DawRouting::getNumSends(); }
        static int inline getNumReturns() { return DawRouting::getNumReturns(); }
        static int inline getNumSubmixes() { return DawRouting::getNumSubmixes(); }
        static int inline getNumAuxes() { return DawRouting::getNumAuxes(); }
        static int inline getNumObjects() { return DawRouting::getNumObjects(); }
        static int inline getNumOthers() { return DawRouting::getNumOthers(); }

        static DawBus* getBus( uint32_t Id );
        static DawBus* getSends( uint32_t Position ) { return getBus( DawRouting::getSendReturnID( Position ) ); }
        static DawBus* getReturns( uint32_t Position ) { return getBus( DawRouting::getSendReturnID( Position ) ); }
        static DawBus* getSubmix( uint32_t Position ) { return getBus( DawRouting::getSubmixID( Position ) ); }
        static DawBus* getAux( uint32_t Position ) { return getBus( DawRouting::getAuxID( Position ) ); }
        static DawBus* getOut( uint32_t Position ) { return getBus( DawRouting::getOutID( Position ) ); }
        static DawBus* getIn( uint32_t Position ) { return getBus( DawRouting::getInID( Position ) ); }

        void        forEachBusNames( const std::vector<std::string>& BusNames, const std::function<void( DawBus* )>& Func ) const;
        inline void forEachInputBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( inNames, Func ); }
        inline void forEachOutputBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( outNames, Func ); }
        inline void forEachSendsBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( sendsNames, Func ); }
        inline void forEachReturnsBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( returnsNames, Func ); }
        inline void forEachSubmixBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( submixNames, Func ); }
        inline void forEachAuxBus( const std::function<void( DawBus* )>& Func ) const { forEachBusNames( auxNames, Func ); }

        //
        // String functions...
        //
#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
        float getBusOutGain( const std::string& Bus ) const;
        float getBusOutPan( const std::string& Bus ) const;
        bool  isBusMuted( const std::string& Bus ) const;
        bool  isBusBypassed( const std::string& Bus ) const;
        bool  isBusSolo( const std::string& Bus ) const;

        void setBusOutGain( const std::string& Bus, float Gain ) const;
        void setBusOutPan( const std::string& Bus, float Pan ) const;
        void setBusOutMute( const std::string& Bus, bool Mute ) const;
        void setBusOutBypass( const std::string& Bus, bool Bypass ) const;
        void setBusOutSolo( const std::string& Bus, bool Solo ) const;

        void             enableInputMetering( const std::string& Bus, bool Enable ) const;
        void             enableOutputMetering( const std::string& Bus, bool Enable ) const;
        const MeterData& readInMeter( const std::string& BusName ) const;
        const MeterData& readOutMeter( const std::string& BusName ) const;

        // Static functions.
        static DawBus*     getBus( const std::string& Name );
        static uint32_t    getBusId( const std::string& Name );
        static std::string getBusName( uint32_t Id );

        static std::string getDefaultStereoOutBusName() { return getBusName( getDefaultStereoOutputID() ); };

        static const std::vector<std::string>& getInNames() { return inNames; }
        static const std::vector<std::string>& getOutNames() { return outNames; }
        static const std::vector<std::string>& getSendsNames() { return sendsNames; }
        static const std::vector<std::string>& getReturnsNames() { return returnsNames; }
        static const std::vector<std::string>& getSubmixNames() { return submixNames; }
        static const std::vector<std::string>& getAuxNames() { return auxNames; }

        static bool isAReturnBus( const std::string& BusName ) { return std::find( returnsNames.begin(), returnsNames.end(), BusName ) != returnsNames.end(); }
        static bool isASubmixBus( const std::string& BusName ) { return std::find( submixNames.begin(), submixNames.end(), BusName ) != submixNames.end(); }
        static bool isAAuxBus( const std::string& BusName ) { return std::find( auxNames.begin(), auxNames.end(), BusName ) != auxNames.end(); }
        static bool isASendBus( const std::string& BusName ) { return std::find( sendsNames.begin(), sendsNames.end(), BusName ) != sendsNames.end(); }
#endif
    private:
        Design& design_;

        static std::vector<std::unique_ptr<DawDevice>> inDevices_;
        static std::vector<std::unique_ptr<DawDevice>> outDevices_;
        static std::vector<std::unique_ptr<DawBus>>    buses_;
        static std::map<uint32_t, DawBus*>             busIdMap_;

#if defined( ENABLE_STRING_HELPER_FUNCTIONS ) && ( 0 < ENABLE_STRING_HELPER_FUNCTIONS )
        static std::map<std::string, DawBus*>  busNameMap_;
        static std::map<std::string, uint32_t> busNameIdMap_;

        static std::vector<std::string> inNames;
        static std::vector<std::string> outNames;
        static std::vector<std::string> sendsNames;
        static std::vector<std::string> returnsNames;
        static std::vector<std::string> submixNames;
        static std::vector<std::string> auxNames;
#endif
    };
};
