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

#include "mt_audio_metronome.h"
#include "mt_audio_recorder.h"
#include "mt_audio_manager_program.h"
#include "mt_audio_manager_sequence.h"
#include "mt_audio_manager_transport.h"

namespace mt
{
    class MTSequence;
    class MTTrack;
    class AudioDevice;

    //
    class MTAudioManager
        : public projectManager
        , public MTTempoMgr
        , public MTMidiDeviceManager
        , public MTAudioRecorderManager
        , public MTAudioAssetManager
        , public MTSongManager
        , public MTAudioProgramManager
        , public MTAudioTransportManager
        , public MTIPadPerformanceData
    {
    public:
        MTAudioManager( const char* PrjName, const char* PrjManufacturer, const char* PrjVersion );
        virtual ~MTAudioManager();

        void addListener( MTAudioInterfaceListener* l );
        void removeListener( MTAudioInterfaceListener* l );
        void removeAllListeners() { listeners.clear(); }
        std::function<void(const MTSelected&, const juce::MidiMessage&)> liveMidiReceived;
        std::function<void()> externalMidiStop;

        // Project API's..
        bool isProjectLoaded() const noexcept { return projectLoaded; };
        bool loadProject( juce::File& ProjectFile );
        bool closeProject();
        bool saveProject();

        // State
        inline juce::UndoManager& getUndoManager() noexcept { return undoManager; }
        inline juce::ValueTree&   getValueTree() noexcept { return vTree; }

        // Selection API's...
        inline const MTSelected&   getSelected() { return selected; };
        inline const juce::String& getSelectedStr() { return selectedStr; };
        void                       setSelected( MTSelected Selected );

        // Creators
        MTSampleEditorInterface* createSampleEditor();

        // Utilities...
        void triggerTrackNotesChanged( int Sequence, int Track );
        void triggerTrackProgramChanged( int Track, MTIProgram* Program );

        // Device manager...
        juce::AudioDeviceManager& getAudioDeviceManager();

        // Bus Manager
        MTBusManager& getBusManager() { return busMgr; }

        //Song Utilities
        void playSong( const juce::String& SongName, int SequenceIndexToPlay );
        void resumeSong();
        void stopSong();

        // Preview Utilities...
        void        createPreview( const juce::File& F, float Gain );
        void        stopPreview();
        void        loopPreview( bool Loop );
        void        setPreviewMute( bool Mute );
        bool        isPreviewRunning();
        bool        isPreviewLooping();
        bool        isPreviewMuted();
        void        setPreviewGain( float Gain );
        MTMeterData readPreviewMeter();

        void                setBank( const juce::String& Bank );
        inline juce::String getBank() const { return bank; };

        void performUndo();
        void performRedo();

    private:
        void interTimerCallbackHandler();

    private:
        AudIO::DawEngine engine{ AUDIO_NUM_INPUTS, AUDIO_NUM_OUTPUTS, AUDIO_NUM_SEND_RETURNS, AUDIO_NUM_SUBMIX, AUDIO_NUM_AUX };

        juce::ValueTree   vTree{ IDs::ROOT };
        juce::String      selectedStr{ "" };
        juce::String      bank{ "A" };
        juce::UndoManager undoManager;

        juce::ListenerList<MTAudioInterfaceListener> listeners;

        std::unique_ptr<AudioDevice> dm;
        std::unique_ptr<MTMetronome> _metronome;

        MTBusManager busMgr{ *engine.getBusManager() };
        MTSelected   selected{ -1, -1, -1 };

        AudioPreview*    _previewer{ nullptr };
        KickPrivateTimer internalTimer;

        bool projectLoaded{ false };

#if defined( USE_VALUE_TREE_DEBUGGER ) && ( USE_VALUE_TREE_DEBUGGER > 0 )
        jcf::ValueTreeDebugger debugger;
#endif

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTAudioManager )
    };
} // namespace MT
