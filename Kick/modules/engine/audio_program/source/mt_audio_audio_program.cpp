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

#include "mt_audio_audio_program.h"

namespace mt
{
    MTAudioProgram::MTAudioProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgram( VT, UM )
        , MTIAudioProgramData( VT, UM )
        , engine( Engine )
    {
        auto pProg = std::make_unique<AudioProgram>( Engine, getName().toStdString(), this );
        if( pProg )
        {
            prog = pProg.get();
            engine.createObject( std::move( pProg ) );
            prog->initAudioRouting();

            auto outBusId = engine.getBusManager()->getBusId( getOutput().toStdString() );
            prog->connectOutputTo( outBusId );
            prog->setGainsForSend( getSendGains() );

            onNameChanged     = [&]() { prog->setName( getName().toStdString() ); };
            onSendGainChanged = [&]( int Channel, float Value ) { prog->setGainForSend( (uint32_t)Channel, Value ); };

            onOutChanged = [&]() {
                auto outBusId = engine.getBusManager()->getBusId( getOutput().toStdString() );
                prog->connectOutputTo( outBusId );
            };

            sampleTree = VT.getOrCreateChildWithName( IDs::AUDIOS, nullptr );

            // Inserts...
            auto vt = VT.getOrCreateChildWithName( IDs::INSERTS, nullptr );
            MTIInsertsInterface::setup( vt, UM, prog );
            input.onValueChange.connect([this] { updateInput(); });
            monitor.onValueChange.connect([this] { prog->monitor = monitor.get(); });
            updateInput(); prog->monitor = monitor.get(); refresh();
        }
    }

    MTAudioProgram::~MTAudioProgram()
    {
        cancelPendingUpdate();
        if( prog )
        {
            engine.removeObject( prog->getId() );
        }
    }

    void MTAudioProgram::setSelectedAudioSampleLevel( int Value )
    {
        for( auto& vt : selectedList )
        {
            MTIAudio sample( vt, um );
            sample.level = Value;
        }
    }
    void MTAudioProgram::setSelectedAudioSampleMute( bool isMuted )
    {
        for( auto& vt : selectedList )
        {
            MTIAudio sample( vt, um );
            sample.mute = isMuted;
            //sample.src->isMuted = isMuted;
        }
    }
    void MTAudioProgram::setSelectedAudioSampleReverse( bool isReversed )
    {
        for( auto& vt : selectedList )
        {
            MTIAudio sample( vt, um );
            sample.reverse = isReversed;
            //sample.src->isReversed = isReversed;
        }
    }
    void MTAudioProgram::setSelectedAudioSampleWarp( bool isWarp )
    {
        for( auto& vt : selectedList )
        {
            MTIAudio sample( vt, um );
            sample.warp = isWarp;
            // sample.src->isWarp = isWarp;
        }
    }
    void MTAudioProgram::setSelectedAudioSampleBPM( int value )
    {
        for( auto& vt : selectedList )
        {
            MTIAudio sample( vt, um );
            sample.bpm = value;
        }
    }

    void MTAudioProgram::setLoopEnd( int max, int Start )
    {
        auto p          = getOrCreateAudioSample( Start );
        p->src->loopEnd = max;
    }
    void MTAudioProgram::setLoopStart( int min, int Start )
    {
        auto p            = getOrCreateAudioSample( Start );
        p->src->loopStart = min;
    }

    void MTAudioProgram::setValueForParam( const juce::String& Param, int Start, double Value )
    {
        if(AudioAutomation::index(Param) < 0 || Start < 0 || !std::isfinite(Value)) return;
        auto vt        = vTree.getOrCreateChildWithName( "PARAMETERS", um );
        auto paramTree = vt.getOrCreateChildWithName( Param, um );
        paramTree.setProperty( "s" + juce::String( Start ), juce::jlimit(0.0, 100.0, Value), um );
    }

    juce::ValueTree MTAudioProgram::getParamTree( const juce::String& Param )
    {
        auto vt        = vTree.getChildWithName("PARAMETERS");
        auto paramTree = vt.getChildWithName(Param);
        return paramTree;
    }

