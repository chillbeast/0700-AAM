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

#include <climits>

namespace mt
{
    class ASoundCache
    {
    public:
        ASoundCache();
        ~ASoundCache();

        void clear();
        void purge();

        ASound::Ptr exists( const juce::String& File );
        ASound::Ptr getOrLoad( const juce::String& File, double LengthInSeconds = -1 );
        ASound::Ptr add( const juce::String& File, double LengthInSeconds = -1 );
        void        remove( const juce::String& File );

        ASound::Ptr getOrLoadMetronomeSound( const juce::String& File );
        ASound::Ptr getBuiltinClick();

    private:
        juce::AudioFormatManager            fmtMgr;
        juce::ReferenceCountedArray<ASound> aSounds;
        juce::ReferenceCountedArray<ASound> metronomeSounds;
        ASound::Ptr builtinClick;
    };

    /// API to obtain static instance of all currently loaded Sound sample cache.
    ASoundCache& getAudioSoundCache();

} // namespace mt
