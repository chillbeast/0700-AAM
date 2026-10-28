/*
 * Copyright (c) 2024 MUSIC Tribe
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
    ///
    /// ASoundPlayer
    ///
    ASoundPlayer::ASoundPlayer()
    {
        audioFmtMgr.registerBasicFormats();
        ASliceRenderer::setSlice( &slice );

        // `sound` is a member, not heap-allocated, but the slice holds it through a
        // ReferenceCountedObjectPtr. Keep our own reference so the count never drops
        // to zero, otherwise releasing the slice would `delete` a member object.
        sound.incReferenceCount();
        slice.setSource( &sound );
    }

    ASoundPlayer::~ASoundPlayer()
    {
        // The renderer owns a source snapshot: release it while our embedded
        // sound is still alive, before member and then base destruction.
        ASliceRenderer::setSlice( nullptr );
        slice.setSource( nullptr );
        sound.decReferenceCountWithoutDeleting();
    }

    void ASoundPlayer::setSoundToPlay( const juce::String& File, double LengthInSeconds )
    {
        ASliceRenderer::setSlice( nullptr );
        sound.setSound( File, LengthInSeconds );

        slice.refresh();
        slice.setLoopPoints( 0, sound.getNumSamplesPerChannel() );

        ASliceRenderer::setSlice( &slice );
    }

    void ASoundPlayer::removeSound()
    {
        ASliceRenderer::setSlice( nullptr );

        // ASound::purge() only frees the data when nothing references it, so
        // detach the slice and drop our own reference around the purge.
        slice.setSource( nullptr );
        sound.decReferenceCountWithoutDeleting();
        sound.purge();
        sound.incReferenceCount();

        // Keep the slice bound, but leave the renderer detached so purging does
        // not immediately reload the data. The next load binds its new snapshot.
        slice.setSource( &sound );
    }

    bool ASoundPlayer::isValidSource()
    {
        return sound.isValid();
    }

} // namespace mt
