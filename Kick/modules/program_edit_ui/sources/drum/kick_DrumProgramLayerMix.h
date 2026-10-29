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

namespace kick_ui
{
    class DPLayerMix : public ui::KickGrid
        , public DrumInterface
    {
    public:
        DPLayerMix( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum );
        ~DPLayerMix() = default;

        void refresh() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void refreshPads();

    private:
        std::vector<int>&   pads;
        mt::MTDrumProgram*& pDrum;

        std::array<ui::KickRotary, 4> semi;
        std::array<ui::KickRotary, 4> fine;
        std::array<ui::KickRotary, 4> amp;
        std::array<ui::KickRotary, 4> pan;
        std::array<ui::KickRotary, 4> velocityLow, velocityHigh;


        void visibilityChanged() override;
    };
}
