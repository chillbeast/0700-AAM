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
#include "kick_ui/audio_components/gridview/kick_GridData.h"

namespace kick_ui
{
    class ListEditTimingCorrect : public juce::Component
    {
    public:
        ListEditTimingCorrect();
        ~ListEditTimingCorrect() = default;
        mt::TimingCorrect getOptions();
        static void show();

    private:
        ui::KickToggleOptions globalTC{ "GLOBAL TIMING CORRECT", { "Off", "On" } };
        ui::KickLabel         timeDivisionLabel{ "TIME DIVISION" };
        ui::KickListBox       timeDivList{ ui::KickListBox::withoutTitleBar, "", CommonUtils::getDefaultTimeDivisions(), 3 };
        ui::KickImageButton   triplet{ "TRIPLET" };
        ui::KickListBox       type{ "TYPE", { "Start", "End", "Length", "Legato" } };
        ui::KickListBox       events{ "EVENTS", { "All", "Selected", "Range" } };
        ui::KickNumber        fromBar{"FROM BAR", 1, 999, 1}, toBar{"THROUGH BAR", 1, 999, 1};
        mt::MTSelected target{mt::getAudioInterface().getSelected()};
        ui::KickSlider        swing{ "SWING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        shiftTiming{ "SHIFT  TIMING", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        window{ "WINDOW", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickSlider        strength{ "STRENGTH", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };

        void resized() override;
    };

    class ListEditMode : public IScreen
        , public juce::Component
    {
    public:
        ListEditMode();
        ~ListEditMode();

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void knobCommand( int knob, int value );

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& sel{ mt::getAudioInterface().getSelected() };
        AppStatusbar&         statusBar{ AppStatusbar::Instance() };

        KickModeHeader         header{ "List Edit", "Icon_listEdit" };
        HeaderBarTrackSelector trackSelector;
        GridSettings&          gridSettings{ GridSettings::Instance() };
        GridData&              gridData{ GridData::Instance() };

        std::unique_ptr<ui::KickTableView> tableView;
        ui::KickTableViewProvider          provider;

        ui::KickListBox      filterListBox{ ui::KickListBox::withTitleBar, "FILTER", {} };
        ui::KickToggleButton snapButton{ "Snap:Off", "Snap:On", "Snap:Off" };
        ui::KickToggleButton deleteButton{ "Delete", "Delete", "Delete" };
        ui::KickToggleButton insertButton{ "Insert", "Insert", "Insert" };
        ui::KickButton       tcButton{ "TC", "TC" };

        std::vector<juce::String>                   headerTitles{ "Event", "Time", "Pad/Note", "Length", "Velocity", "Mod Type", "Value", "Prob", "Rat" };
        std::vector<std::unique_ptr<ui::KickLabel>> tableHeaderData;
        int                                         numNotes{ 0 };
        juce::ValueTree                             noteList;

        void refreshData();
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
        void markSelectedRows();
        void updateViewPosition();
    };

    class ListEntry : public juce::Component
    {
    public:
        ListEntry( int Row, juce::ValueTree Vt );

        void                 setValues( int Row, juce::ValueTree Vt );
        void                 refreshData();
        void                 setSelected( bool Select );
        void                 linkRotated( int ID, int value );
        juce::Rectangle<int> getElementBounds( int state );

    private:
        juce::ValueTree    vt;
        bool               isSelected{ false };
        ui::KickTitleLabel time{ "" };
        ui::KickListBox    note{ ui::KickListBox::withoutTitleBar, "", mt::Kick::padNames };
        ui::KickNumber     length{ "Length", 1, 9999, 1, 1, false }, velocity{ "Velocity", MIDI::MIN, MIDI::MAX, 1, 1, false }, rat{ "Rat", 1, 8, 0, 1, false }, prob{ "Probability", 0, 100, 100, 1, false }, value{ "Value", MIDI::MIN, MIDI::MAX, 1, 1, false };
        ui::KickLabel      row{ "" }, type{ "" };

        void resized() override;
    };

    class ListEditInsertDialog : public juce::Component
    {
    public:
        struct ListInsertOptions
        {
            int          bar;
            int          beats;
            int          tick;
            juce::String sample;
            int          padIndex;
        };

