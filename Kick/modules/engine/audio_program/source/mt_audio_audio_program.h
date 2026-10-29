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

#include "mt_audio_iprogram.h"

namespace mt
{
    class MTAudioProgram : public MTIProgram, private juce::AsyncUpdater
        , public MTIAudioProgramData
    {
    public:
        explicit MTAudioProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        virtual ~MTAudioProgram();
        std::shared_ptr<AudioPlaybackClock> createPlaybackClock();
        bool prepareRecording(const std::shared_ptr<AudioPlaybackClock>& clock, int startTick, int endTick, double tickRate, bool replace);
        bool finishRecording();
        bool isTakeComplete() const { return prog->timeline.captureDone(); }
        const juce::String& getRecordingError() const { return recordingError; }
        const MTMeterData& readInMeter() const override { return prog->readInputMeter(); }
        const MTMeterData& readOutMeter() const override { return prog->readOutMeter(); }
        void enableOutputMetering(bool enable) override { prog->enableOutputMetering(enable); }
        void refresh() override;
        void setSelectedAudioProperty(const juce::Identifier& property, const juce::var& value);
        juce::ValueTree copySelectedAudio() const;
        bool pasteAudio(const juce::ValueTree& clipboard, int atTick, int limit);
        bool splitSelectedAudio(int atTick, double tickRate);
        juce::String getDrawingSource() const;
        void applySendAutomation(int tick);
        void playbackStopped();
        void clearAutomation(const juce::String& name);
        void eraseAutomationPoint(const juce::String& name, int tick);

        void forAllAudio( const std::function<void( const juce::ValueTree& audioVT )>& func );

        void setSelectedAudioSampleLevel( int value );
        void setSelectedAudioSampleMute( bool isMute );
        void setSelectedAudioSampleReverse( bool isReverse );
        void setSelectedAudioSampleWarp( bool isWarp );
        void setSelectedAudioSampleBPM( int value );

        juce::ValueTree&              getAudioSampleList() { return sampleTree; }
        juce::Array<juce::ValueTree>& getSelectedAudioSampleList() { return selectedList; }

        void setLoopEnd( int max, int Start );
        void setLoopStart( int min, int Start );

        void selectAllSamples(bool select) { selectedList.clearQuick(); if(select) for(auto clip : sampleTree) selectedList.add(clip); }
        void            setValueForParam( const juce::String&, int Start, double Value );
        juce::ValueTree getParamTree( const juce::String& Param );
        void            eraseSelectedSample();

        void addSample( const juce::String& Source, int Start, int Length, int Slice );
        void removeSample( int Start );
        void forAllAudio( const std::function<void( MTIAudio& audio )>& func );

        void selectAudioSample( int Start );
        void setSelectAudioSample( int Start, bool IsSelect );
        void setStartForSample( int, int );
        void setLoopPointsForSample( int, double, double );
        void setSampleLength( int, int );

        juce::ValueTree getClipTree( int Start );
        std::unique_ptr<MTIAudio> getAudioClip( int Start );
        ASoundSlice*    getAudioSlice( int Start );
        std::unique_ptr<MTIAudio> getAudioSample( int Start );

        juce::Array<std::unique_ptr<MTIAudio>> getAudioClipsData();

        //Parameter API's

        int editSelectedSampleStart( int, int, bool );
        int editSelectedSampleEnd( int, int, bool );

    protected:
    private:
        friend struct AudioWorkflowAuditAccess;
        std::unique_ptr<MTIAudio> getOrCreateAudioSample( int Start );

    private:
        void handleAsyncUpdate() override { refresh(); }
        void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
        void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
        void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
        void updateInput();
        void replaceAudioRange(int start, int end, double tickRate);
        juce::ValueTree croppedAudio(const juce::ValueTree& clip, int start, int finish, double tickRate);
        juce::String drawingSource;
        bool takePrepared{false}, replaceTake{false};
        int takeStart{0}, takeEnd{0}; double takeTickRate{0};
        juce::File takeFile;
        juce::String recordingError;
        uint32_t connectedInputId{0};
        MTAudioInterface&                      ae{ getAudioInterface() };
        juce::Array<juce::ValueTree>           selectedList;
        std::vector<std::unique_ptr<MTIAudio>> selectedLists;
        juce::ValueTree                        sampleTree;
        bool                                   editStart{ false };

    private:
        AudIO::DawEngine& engine;
        AudioProgram*     prog{nullptr};
    };
} // namespace MT
