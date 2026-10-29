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

#include "mt_audio_timebase_types.h"
#include "mt_audio_inserts_types.h"

///
/// Macro definitions; VariantConverter
///
#define VARIANT_CONVERTER( DATATYPE )                                       \
    namespace juce                                                          \
    {                                                                       \
        template<>                                                          \
        struct VariantConverter<DATATYPE>                                   \
        {                                                                   \
            static DATATYPE fromVar( const var& v )                         \
            {                                                               \
                return magic_enum::enum_cast<DATATYPE>( (int)v ).value();   \
            }                                                               \
            static var toVar( const DATATYPE& c )                           \
            {                                                               \
                return var( (int)magic_enum::enum_integer<DATATYPE>( c ) ); \
            }                                                               \
        };                                                                  \
    }

///
/// Namespace mt
///
namespace mt
{
    static constexpr int MAX_LAYERS{ 4 };
    static constexpr int MAX_SIMUL_ACTIONS{ 4 };

    static constexpr int MAX_SEQUENCE{ 128 };
    static constexpr int MAX_TRACKS{ 128 };

    enum class MTSamplePlayMode
    {
        samplePlayFromStart,
        samplePlayToStart,
        samplePlayToEnd,
        samplePlayFromEnd,
        samplePlayModeLoopForward,
        samplePlayModeLoopBackward,
        samplePlayModeLoopBidirectional,
        samplePlayModeCue
    };

    enum class MTMetronomeStyle
    {
        Style1,
        Style2,
        Style3,
        Style4,
        Style5,
        Style6,
        Style7,
    };

    enum class MTMetronomeMode
    {
        MET_RECORD,
        MET_PLAY,
        MET_RECORD_PLAY
    };

    enum class MTMetronomeRate
    {
        MET_1_16,
        MET_1_8,
        MET_1_4,
        MET_1_2,
        MET_SEQ
    };

    enum class MTProgramType
    {
        Drum     = 0,
        Keygroup = 1,
        Midi     = 2,
        Audio    = 3,
        Clip     = 4,
        Plugin   = 5,
        Cv       = 6,
        Unknown  = 7
    };

    enum class MTProgramCategory
    {
        Midi,
        Audio,
        Unused
    };

    enum class MTSampleRecordMode
    {
        mtSampleRecordModeNone,
        mtSampleRecordModeRecord,
        mtSampleRecordModeOverdub,
        mtSampleRecordModeReplace,
        mtSampleRecordModeReverse
    };

    enum class AssetTypes
    {
        Samples,
        Exports,
        Audio
    };

    enum class LayerPlayType
    {
        CYCLE,
        VELOCITY,
        RANDOM
    };

    enum class SamplePlayType
    {
        ONE_SHOT,
        NOTE_OFF,
        NOTE_ON
    };

    enum class ClipPlayType
    {
        CLIP_TOGGLE,
        CLIP_ONE_SHOT,
    };

    enum PadColorMode
    {
        INVALID = -1,

        OFF,
        OFF_VELOCITY,
        OFF_FULL,

        DIM_OFF, //PadOff,PadOn
        DIM_VELOCITY,
        DIM_FULL,

        OFF_DIM_OFF, //EmptyPad,PadOff,PadOn
        OFF_DIM_VELOCITY,
        OFF_DIM_FULL,

        DISABLE_PAD_COLOUR_MODE
    };

    enum PadModeState
    {
        PAD_DIM_STATE,
        PAD_OFF_STATE,
        EMPTYPAD_STATE
    };

    enum class EmptyPadColorMode
    {
        Off,
        Dim,
        Normal
    };

    enum TransportState
    {
        Idle,
        Playing,
        Paused,
        Stopped,
    };

    enum class RecordState
    {
        NotArmed,
        Armed,
        Recording,
    };

    enum class PerformanceMode
    {
        OFF,
        CHROMATIC,
        NOTES,
        CHORDS,
        PROGRESSION,
        CHROMATIC_CHORDS
    };
}

