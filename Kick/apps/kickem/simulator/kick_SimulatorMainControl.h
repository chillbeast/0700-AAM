#pragma once

#include "kick_SimulatorControls.h"

namespace sim
{
    ///
    /// Control section below the screen: mode buttons, BANK A-D, BACK/COPY/-/+, jog wheel (with ENTER).
    /// Logical size 1024 x 300.
    ///
    class SimulatorMainControl : public juce::Component
    {
    public:
        SimulatorMainControl();

        void resized() override;
        void paint( juce::Graphics& g ) override;

        void collectButtons( std::vector<SimButton*>& out );
        std::array<SimButton*, 4> getBankButtons() { return { &bankA, &bankB, &bankC, &bankD }; }
        SimButton&                getShiftButton() { return shiftBtn; }
        SimKnob&                  getJog() { return jog; }
        void setPanelArtwork(const PanelArtwork* panel);

    private:
        const PanelArtwork* artwork { nullptr };
        // Left block
        SimButton mainBtn { "MAIN", DCC::EN_MAINMODE, "GRID", DCC::EN_GRID };
        SimButton browseBtn { "BROWSE", DCC::EN_BROWSE, "SAVE", DCC::EN_SAVE };
        SimButton trackMixBtn { "TRACK MIX", DCC::EN_TRACK_MIX, "PAD MIX", DCC::EN_PAD_MIX };
        SimButton trackMuteBtn { "TRACK MUTE", DCC::EN_TRACK_MUTE, "PAD MUTE", DCC::EN_PAD_MUTE };
        SimButton nextSeqBtn { "NEXT SEQ", DCC::EN_NEXT_SEQ, "FX", DCC::EN_FX };
        SimButton stepSeqBtn { "STEP SEQ", DCC::EN_STEP_SEQ, "AUTOMATION", DCC::EN_AUTOMATION };
        SimButton tapTempoBtn { "TAP TEMPO", DCC::EN_TAP_TEMPO, "MASTER", DCC::EN_MASTER };
        SimButton noteRepeatBtn { "NOTE REPEAT", DCC::EN_NOTE_REPEAT, "LATCH", DCC::EN_LATCH };

        // Banks (SHIFT -> E-H)
        SimButton bankA { "A", DCC::EN_BANK_A, "E", DCC::EN_BANK_E };
        SimButton bankB { "B", DCC::EN_BANK_B, "F", DCC::EN_BANK_F };
        SimButton bankC { "C", DCC::EN_BANK_C, "G", DCC::EN_BANK_G };
        SimButton bankD { "D", DCC::EN_BANK_D, "H", DCC::EN_BANK_H };

        // Navigation around the jog wheel
        SimButton backBtn { "BACK", DCC::EN_BACK };
        SimButton copyBtn { "COPY", DCC::EN_COPY };
        SimButton minusBtn { "-", DCC::EN_MINUS };
        SimButton plusBtn { "+", DCC::EN_PLUS };
        SimKnob   jog { {}, SimKnob::Mode::endless, true };

        // Right block
        SimButton tcBtn { "TC", DCC::EN_TC, "ON/OFF", DCC::EN_ON_OFF };
        SimButton samplerBtn { "SAMPLER", DCC::EN_SAMPLER, "LOOPER", DCC::EN_LOOPER };
        SimButton sampleEditBtn { "SAMPLE EDIT", DCC::EN_SAMPLE_EDIT, "LINK EDIT", DCC::EN_LINK_EDIT };
        SimButton progEditBtn { "PROGRAM EDIT", DCC::EN_PROG_EDIT, "MIDI CONTROL", DCC::EN_MIDI_CONTROL };
        SimButton menuBtn { "MENU", DCC::EN_MENU, "PREFS", DCC::EN_PREF };
        SimButton shiftBtn { "SHIFT", DCC::EN_SHIFT };
    };
}
