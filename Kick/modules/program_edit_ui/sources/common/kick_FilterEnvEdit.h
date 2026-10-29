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

#include "kick_SampleVoiceEditor.h"
namespace kick_ui
{
    class FilterEnvEdit : public ui::KickGrid
        , public juce::Button::Listener
    {
    public:
        FilterEnvEdit( std::vector<int>& Pads );
        ~FilterEnvEdit();

        void buttonClicked( juce::Button* ) override;

        void refresh();

    private:
        void visibilityChanged() override
        {
            for( auto it : getChildren() )
                it->setVisible( isVisible() );

            if( isVisible() )
                refresh();
        }

    private:
        std::vector<int>&     pads;
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };

        ui::KickLabel filterLabel{ TRANS( "Filter" ) };
        ui::KickLabel msLabel{ TRANS( "Modulation Sources" ) };
        ui::KickLabel feOptions{ TRANS( "Filter Envelope" ) };
        ui::KickLabel ampEnvOptions{ TRANS( "Amp Envelope" ) };

        ui::KickListBox filterType{ "Type", 1 };
        ui::KickRotary  filterCutoff{ "Cutoff", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  filterReso{ "Reso", juce::Slider::TextBoxLeft, true };
        ui::KickRotary  filterEnv{ "Env", juce::Slider::TextBoxLeft, true };

        ui::KickRotary msVelStat{ "Vel>Sta", juce::Slider::TextBoxLeft, true };
        ui::KickRotary msVelAtk{ "Vel>Atk", juce::Slider::TextBoxLeft, true };
        ui::KickRotary msVelEnv{ "Vel>Envg", juce::Slider::TextBoxLeft, true };
        ui::KickRotary msVelFlt{ "Vel>Flt", juce::Slider::TextBoxLeft, true };

        ui::KickNumber      feAttack{ "Attack", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      feHold{ "Hold", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      feDecay{ "Decay", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      feSustain{ "Sustain", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickImageButton feAhdsrBtn{ "ahdsr", "effect3" };

        ui::KickNumber      ampAttack{ "Attack", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      ampHold{ "Hold", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      ampDecay{ "Decay", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickNumber      ampSustain{ "Sustain", MIDI::MIN, MIDI::MAX, 0, 1, true, false };
        ui::KickImageButton ampAhdsrBtn{ "ahdsr", "effect3" };
    };

    class KickFilterEnvelope : public SampleVoiceEditor, public KeyGroupInterface
    {
    public:
        KickFilterEnvelope(std::vector<int>& Keys, mt::MTKeygroupProgram*& Program);
        void refresh() override { SampleVoiceEditor::refresh(); }
        void refreshPads() { refresh(); }
        void handleCommand(CPktMgr::HWMsg_t& Message) override { SampleVoiceEditor::handleCommand(Message); }
    };
}