        int getStartBar() const { return bar.getValue(); }
        int getStartBeat() const { return beat.getValue(); }
        int getStartTick() const { return tick.getValue(); }

        ListEditInsertDialog()
        {
            mt::utils::addAndMakeVisible( this, bar, beat, tick, barLabel, beatLabel, tickLabel, typeLabel, padLabel, typeList, padsList );

            mt::utils::setValueColour( ui::Theme::textColour( 1 ), barLabel, beatLabel, tickLabel, typeLabel, padLabel );
            mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), barLabel, beatLabel, tickLabel, typeLabel, padLabel );
            mt::utils::setValueFontHeight( 16, barLabel, beatLabel, tickLabel, typeLabel, padLabel, bar, beat, tick );
            mt::utils::setJustificationType( juce::Justification::centred, padsList, typeList );

            setName( "INSERT EVENT" );

            auto numBars = ae.getSequenceLength( ae.getSelectedSequence() );
            bar.setRange( 1, numBars, 1 );
            beat.setRange( 1, gridData.getBeatsPerBar( numBars ), 1 );
            tick.setRange( 1, ( gridData.getTicksPerBar( numBars ) ) / beat.getMax(), 1 );

            bar.onValueChanged          = [&]( int Value ) { info.bar = Value; };
            beat.onValueChanged         = [&]( int Value ) { info.beats = Value; };
            tick.onValueChanged         = [&]( int Value ) { info.tick = Value; };
            padsList.onSelectionChanged = [&]( int Index, const juce::String& value ) { info.padIndex = Index; };
            typeList.onSelectionChanged = [&]( int Index, const juce::String& Value ) {};

            padsList.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

            int letterAsciiCode = 65;
            padData.clear();
            for( int i = 0; i <= kick_Constants::totalPianoKeys; i += 16 )
            {
                for( int j = 0; j < 16; j++ )
                {
                    juce::String noteLabel;
                    noteLabel = noteLabel.formatted( "%c%02d", letterAsciiCode, ( j + 1 ) );
                    padData.push_back( noteLabel );
                }
                letterAsciiCode++;
            }
            padsList.setListOptions( padData );
            padsList.setSelected( 0, juce::sendNotification );
        }

        ~ListEditInsertDialog() {}
        ListInsertOptions& getData() { return info; }

    private:
        ListInsertOptions         info;
        mt::MTAudioInterface&     ae{ mt::getAudioInterface() };
        GridData&                 gridData{ GridData::Instance() };
        std::vector<juce::String> padData;
        ui::KickListBox           padsList{ ui::KickListBox::withoutTitleBar, "", {} };
        ui::KickNumber            bar{ "", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            beat{ "", BAR::MIN, BAR::MAX, 1 };
        ui::KickNumber            tick{ "", BAR::MIN, BAR::MAX, 1 };
        ui::KickListBox           typeList{ ui::KickListBox::withoutTitleBar, "", { "Notes", "Program Automation", "Program Mixer Automation", "Track Automation" } };
        ui::KickLabel             barLabel{ "BAR" }, beatLabel{ "BEAT" }, tickLabel{ "TICK" }, typeLabel{ "TYPE" }, padLabel{ "PAD" };
        int                       barValue, beatValue, tickValue;

        void resized() override
        {
            barLabel.setBoundsRelative( .083f, .052f, .167f, .088f );
            beatLabel.setBoundsRelative( .083f, .224f, .167f, .088f );
            tickLabel.setBoundsRelative( .083f, .395f, .167f, .088f );
            typeLabel.setBoundsRelative( .083f, .656f, .167f, .12f );
            padLabel.setBoundsRelative( .083f, .828f, .167f, .12f );

            bar.setBoundsRelative( .334f, .052f, .582f, .12f );
            beat.setBoundsRelative( .334f, .224f, .582f, .12f );
            tick.setBoundsRelative( .334f, .395f, .582f, .12f );
            typeList.setBoundsRelative( .334f, .656f, .582f, .12f );
            padsList.setBoundsRelative( .334f, .828f, .582f, .12f );
        }
    };

}
