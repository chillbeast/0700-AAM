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

namespace mt
{
    /// <summary>
    ///  ASoundCache
    /// </summary>
    ///
    ASoundCache& getAudioSoundCache()
    {
        static ASoundCache cache;
        return cache;
    }

    ASoundCache::ASoundCache()
    {
        fmtMgr.registerBasicFormats();
    }

    ASoundCache::~ASoundCache()
    {
        clear();
    }

    void ASoundCache::clear()
    {
        aSounds.clear();
    }

    void ASoundCache::purge()
    {
        for( const auto& it : aSounds )
        {
            return it->purge();
        }
    }

    ASound::Ptr ASoundCache::exists( const juce::String& File )
    {
        for( auto it : aSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
        }
        return nullptr;
    }

    ASound::Ptr ASoundCache::getOrLoad( const juce::String& File, double LengthInSeconds )
    {
        if( auto ptr = exists( File ) )
            return ptr;

        return add( File );
    }

    void ASoundCache::remove( const juce::String& File )
    {
        aSounds.removeObject( exists( File ) );
    }

#define ASOUND_UPSAMPLE_RATIO ( 1 )

    ASound::Ptr ASoundCache::add( const juce::String& File, double LengthInSeconds )
    {
        if( File.isNotEmpty() )
        {
            if( juce::File( File ).existsAsFile() )
            {
                if( auto ptr = new ASound( File, fmtMgr, ASOUND_UPSAMPLE_RATIO, LengthInSeconds ) )
                {
                    return aSounds.add( ptr );
                }
            }
        }
        return nullptr;
    }

    ASound::Ptr ASoundCache::getBuiltinClick()
    {
        if(!builtinClick) builtinClick = new ASound(fmtMgr, AUDIO_SAMPLE_RATE);
        return builtinClick;
    }

    ASound::Ptr ASoundCache::getOrLoadMetronomeSound( const juce::String& File )
    {
        for( auto it : metronomeSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
        }

        if( File.isNotEmpty() && juce::File( File ).existsAsFile() )
        {
            if( auto ptr = new ASound( File, fmtMgr, ASOUND_UPSAMPLE_RATIO ) )
            {
                return metronomeSounds.add( ptr );
            }
        }
        return nullptr;
    }

#undef ASOUND_UPSAMPLE_RATIO

} // namespace mt
