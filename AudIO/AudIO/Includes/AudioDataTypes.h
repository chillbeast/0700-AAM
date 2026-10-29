#pragma once

#include <string>
#include <vector>

namespace AudIO
{

#define AUD_IO_MAKE_NON_COPYABLE( XYZ )    \
    XYZ( const XYZ& )            = delete; \
    XYZ& operator=( const XYZ& ) = delete;

    /*
        struct AudioParameterProperties{
            unsigned int id;
            bool isSmooth;
            char* name, unit;
            float min, max, def;
        };
    */

    constexpr uint32_t POS_END = 0xFFFFFFFF;

    enum BusOrChain
    {
        kParamBypass,
        kParamMute,
        kParamGain,
        kParamPan,
        kParamDownMixToLeft,
        kParamDownMixToRight,
        kParamStereo,
        kParamLimiter,
        kParamMax
    };

    struct BusInputMeta
    {
        uint32_t id{ 0 };
        float    gain{ 0.f };

        void clear()
        {
            id   = 0;
            gain = 0.f;
        }
    };

    struct ObjectInfo
    {
        std::string Manufacturer;
        std::string Type;
        std::string Category;
        std::string Name;
    };

    struct ParamInfo
    {
        std::string name;
        std::string unit;
        std::string displayValue;
        float       value;
    };

    template<typename T = float>
    union Gains
    {
        struct
        {
            T l;
            T r;
        };
        T gain[2];
    };

    struct Meter
    {
        float rms{ -100.f };
        float peak{ -100.f };
        void  reset()
        {
            rms  = -100.f;
            peak = -100.f;
        }
    };
    using MeterData = std::vector<Meter>;

    const static std::vector<std::string> effectManufacturers = { "AirWindows" };

    const static std::vector<std::string> effectCategories = { "Ambience", "Amp_Sims", "Bass", "Biquads", "Brightness", "Clipping", "Consoles", "Distortion", "Dithers", "Dynamics", "Effects", "Filter", "Lo-Fi", "Noise", "Reverb", "Saturation", "Stereo", "Subtlety", "Tape", "Tone Color", "Utility", "XYZ Filters" };

    // Extern definitions...
    extern const std::string              _emptyStr;
    extern const std::vector<std::string> _emptyVectorStr;
    extern const std::vector<ParamInfo>   _emptyVectorParamInfo;
    extern const MeterData                _emptyMeterData;

} // Namespace AudIO
