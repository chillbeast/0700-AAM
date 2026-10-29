/*
 * Copyright (c) 2022 MUSIC Tribe
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

#ifdef ENGINE_H_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
     already included any other headers - just put it inside a file on its own, possibly with your config
     flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
     header files that the compiler may be using.
  */
    #error "Incorrect use of JUCE cpp file"
#endif

// Module include files...
#include "audio_engine.h"

// Module internal include files...

// Module source files...
#include "engine/mt_audio_interface.cpp"
#include "engine/mt_audio_performance.cpp"
#include "engine/mt_audio_inserts_types.cpp"
#include "engine/mt_audio_interface_types.cpp"
#include "engine/mt_audio_interface_looper.cpp"

#include "engine/mt_audio_manager.cpp"
#include "engine/mt_audio_manager_program.cpp"
#include "engine/mt_audio_manager_sequence.cpp"
#include "engine/mt_audio_manager_transport.cpp"
#include "engine/mt_audio_bus_manager.cpp"
#include "engine/mt_audio_note.cpp"
#include "engine/mt_audio_sequence.cpp"
#include "engine/mt_audio_track.cpp"
#include "engine/mt_audio_transport.cpp"

// Sample Editor...
#include "engine/mt_sample_editor_interface.cpp"
#include "engine/mt_sample_editor_actions.cpp"

// Sample Player...
#include "engine/mt_sample_player.cpp"

// Recorder...
#include "engine/mt_audio_recorder.cpp"