///
/// Namespace juce
///
VARIANT_CONVERTER( mt::ADSRType );
VARIANT_CONVERTER( mt::MTMetronomeStyle );
VARIANT_CONVERTER( mt::MTMetronomeRate );
VARIANT_CONVERTER( mt::MTMetronomeMode );
VARIANT_CONVERTER( mt::MTProgramType );
VARIANT_CONVERTER( mt::MTProgramCategory );
VARIANT_CONVERTER( mt::AssetTypes );
VARIANT_CONVERTER( mt::LayerPlayType );
VARIANT_CONVERTER( mt::SamplePlayType );
VARIANT_CONVERTER( mt::ClipPlayType );
VARIANT_CONVERTER( mt::PadColorMode );
VARIANT_CONVERTER( mt::PadModeState );
VARIANT_CONVERTER( mt::EmptyPadColorMode );
VARIANT_CONVERTER( mt::TransportState );
VARIANT_CONVERTER( mt::RecordState );
VARIANT_CONVERTER( mt::PerformanceMode );

///
/// Namespace mt
///
namespace mt
{
    static const char* toString( MTProgramType T ) noexcept
    {
        switch( T )
        {
            case MTProgramType::Drum: return "Drum";
            case MTProgramType::Keygroup: return "Keygroup";
            case MTProgramType::Midi: return "Midi";
            case MTProgramType::Audio: return "Audio";
            case MTProgramType::Clip: return "Clip";
            case MTProgramType::Cv: return "CV";
            case MTProgramType::Plugin: return "Plugin";
            default: return "Unknown";
        }
    }

    static MTProgramType fromString( const juce::String& S )
    {
        if( S.equalsIgnoreCase( "Drum" ) ) return MTProgramType::Drum;
        if( S.equalsIgnoreCase( "Keygroup" ) ) return MTProgramType::Keygroup;
        if( S.equalsIgnoreCase( "Midi" ) ) return MTProgramType::Midi;
        if( S.equalsIgnoreCase( "Audio" ) ) return MTProgramType::Audio;
        if( S.equalsIgnoreCase( "Clip" ) ) return MTProgramType::Clip;
        if( S.equalsIgnoreCase( "CV" ) ) return MTProgramType::Cv;
        if( S.equalsIgnoreCase( "Plugin" ) ) return MTProgramType::Plugin;
        // Fallback, default to it.
        return MTProgramType::Unknown;
    }

    struct MTSampleRegion
    {
        MTSampleRegion() = default;
        MTSampleRegion( juce::int64 s, juce::int64 e )
            : start( s )
            , end( e )
        {
        }
        juce::int64 getEnd() const { return end; }
        juce::int64 getStart() const { return start; }
        bool        isValid() const { return ( start >= 0 && end >= 0 ); }

        juce::int64 start = -1;
        juce::int64 end   = -1;
    };

    struct NotesInRange
    {
        int noteStart{ 0 };
        int noteEnd{ 127 };
        int startTime{ -1 };
        int endTime{ -1 };

        explicit NotesInRange( int StartNote = 0, int EndNote = 127, int StartTime = -1, int EndTime = -1 )
            : noteStart( StartNote ), noteEnd( EndNote ), startTime( StartTime ), endTime( EndTime ){};
    };

    struct SequenceCopyBars
    {
        NotesInRange noteRange;
        int          startBar;
        int          endBar;
        int          numBars;
        int          sourceSequence;
        int          destSequence;
        int          atPositionBar;
        bool         isMerge;
    };

    struct MTSimultaneousAction
    {
        std::array<CachedValue<int>, MAX_SIMUL_ACTIONS> ac;

        void                               setActions( const std::vector<int>& Simul );
        std::array<int, MAX_SIMUL_ACTIONS> getActions();

        MTSimultaneousAction( juce::ValueTree VT, juce::UndoManager* UM );
    };

    struct MTSoundRandomizationData
    {
        CachedValue<MidiConstrainer> rOffset;
        CachedValue<MidiConstrainer> rPitch;
        CachedValue<MidiConstrainer> rLevel;
        CachedValue<MidiConstrainer> rPan;

