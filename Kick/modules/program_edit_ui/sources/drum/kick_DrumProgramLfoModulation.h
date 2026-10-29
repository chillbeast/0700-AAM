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
    class DPLfoModulation : public ui::KickGrid
        , public DrumInterface
    {
    public:
        DPLfoModulation( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum );
        ~DPLfoModulation() = default;

        void refresh() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void refreshPads();

    private:
        ui::KickButton enabled{"LFO ON"};

        std::vector<int>&   pads;
        mt::MTDrumProgram*& pDrum;

        ui::KickListBox     lfoType{ "WAVEFORM", { "SINE", "TRIANGLE", "SAW_UP", "SAW_DOWN", "SQUARE", "NOISE", "SAMPLE_HOLD" }, 0, 1 };
        ui::KickImageButton num{ "1", "num_Header" };
        ui::KickRotary      rate{ "RATE", juce::Slider::TextBoxAbove, true };
        ui::KickRotary      pitch{ "PITCH", juce::Slider::TextBoxAbove, true };
        ui::KickRotary      filter{ "FILTER", juce::Slider::TextBoxAbove, true };
        ui::KickRotary      amp{ "AMP", juce::Slider::TextBoxAbove, true };
        ui::KickRotary      pan{ "PAN", juce::Slider::TextBoxAbove, true };

        void visibilityChanged() override;
    };

    class DrumGroupModulation : public juce::Component
        , public DrumInterface
    {
    public:
        DrumGroupModulation( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum );
        ~DrumGroupModulation() = default;

        void refresh() override {}
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        ui::KickLabel source{ "Source" }, destination{ "Destination" };

        std::array<ui::KickListBox, 4> listBox;

        std::unique_ptr<ui::KickNumber> lfos[4];
        std::unique_ptr<ui::KickNumber> filters[4];
        std::unique_ptr<ui::KickNumber> envs[4];
        std::unique_ptr<ui::KickNumber> customs[4];

        mt::MTDrumProgram* pDrum{ nullptr };
        std::vector<int>   pads;

        void paint( juce::Graphics& ) override;
        void resized() override;
    };
}
