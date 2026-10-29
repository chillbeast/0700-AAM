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

// Module include files.
namespace kick_ui
{
    class SettingsHardware : public SettingsInterface
    {
    public:
        SettingsHardware();
        void UpdatePadInfo( uint8_t f_velocity, uint8_t f_afterTouch );

        // Virtual functions
        void refresh() override;
        void setupHeader( kick_ui::KickModeHeader& Hdr ) override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void visibilityChanged() override;

    private:
    private:
        int               Initialvelocity_ = 0;
        mt::KickSettings& settings{ mt::getSettings() };

        ui::KickListBox padThreshold{ ui::KickListBox::withTitleBar, "Thresold", { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" }, 8 };
        ui::KickListBox padSensitivity{ ui::KickListBox::withTitleBar, "Sensitivity", { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" }, 8 };
        ui::KickListBox padCurve{ ui::KickListBox::withTitleBar, "Curve", { "1", "2", "3", "4" }, 1 };
        ui::KickSlider  velocity{ "VELOCITY", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider  afterTouch{ "AFTERTOUCH", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        ui::KickSlider brightness{ "BRIGHTNESS", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        ui::KickToggleButton enabled{ "TAP Tempo LED", "ENABLED", "DISABLED" };
        ui::KickListBox      taps{ ui::KickListBox::withTitleBar, "NO OF TAPS", { "2", "4", "8" }, 1 };

        ui::KickNumberT<int> date_DD{ "Day", 1, 31, 1, 1, true };
        ui::KickNumberT<int> date_MM{ "Minute", 1, 12, 1, 1, true };
        ui::KickNumberT<int> date_YYYY{ "Year", 2014, 2099, 2024, 1, true };

        ui::KickNumberT<int> time_HH{ "Hour", 00, 23, 00, 1, true }, time_MM{ "Minute", 0, 59, 00, 1, true };
        ui::KickListBox      timezone{ "timeZone", 4 };

        void updateDateTime();
        void setTime();
        void setDate();
        void SendPadSettings( void );
    };
}