        MTSoundRandomizationData( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    class ASoundSlice; // Forward declare.
    struct MTISource
    {
        ASoundSlice*              src{ nullptr }; /// Do not set this directly.
        CachedValue<juce::String> source;
        CachedValue<int>          slice;

        MTISource( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTILayer
        : public MTISource
        , public MTSoundRandomizationData
    {
        CachedValue<bool>            isLooping;
        CachedValue<MidiConstrainer> velStart;
        CachedValue<MidiConstrainer> velEnd;

        CachedValue<int> offset;
        CachedValue<int> tailStart;
        CachedValue<int> tailLength;

        MTILayer( juce::ValueTree& VT, juce::UndoManager* UM );

        void setSource( const juce::String& Source, int Slice );
        void clearSource();

        juce::ValueTree layerTree;
    };

    struct MTISample
    {
        juce::ValueTree    vTree;
        juce::UndoManager* um{ nullptr };

        CachedValue<bool>                 solo;
        CachedValue<bool>                 mute;
        CachedValue<MidiConstrainer>      keyNote;
        CachedValue<MidiConstrainer>      minNote;
        CachedValue<MidiConstrainer>      maxNote;
        CachedValue<MuteGroupConstrainer> muteGroup;
        CachedValue<PolyphonyConstrainer> polyPhony;
        CachedValue<SemiConstrainer>      semi;
        CachedValue<FineConstrainer>      fine;
        CachedValue<GainConstrainer>      gain;
        CachedValue<PanConstrainer>       pan;

        MTSimultaneousAction sPlay;
        MTSimultaneousAction sMute;

        std::atomic<int>   cents{ 0 }; // Don't set this directly. Treat as ReadOnly.
        std::atomic<float> gL{ 1.0 }; // Don't set this directly. Treat as ReadOnly.
        std::atomic<float> gR{ 1.0 }; // Don't set this directly. Treat as ReadOnly.

        std::array<std::unique_ptr<MTILayer>, MAX_LAYERS> layers;

        MTISample( juce::ValueTree& VT, juce::UndoManager* UM );
        // Held as std::unique_ptr<MTISample> but created as derived pad/clip types.
        virtual ~MTISample() = default;

        void                                 removeSource( const juce::String& Source );
        juce::String                         getSource( int Layer = 0 );
        ASoundSlice*                         getSourceSlice( int Layer = 0 );
        std::array<juce::String, MAX_LAYERS> getSources();

        virtual void virtualFunc(){};
    };

    struct MTILFO
    {
        CachedValue<bool>            lfoIsEnabled;
        CachedValue<int>             lfoType;
        CachedValue<float>           lfoFrequency;
        CachedValue<int>             lfoFilter;
        CachedValue<int>             lfoPitch;
        CachedValue<int>             lfoSync;
        CachedValue<PanConstrainer>  lfoPan;
        CachedValue<MidiConstrainer> lfoLevel;

        MTILFO( const juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIPitchEnvelope
    {
        CachedValue<ADSRType> pitchtype;
        CachedValue<int>      pitchAttack;
        CachedValue<int>      pitchDecay;
        CachedValue<int>      pitchDepth;
        CachedValue<int>      pitchHold;
        CachedValue<int>      pitchSustain;

        MTIPitchEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIFilterEnvelope
    {
        CachedValue<ADSRType> filtertype;
        CachedValue<int>      filterAttack;
        CachedValue<int>      filterDecay;
        CachedValue<int>      filterDepth;
        CachedValue<int>      filterHold;
        CachedValue<int>      filterSustain;

        MTIFilterEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIAmpEnvelope
    {
        CachedValue<ADSRType> amptype;
        CachedValue<int>      ampAttack;
        CachedValue<int>      ampDecay;
        CachedValue<int>      ampDepth;
        CachedValue<int>      ampHold;
        CachedValue<int>      ampSustain;

        MTIAmpEnvelope( const juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTICommonDrumKeys
        : public MTISample
        , public MTILFO
        , public MTIPitchEnvelope
        , public MTIFilterEnvelope
        , public MTIAmpEnvelope
    {
        CachedValue<int> idexssgh;

        MTICommonDrumKeys( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIClip : public MTISample
    {
        CachedValue<ClipPlayType> padPlay;

        MTIClip( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIDrum : public MTICommonDrumKeys
    {
        CachedValue<SamplePlayType> samplePlay;
        CachedValue<LayerPlayType>  layerPlay;

        MTIDrum( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIKeys : public MTICommonDrumKeys
    {
        CachedValue<bool> keyTracking;
        CachedValue<SamplePlayType> samplePlay;
        CachedValue<LayerPlayType>  layerPlay;

        MTIKeys( juce::ValueTree& VT, juce::UndoManager* UM );
    };

    struct MTIAudio : public MTISource
    {
        CachedValue<int>  startTime;
        CachedValue<int>  length;
        CachedValue<int>  start;
        CachedValue<int>  end;
        CachedValue<int>  level;
        CachedValue<int>  bpm;
        CachedValue<bool> mute;
        CachedValue<bool> reverse;
        CachedValue<bool> warp;

        juce::ValueTree vTree;
        MTIAudio( juce::ValueTree& VT, juce::UndoManager* UM = nullptr );

        void setSource( const juce::String& Source, int Slice, double Start, double Length );
        void clearSource();
        void setLoopPoints( double, double );
    };

    struct MTAudioSampleData
    {
        bool         reverse{ false };
        bool         wrap{ false };
        juce::String source{ "" };
    };

    struct MTSelected
    {
        int sequence{ 0 };
        int track{ 0 };
        int program{ 0 };
    };

    struct MTPadColors
    {
        EmptyPadColorMode    emptyMode{ EmptyPadColorMode::Off };
        PadColorMode         Mode{ OFF };
        std::array<int, 128> Colours;
    };

    struct MTIPadColors
    {
        explicit MTIPadColors( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        CachedValue<PadColorMode>         colourMode;
        CachedValue<EmptyPadColorMode>    emptyPadColourMode;
        std::array<CachedValue<int>, 128> padColour;
    };

    struct MTIPadPerformanceData
    {
        explicit MTIPadPerformanceData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        CachedValue<PerformanceMode> performMode;
        CachedValue<juce::String>    performScale;
        CachedValue<juce::String>    performRootNote;
        CachedValue<juce::String>    performOctave;
        CachedValue<juce::String>    performChords;
        CachedValue<juce::String>    performBanks;
    };

    struct MTIProgramData
        : public MTIPadColors
        , public MTIPadPerformanceData
    {
        explicit MTIProgramData( juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        int                                   index{ 0 };
        CachedValue<bool>                     mute;
        CachedValue<bool>                     solo;
        CachedValue<MTProgramType>            type;
        CachedValue<juce::String>             id;
        CachedValue<juce::String>             name;
        CachedValue<juce::String>             output;
        CachedValue<MidiConstrainer>          level;
        CachedValue<SemiConstrainer>          semi;
        CachedValue<FineConstrainer>          fine;
        CachedValue<MidiTransposeConstrainer> transpose;
        CachedValue<PolyphonyConstrainer>     polyphony;
        CachedValue<PanConstrainer>           pan;
        CachedValue<GainConstrainer>          send1;
        CachedValue<GainConstrainer>          send2;
        CachedValue<GainConstrainer>          send3;
        CachedValue<GainConstrainer>          send4;

        /// The PROGRAM tree itself, for settings without a typed member (e.g. PAD_LAYOUT).
        juce::ValueTree programState;
    };

    struct MTIClipProgramData
    {
        explicit MTIClipProgramData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        CachedValue<float> launchQuantize;
    };

    struct MTICvProgramData
    {
        explicit MTICvProgramData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        CachedValue<juce::String> cvNote;
        CachedValue<int>          cvPort;
        CachedValue<int>          cvGate;
        CachedValue<int>          cvWheel;
        CachedValue<int>          cvVelocity;
    };

    struct MTIMidiProgramData
    {
        explicit MTIMidiProgramData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );

        CachedValue<juce::String>           midiPort;
        CachedValue<int>                    programCh;
        CachedValue<int>                    bankMsb;
        CachedValue<int>                    bankLsb;
        CachedValue<MidiChannelConstrainer> midiCh;
    };

    struct MTIAudioProgramData
    {
        explicit MTIAudioProgramData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );
        CachedValue<juce::String> input;
        CachedValue<bool> armed, monitor;
    };

    struct MTISequenceData : public MTILoopData
    {
        int         numTracks{ 0 };
        int         index{ 0 };
        RecordState rState{ RecordState::NotArmed };

        CachedValue<bool>                     isMasterBPM;
        CachedValue<juce::String>             id;
        CachedValue<juce::String>             name;
        CachedValue<LengthConstrainer>        length;
        CachedValue<MidiTransposeConstrainer> transpose;
        CachedValue<float>                    tempo;

        explicit MTISequenceData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );
        void forceUpdateOfCachedValues() final;
    };

    struct MTITrackData
    {
        explicit MTITrackData( const juce::ValueTree& V, juce::UndoManager* UM = nullptr );
        CachedValue<bool> armed, monitor;

        bool                                  record{ false };
        int                                   index{ 0 };
        CachedValue<bool>                     isSeqLength;
        CachedValue<bool>                     mute;
        CachedValue<bool>                     solo;
        CachedValue<bool>                     muteDueToSolo;
        CachedValue<juce::String>             name;
        CachedValue<juce::String>             id;
        CachedValue<juce::String>             midiPort;
        CachedValue<juce::String>             programId;
        CachedValue<juce::String>             route;
        CachedValue<MidiTransposeConstrainer> transpose;
        CachedValue<MuteGroupConstrainer>     muteGroup;
        CachedValue<LengthConstrainer>        length;
        CachedValue<MidiChannelConstrainer>   midiChannel;
        CachedValue<MidiChannelConstrainer>   midiOutChannel;
        CachedValue<MidiConstrainer>          level;
        CachedValue<PanConstrainer>           pan;

        MTIProgramData* prog{ nullptr };
    };

    class MTNoteRO
    {
    public:
        explicit MTNoteRO( const juce::ValueTree& V );

        bool isValid() const;
        bool mute() const;
        int  start() const;
        int  length() const;
        int  number() const;
        int  velocity() const;
        int  probability() const;
        int  ratchet() const;

    private:
        const juce::ValueTree& vt;
    };

    class MTNote
    {
    public:
        CachedValue<bool>               mute;
        CachedValue<int>                id;
        CachedValue<int>                start;
        CachedValue<int>                length;
        CachedValue<MidiConstrainer>    number;
        CachedValue<MidiConstrainer>    velocity;
        CachedValue<MidiConstrainer>    probability;
        CachedValue<RatchetConstrainer> ratchet;

        MTNote( const juce::ValueTree& V = juce::ValueTree{ IDs::NOTE }, juce::UndoManager* UM = nullptr );
        MTNote( const MTNote& rhs );

        bool isValid() const;
        bool operator<( const MTNote& rhs ) const;
        bool operator()( MTNote const& lhs, MTNote const& rhs ) const;
        bool operator==( const MTNote& rhs ) const;

        juce::ValueTree& getState();

    private:
        juce::ValueTree    state;
        juce::UndoManager* um{ nullptr };
        void               referTo( const juce::ValueTree& V, juce::UndoManager* UM );
    };

    struct track
    {
        enum class RandomEventType
        {
            ReplaceAll,
            ReplaceInRange,
            AppendExisting
        };
        enum class EventType
        {
            DrumEvents,
            MelodicEvents
        };

        struct noteRange
        {
            juce::String start{ "C-2" };
            juce::String end{ "G8" };
        };

        struct splitInfo
        {
            bool onlySelected{ false };
            int  parts{ 2 };
        };

        struct copyInfo
        {
            juce::String from{ "" };
            juce::String to{ "" };
        };

        struct humanizeInfo
        {
            bool onlySelected{ false };
            bool isNoteLengthEnabled{ false };
            bool isTimeEnabled{ false };
            bool isVelocityEnabled{ false };
            int  eagerness{ 0 };
            int  time{ 1 };
            int  velocity{ 1 };
            int  noteLength{ 2 };
        };

        struct pitchQuantizeInfo
        {
            bool         onlySelected{ false };
            noteRange    range;
            juce::String rootNoteValue;
            juce::String scaleValue;
        };

        struct randomEventsInfo
        {
            bool            isLegatoSelected{ false };
            bool            isConstrainNotesToScaleSelected{ false };
            EventType       eventType{ EventType::DrumEvents };
            RandomEventType replace{ RandomEventType::ReplaceAll };
            int             patternSize;
            int             densityValue;
            int             rhythmVariationValue;
            int             polyphonyValue;
            MTTimeBase      noteLength;
            juce::String    bankValue;
            juce::String    rootNoteValue;
            juce::String    scaleValue;
            noteRange       pads;
            noteRange       range;
        };

        struct copyTrackNotes
        {
            int          FromSeq;
            int          FromTrack;
            int          ToSeq;
            int          ToTrack;
            bool         CopyEvents;
            NotesInRange SourceRange;
            NotesInRange DestinationRange;
            MTBarNBeats  DestTime;
            int          NumCopy{ 1 };
        };
    }; // track

    ///
    /// SONG INFORMATION
    ///
    struct MTSongStepInfo
    {
        juce::String seqID;
        juce::String dispName;
        int          seqNo{ 0 };
        int          repeatCount{ 1 };
        float        bpm{ 0 };
        int          bars{ 0 };
        int          loopEnd{ 0 };
    };
    ///
    struct MTSongInfo
    {
        juce::String                name{ "" };
        std::vector<MTSongStepInfo> steps;
    };

    struct MTRecorderListener
    {
        std::function<void()>                       onRecorderStarted        = []() {};
        std::function<void()>                       onRecorderStopped        = []() {};
        std::function<void()>                       onRecorderPaused         = []() {};
        std::function<void()>                       onRecorderResumed        = []() {};
        std::function<void()>                       onRecorderPreviewStarted = []() {};
        std::function<void()>                       onRecorderPreviewStopped = []() {};
        std::function<void( double TimeInSeconds )> onRecorderLengthChanged  = []( double ) {};
    };

    using ParamInfo = AudIO::ParamInfo;

    class MTIBus
        : public MTIInsertsInterface
        , private juce::ValueTree::Listener
    {
    public:
        explicit MTIBus( MTBus* Bus );
        ~MTIBus();

        void setup( juce::ValueTree VT, juce::UndoManager* UM );
        void setBus( MTBus* Bus );

        inline MTIInsertsInterface* getInsertsInterface() { return dynamic_cast<MTIInsertsInterface*>( this ); }

        juce::String                 name;
        CachedValue<bool>            mute;
        CachedValue<bool>            bypass;
        CachedValue<bool>            solo;
        CachedValue<bool>            limiter;
        CachedValue<juce::String>    routedTo;
        CachedValue<GainConstrainer> gain;
        CachedValue<PanConstrainer>  pan;

        void               enableInMetering( bool Enable );
        void               enableOutMetering( bool Enable );
        const MTMeterData& readInMeter() const;
        const MTMeterData& readOutMeter() const;

    private:
        juce::ValueTree state;
        MTBus*          bus{ nullptr };
        void            valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property ) override;
    };

    class MTBusManager
    {
    public:
        explicit MTBusManager( AudIO::DawBusManager& BusMgr );

        void reset();
        void setup( juce::ValueTree VT, juce::UndoManager* UM );

        int getNumIns() const;
        int getNumMonoIns() const;
        int getNumStereoIns() const;
        int getNumOuts() const;
        int getNumMonoOuts() const;
        int getNumStereoOuts() const;
        int getNumSends() const;
        int getNumReturns() const;
        int getNumSubmixes() const;
        int getNumAuxes() const;
        int getNumObjects() const;
        int getNumOthers() const;

        uint32_t     getBusId( const juce::String& Name ) const;
        juce::String getBusName( uint32_t Id ) const;

        MTIBus* getBus( const juce::String& Name ) const;
        MTIBus* getBus( const uint32_t ID ) const;

        MTIBus* getSends( uint32_t Position ) const;
        MTIBus* getReturns( uint32_t Position ) const;
        MTIBus* getSubmix( uint32_t Position ) const;
        MTIBus* getOutput( uint32_t Position ) const;
        MTIBus* getInput( uint32_t Position ) const;

        uint32_t getSendsBusID( uint32_t Position ) const;
        uint32_t getReturnsBusID( uint32_t Position ) const;
        uint32_t getSubmixBusID( uint32_t Position ) const;
        uint32_t getOutBusID( uint32_t Position ) const;
        uint32_t getInBusID( uint32_t Position ) const;

        juce::String getSendsBusName( uint32_t Position ) const;
        juce::String getReturnsBusName( uint32_t Position ) const;
        juce::String getSubmixBusName( uint32_t Position ) const;
        juce::String getOutBusName( uint32_t Position ) const;
        juce::String getInBusName( uint32_t Position ) const;

        // BUS Names...
        const std::vector<std::string>& getInputBusNames();
        const std::vector<std::string>& getOutputBusNames();
        const std::vector<std::string>& getSendBusNames();
        const std::vector<std::string>& getReturnBusNames();
        const std::vector<std::string>& getSubmixBusNames();

        // Bus control API's
        float getBusGain( const juce::String& BusName );
        float getBusPan( const juce::String& BusName );

        bool isBusMuted( const juce::String& BusName );
        bool isBusBypassed( const juce::String& BusName );
        bool isBusSolo( const juce::String& BusName );

        void setBusMute( const juce::String& BusName, bool Value );
        void setBusBypass( const juce::String& BusName, bool Value );
        void setBusSolo( const juce::String& BusName, bool Value );

        void setBusGain( const juce::String& BusName, float Value );
        void setBusPan( const juce::String& BusName, float Value );

        void               enableInMetering( const juce::String& BusName, bool Enable );
        void               enableOutMetering( const juce::String& BusName, bool Enable );
        const MTMeterData& readInMeter( const juce::String& BusName ) const;
        const MTMeterData& readOutMeter( const juce::String& BusName ) const;

    private:
        AudIO::DawBusManager&                           busMgr;
        std::map<juce::String, std::unique_ptr<MTIBus>> busMap;
        juce::ValueTree                                 state;
        juce::UndoManager*                              um{ nullptr };
    };
} // namespace MT
