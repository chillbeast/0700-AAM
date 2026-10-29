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
    //
    struct MidiDeviceListEntry : juce::ReferenceCountedObject
    {
        explicit MidiDeviceListEntry( juce::MidiDeviceInfo info )
            : deviceInfo( info ) {}

        bool                              isInput{ false };
        juce::MidiDeviceInfo              deviceInfo;
        std::unique_ptr<juce::MidiInput>  inDevice;
        std::unique_ptr<juce::MidiOutput> outDevice;

        using Ptr = juce::ReferenceCountedObjectPtr<MidiDeviceListEntry>;
    };

    struct MTMidiMessage : juce::ReferenceCountedObject
    {
        juce::MidiInput*  source{nullptr};
        double receivedAtMilliseconds{0.0};
        juce::String sourceName; // stable after the input device is unplugged
        juce::String sourceIdentifier; // distinguish controllers with the same display name
        juce::MidiMessage message;
        using Ptr = juce::ReferenceCountedObjectPtr<MTMidiMessage>;
    };
    //

} // namespace MT
