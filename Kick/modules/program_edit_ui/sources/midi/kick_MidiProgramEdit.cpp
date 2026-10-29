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

namespace kick_ui
{
    MidiProgramEdit::MidiProgramEdit()
        : ProgEditInterface( mt::MTProgramType::Midi )
    {
        // Add Sliders and create attachements...
        int sliderIdx = 0;
        for( auto& it : sliders )
        {
            juce::String name = juce::MidiMessage::getControllerName( sliderIdx );
            if( 0 >= name.length() )
                name = "CC" + juce::String( sliderIdx );

            it.setTitle( name );
            it.setComponentID("midi-cc-" + juce::String(sliderIdx));
            it.setRange( MIDI::MIN, MIDI::MAX, 1 );
            it.showTitleAndValue( true, juce::Slider::TextBoxBelow );
            it.setTitleJustification( juce::Justification::centredTop );
            it.setValueJustification( juce::Justification::centred );
            attachments[sliderIdx] = std::move( std::make_unique<ui::SliderAttachment>( it ) );
            addAndMakeVisible( it );

            ++sliderIdx;
        }

        // Add selector buttons...
        sliderIdx = 1;
        for( auto& it : selectors )
        {
            auto name = juce::String( sliderIdx );
            it.setName( name );
            it.setTitle( name );
            it.setImages( "progedit/default/" + name + ".svg" );
            it.setOverlayColours( ui::Theme::highlightColour().withMultipliedAlpha( 0.4f ) );
            it.setRadioGroupId( (int)RadioGroupId::MidiProgram, juce::dontSendNotification );
            addAndMakeVisible( it );

            it.onClick = [&, sliderIdx]() {
                auto state = selectors[( sliderIdx - 1 )].getToggleState();
                if( state )
                {
                    // Control visibility...
                    for( auto& it : sliders )
                        it.setVisible( false );
                    //
                    for( int i = ( sliderIdx - 1 ) * 16; i < ( sliderIdx * 16 ); ++i )
                        sliders[i].setVisible( true );
                }
            };

            ++sliderIdx;
        }
        selectors[0].setToggleState( true, juce::sendNotification );

        // Program change handler...
        onProgramChanged = [&]() {
            state = pIProg && pIProg->isMidi() ? pIProg->getState() : juce::ValueTree{};
            juce::String id;
            int          idx = 0;
            for( auto& it : attachments )
            {
                id = "CC" + juce::String( idx++ );
                it->resetAttachment(state.isValid() ? state.getPropertyAsValue(id, pIProg->getUndoManager()) : juce::Value{});
            }
            setEnabled(state.isValid());
        };

        ui::connectTheme( *this, [&]() {
            auto id = 0;
            for( auto& it : selectors )
            {
                auto name = juce::String( id );
                it.setImages( "progedit/default/" + name + ".svg" );
            }
        } );
    }

    MidiProgramEdit::~MidiProgramEdit()
    {
        stopTimer();
    }

    void MidiProgramEdit::refresh()
    {
    }

    void MidiProgramEdit::handleCommand( CPktMgr::HWMsg_t& message )
    {
        if(!state.isValid() || !contracts::DclContractsHelper::isKnobMessage(message)) return;
        const int knob = contracts::DclContractsHelper::getKnobCode(message) - contracts::DeviceControlContracts::EN_KNOB_1;
        if(!juce::isPositiveAndBelow(knob, 16)) return;
        for(int page = 0; page < 8; ++page)
            if(selectors[page].getToggleState())
                sliders[page * 16 + knob].knobRotated(contracts::DclContractsHelper::getKnobValue(message));
    }

    void MidiProgramEdit::timerCallback()
    {
    }

    void MidiProgramEdit::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds().reduced( 9, 10 ).toFloat();
        b.removeFromTop( 54 );
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( b, 5.f );
    }

    void MidiProgramEdit::resized()
    {
        auto b = getLocalBounds();

        auto by = ( 10.f / 458.f );
        auto bw = ( 122.f / 1022.f );
        auto bh = ( 44.f / 458.f );
        selectors[0].setBoundsRelative( ( 9.f / 1022.f ), by, bw, bh );
        selectors[1].setBoundsRelative( ( 135.f / 1022.f ), by, bw, bh );
        selectors[2].setBoundsRelative( ( 261.f / 1022.f ), by, bw, bh );
        selectors[3].setBoundsRelative( ( 387.f / 1022.f ), by, bw, bh );
        selectors[4].setBoundsRelative( ( 513.f / 1022.f ), by, bw, bh );
        selectors[5].setBoundsRelative( ( 639.f / 1022.f ), by, bw, bh );
        selectors[6].setBoundsRelative( ( 765.f / 1022.f ), by, bw, bh );
        selectors[7].setBoundsRelative( ( 891.f / 1022.f ), by, bw, bh );

        auto x1 = ( 62.f / 1022.f );
        auto x2 = ( 308.f / 1022.f );
        auto x3 = ( 554.f / 1022.f );
        auto x4 = ( 800.f / 1022.f );

        auto y1 = ( 72.f / 458.f );
        auto y2 = ( 166.f / 458.f );
        auto y3 = ( 260.f / 458.f );
        auto y4 = ( 354.f / 458.f );

        auto sw = ( 160.f / 1022.f );
        auto sh = ( 84.f / 458.f );

        for( int i = 0; i < 8; ++i )
        {
            int sliderIdx = i * 16;
            sliders[sliderIdx++].setBoundsRelative( x1, y4, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x2, y4, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x3, y4, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x4, y4, sw, sh );

            sliders[sliderIdx++].setBoundsRelative( x1, y3, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x2, y3, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x3, y3, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x4, y3, sw, sh );

            sliders[sliderIdx++].setBoundsRelative( x1, y2, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x2, y2, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x3, y2, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x4, y2, sw, sh );

            sliders[sliderIdx++].setBoundsRelative( x1, y1, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x2, y1, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x3, y1, sw, sh );
            sliders[sliderIdx++].setBoundsRelative( x4, y1, sw, sh );
        }
    }

    void MidiProgramEdit::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
        {
            auto selected = ae.getSelectedProgram();

            startTimerHz( 20 );
        }
        else
            stopTimer();
    }
}
