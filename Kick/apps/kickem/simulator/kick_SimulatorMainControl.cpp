#include "kick_SimulatorMainControl.h"
#include "kick_PanelArtwork.h"

namespace sim
{
    SimulatorMainControl::SimulatorMainControl()
    {
        setInterceptsMouseClicks(false, true);
        std::vector<SimButton*> all;
        collectButtons( all );
        for( auto* b : all )
            addAndMakeVisible( b );
        addAndMakeVisible( jog );

        // Jog wheel: rotation -> EN_KNOB_CENTER knob packets, centre push -> EN_KNOB_ENTER button packets.
        jog.onStep = []( int steps ) {
            hw::send( hw::knobMessage( DCC::EN_KNOB_CENTER, steps ) );
        };
        jog.onCentrePress = []( bool pressed ) {
            hw::send( hw::buttonMessage( DCC::EN_KNOB_ENTER, pressed ? DCC::BTN_PRESSED : DCC::BTN_RELEASED ) );
        };

        // SHIFT latches (click to engage, click again or use any button to release).
        shiftBtn.onPress = [this]( bool pressed ) {
            if( pressed )
                ShiftLatch::setLatched( !ShiftLatch::isLatched() );
        };
    }

    void SimulatorMainControl::collectButtons( std::vector<SimButton*>& out )
    {
        out.insert( out.end(), { &mainBtn, &browseBtn, &trackMixBtn, &trackMuteBtn, &nextSeqBtn, &stepSeqBtn, &tapTempoBtn, &noteRepeatBtn,
                                 &bankA, &bankB, &bankC, &bankD,
                                 &backBtn, &copyBtn, &minusBtn, &plusBtn,
                                 &tcBtn, &samplerBtn, &sampleEditBtn, &progEditBtn, &menuBtn, &shiftBtn } );
    }

    void SimulatorMainControl::paint( juce::Graphics& g )
    {
        if(artwork) return;
        g.setColour( palette::well );
        g.fillRoundedRectangle( getLocalBounds().toFloat(), 8.0f );

        g.setColour( palette::text );
        g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
        g.drawText( "BANKS", juce::Rectangle<int>( bankA.getX(), 2, bankD.getRight() - bankA.getX(), 16 ), juce::Justification::centred );
    }

    void SimulatorMainControl::resized()
    {
        if(artwork)
        {
            const std::pair<SimButton*, const char*> placements[] = {
                {&mainBtn, "main"}, {&browseBtn, "browse"}, {&trackMixBtn, "track-mix"}, {&trackMuteBtn, "track-mute"},
                {&nextSeqBtn, "next-seq"}, {&stepSeqBtn, "step"}, {&tapTempoBtn, "tap"}, {&noteRepeatBtn, "repeat"},
                {&bankA, "bank-a"}, {&bankB, "bank-b"}, {&bankC, "bank-c"}, {&bankD, "bank-d"},
                {&backBtn, "back"}, {&copyBtn, "copy"}, {&minusBtn, "minus"}, {&plusBtn, "plus"},
                {&tcBtn, "tc"}, {&samplerBtn, "sampler"}, {&sampleEditBtn, "sample-edit"}, {&progEditBtn, "program-edit"},
                {&menuBtn, "menu"}, {&shiftBtn, "shift"}
            };
            for(const auto& placement : placements) placement.first->setBounds(artwork->bounds(placement.second));
            jog.setBounds(artwork->bounds("jog"));
            jog.toFront(false);
            return;
        }
        const int bw = 112, bh = 58, gx = 8, gy = 12;
        const int top = 22;

        auto place = [&]( SimButton& b, int x, int row ) { b.setBounds( x, top + row * ( bh + gy ), bw, bh ); };

        // Left block: 2 columns x 4 rows
        int x0 = 10, x1 = x0 + bw + gx;
        place( mainBtn, x0, 0 );
        place( browseBtn, x0, 1 );
        place( trackMixBtn, x0, 2 );
        place( trackMuteBtn, x0, 3 );
        place( nextSeqBtn, x1, 0 );
        place( stepSeqBtn, x1, 1 );
        place( tapTempoBtn, x1, 2 );
        place( noteRepeatBtn, x1, 3 );

        // Right block: 2 columns x 4 rows
        int x3 = getWidth() - 10 - bw, x2 = x3 - gx - bw;
        place( tcBtn, x2, 0 );
        place( samplerBtn, x2, 1 );
        place( sampleEditBtn, x2, 2 );
        place( progEditBtn, x2, 3 );
        place( menuBtn, x3, 0 );
        place( shiftBtn, x3, 3 );

        // Centre block
        int cx0 = x1 + bw + 30, cx1 = x2 - 30, cw = cx1 - cx0;
        int bankW = ( cw - 3 * gx ) / 4;
        int i     = 0;
        for( auto* b : getBankButtons() )
            b->setBounds( cx0 + i++ * ( bankW + gx ), top, bankW, 48 );

        int navTop = top + 48 + 14;
        int navH   = getHeight() - navTop - 8;
        int side   = 92;
        backBtn.setBounds( cx0, navTop + 6, side, 52 );
        copyBtn.setBounds( cx0, navTop + navH - 58, side, 52 );
        plusBtn.setBounds( cx1 - side, navTop + 6, side, 52 );
        minusBtn.setBounds( cx1 - side, navTop + navH - 58, side, 52 );

        int jogSize = juce::jmin( navH, cw - 2 * side - 20 );
        jog.setBounds( juce::Rectangle<int>( jogSize, jogSize ).withCentre( { cx0 + cw / 2, navTop + navH / 2 } ) );
    }

    void SimulatorMainControl::setPanelArtwork(const PanelArtwork* panel)
    {
        artwork = panel;
        std::vector<SimButton*> buttons;
        collectButtons(buttons);
        for(auto* b : buttons) b->setHardwareSkin(artwork != nullptr);
        jog.setHardwareSkin(artwork != nullptr);
        resized();
        repaint();
    }
}