    std::unique_ptr<MTIAudio> MTAudioProgram::getOrCreateAudioSample( int Start )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( Start == audioSample.startTime.get() )
                return std::make_unique<MTIAudio>( audio, um );
        }
        juce::ValueTree newSample{ IDs::CLIP };
        vt.addChild( newSample, -1, um );
        return std::make_unique<MTIAudio>( newSample, um );
    }

    void MTAudioProgram::addSample( const juce::String& Source, int Start, int Length, int Slice )
    {
        if( Source.isEmpty() || Start < 0 || Length <= 0 )
            return;
        if( auto sample = getOrCreateAudioSample( Start ) )
        {
            sample->setSource( Source, Slice, Start, Length );
            drawingSource = Source;
        }
    }

    void MTAudioProgram::removeSample( int Start )
    {
        if( auto p = getOrCreateAudioSample( Start ) )
            p->clearSource();
    }

    void MTAudioProgram::forAllAudio( const std::function<void( MTIAudio& audio )>& func )
    {
        if( func )
        {
            sampleTree = vTree.getOrCreateChildWithName( IDs::AUDIOS, nullptr );
            for( auto it : sampleTree )
            {
                MTIAudio audio( it, um );
                func( audio );
            }
        }
    }

    void MTAudioProgram::eraseSelectedSample()
    {
        if( selectedList.size() )
        {
            for( auto sel : selectedList )
                sel.getParent().removeChild( sel, um );
            selectedList.clearQuick();
        }
    }

    std::unique_ptr<MTIAudio> MTAudioProgram::getAudioClip( int Start )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( Start == audioSample.startTime.get() )
                return std::make_unique<MTIAudio>(audio, um);
        }
        return nullptr;
    }

    std::unique_ptr<MTIAudio> MTAudioProgram::getAudioSample( int Start )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( Start >= audioSample.startTime.get() && ( Start < ( audioSample.startTime.get() + audioSample.length.get() ) ) )
                return std::make_unique<MTIAudio>(audio, um);
        }
        return nullptr;
    }

    juce::ValueTree MTAudioProgram::getClipTree( int Start )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( Start == audioSample.startTime.get() )
                return audio;
        }
        return juce::ValueTree{};
    }

    ASoundSlice* MTAudioProgram::getAudioSlice( int Start )
    {
        if( auto p = getAudioClip( Start ) )
            return p->src;
        return nullptr;
    }

    void MTAudioProgram::selectAudioSample( int Start )
    {
        juce::ValueTree clipTree;
        auto            vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, nullptr );
        for( auto it : vt )
        {
            MTIAudio audioClip( it, um );
            if( audioClip.startTime.get() <= Start && Start < audioClip.startTime.get() + audioClip.length.get() )
            {
                drawingSource = audioClip.source.get();
                selectedList.addIfNotAlreadyThere( it );
                return;
            }
        }
    }

    void MTAudioProgram::setSelectAudioSample( int Start, bool IsSelect )
    {
        juce::ValueTree clipTree;
        auto            vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, nullptr );
        for( auto it : vt )
        {
            MTIAudio audioClip( it, um );
            if( audioClip.startTime.get() == Start )
            {
                if( IsSelect ) selectedList.addIfNotAlreadyThere( it );
                else selectedList.removeAllInstancesOf( it );
            }
        }
    }

    void MTAudioProgram::setStartForSample( int PrevStart, int NewStart )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( PrevStart >= audioSample.startTime.get() && ( PrevStart <= ( audioSample.startTime.get() + audioSample.length.get() ) ) )
                audioSample.startTime = NewStart;
        }
    }

    void MTAudioProgram::setLoopPointsForSample( int StartTime, double Start, double End )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( StartTime == audioSample.startTime.get() )
                audioSample.setLoopPoints( Start, End );
        }
    }

    void MTAudioProgram::setSampleLength( int Start, int Length )
    {
        auto vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, um );
        for( auto audio : vt )
        {
            MTIAudio audioSample( audio, um );

            if( Start == audioSample.startTime.get() )
                audioSample.length = Length;
        }
    }

    juce::Array<std::unique_ptr<MTIAudio>> MTAudioProgram::getAudioClipsData()
    {
        juce::Array<std::unique_ptr<MTIAudio>> clipsData;
        auto                                   vt = vTree.getOrCreateChildWithName( IDs::AUDIOS, nullptr );
        for( auto clipTree : vt )
        {
            clipsData.add( std::make_unique<MTIAudio>( clipTree, um ) );
        }
        return clipsData;
    }

    int MTAudioProgram::editSelectedSampleStart( int NudgeValue, int SamplesPerTick, bool IsRight )
    {
        if( selectedList.size() )
        {
            for( auto& vt : selectedList )
            {
                MTIAudio sample{ vt, getUndoManager() };
                if( auto pSample = getAudioSoundCache().exists( sample.source.get() ) )
                {
                    auto maxSamples = pSample->getNumSamplesPerChannel();
                    auto loopStart  = sample.start.get();
                    auto loopEnd    = sample.end.get() == 0 ? maxSamples : sample.end.get();
                    auto length     = sample.length.get();
                    auto startTime  = sample.startTime.get();
                    auto maxLength  = (int)maxSamples / SamplesPerTick;

                    if( maxLength > NudgeValue )
                    {
                        auto afterNudge = loopStart + ( NudgeValue * SamplesPerTick );

                        if( IsRight )
                        {
                            if( afterNudge < loopEnd )
                            {
                                if( loopEnd - afterNudge >= NudgeValue * SamplesPerTick )
                                {
                                    startTime += NudgeValue;
                                    loopStart     = afterNudge;
                                    sample.length = sample.length.get() - NudgeValue;
                                }
                                else if( length > NudgeValue )
                                {
                                    auto newLoopStart    = loopEnd - ( NudgeValue * SamplesPerTick );
                                    auto nudgebleSamples = newLoopStart - loopStart;
                                    auto nudgeWidth      = (int)nudgebleSamples / SamplesPerTick;
                                    startTime += nudgeWidth;
                                    loopStart     = newLoopStart;
                                    sample.length = sample.length.get() - nudgeWidth;
                                }
                            }
                            else
                                return -1;
                        }
                        else
                        {
                            auto newStart = startTime + NudgeValue;
                            if( afterNudge >= 0 && newStart >= 0 )
                            {
                                loopStart     = afterNudge;
                                startTime     = newStart;
                                sample.length = sample.length.get() + ( sample.startTime.get() - startTime );
                            }
                            else if( newStart < 0 )
                            {
                                auto nudgebleSamples = startTime * SamplesPerTick;
                                auto newLoopStart    = loopStart - nudgebleSamples;
                                if( newLoopStart >= 0 )
                                {
                                    loopStart     = newLoopStart;
                                    sample.length = sample.length.get() + startTime;
                                    startTime     = 0;
                                }
                                else
                                {
                                    auto nudgeWidth = (int)loopStart / SamplesPerTick;
                                    loopStart       = 0;
                                    startTime       = startTime - nudgeWidth;
                                    sample.length   = sample.length.get() + nudgeWidth;
                                }
                            }
                            else if( afterNudge < 0 )
                            {
                                auto nudgeWidth = (int)loopStart / SamplesPerTick;
                                loopStart       = 0;
                                startTime       = startTime - nudgeWidth;
                                sample.length   = sample.length.get() + nudgeWidth;
                            }
                        }
                        sample.startTime = startTime;
                        sample.setLoopPoints( loopStart, loopEnd );
                        return 0;
                    }
                    return -1;
                }
            }
        }
        return -1;
    }

    int MTAudioProgram::editSelectedSampleEnd( int NudgeValue, int SamplesPerTick, bool IsRight )
    {
        if( selectedList.size() )
        {
            for( auto& vt : selectedList )
            {
                MTIAudio sample{ vt, getUndoManager() };
                if( auto pSample = getAudioSoundCache().exists( sample.source.get() ) )
                {
                    auto maxSamples = pSample->getNumSamplesPerChannel();
                    auto maxLength  = (int)maxSamples / SamplesPerTick;

                    if( maxLength > ( IsRight ? NudgeValue : -NudgeValue ) )
                    {
                        auto loopStart  = sample.start.get();
                        auto loopEnd    = sample.end.get() == 0 ? maxSamples : sample.end.get();
                        auto length     = sample.length.get();
                        auto startTime  = sample.startTime.get();
                        auto afterNudge = loopEnd + ( NudgeValue * SamplesPerTick );

                        if( IsRight )
                        {
                            if( afterNudge <= maxSamples )
                            {
                                loopEnd       = afterNudge;
                                sample.length = length + NudgeValue;
                            }
                            else
                            {
                                auto nudgebleSamples = maxSamples - loopEnd;
                                auto nudgeDist       = (int)nudgebleSamples / SamplesPerTick;
                                loopEnd              = maxSamples;
                                sample.length        = length + nudgeDist;
                            }
                        }
                        else
                        {
                            if( length - ( -NudgeValue ) >= ( -NudgeValue ) )
                            {
                                loopEnd       = afterNudge;
                                sample.length = length + NudgeValue;
                            }
                            else
                            {
                                auto nudgebleSamples = loopEnd - loopStart - ( -NudgeValue * SamplesPerTick );
                                auto nudgeDist       = (int)nudgebleSamples / SamplesPerTick;
                                loopEnd              = loopEnd - nudgebleSamples;
                                sample.length        = length - nudgeDist;
                            }
                        }

                        sample.setLoopPoints( loopStart, loopEnd );
                        return 0;
                    }
                }
            }
        }
        return -1;
    }
}
