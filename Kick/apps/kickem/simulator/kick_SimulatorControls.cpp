#include <kick_ui/kick_ui.h>
#include "kick_SimulatorControls.h"
#include "kick_PanelArtwork.h"
#include "kick_SimulatorInterfaceImpl.h"

namespace sim
{
    //==============================================================================
    // Hardware packet builders
    //==============================================================================
    namespace hw
    {
        CPktMgr::HWMsg_t buttonMessage( int buttonCode, int btnState )
        {
            CPktMgr::HWMsg_t m;
            m.pktType = CPktMgr::EN_PKT_TYPE_BUTTON;
            m.pktCmd  = DCC::EN_BTN_STATUS;
            m.pktDir  = CPktMgr::EN_PKT_REQUEST;
            m.data    = { buttonCode, btnState };
            return m;
        }

        CPktMgr::HWMsg_t padMessage( int pad, int btnState, int velocity, int aftertouch )
        {
            CPktMgr::HWMsg_t m;
            m.pktType = CPktMgr::EN_PKT_TYPE_PAD;
            m.pktCmd  = DCC::EN_PAD_STATE;
            m.pktDir  = CPktMgr::EN_PKT_REQUEST;
            m.data    = { pad, btnState, velocity, aftertouch };
            return m;
        }

        CPktMgr::HWMsg_t knobMessage( int knob, int steps )
        {
            CPktMgr::HWMsg_t m;
            m.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
            m.pktCmd  = DCC::EN_KNOB_ROTARY;
            m.pktDir  = CPktMgr::EN_PKT_REQUEST;
            m.data    = { knob, steps >= 0 ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE, std::abs( steps ), 0 };
            return m;
        }

        CPktMgr::HWMsg_t sliderMessage( int value )
        {
            CPktMgr::HWMsg_t m;
            m.pktType = CPktMgr::EN_PKT_TYPE_SLIDER;
            m.pktCmd  = DCC::EN_SLIDER_TOUCH_INPUT;
            m.pktDir  = CPktMgr::EN_PKT_REQUEST;
            m.data    = { value };
            return m;
        }

        void send( const CPktMgr::HWMsg_t& msg )
        {
            SimulatorInterfaceImpl::injectInput( msg );
        }

        juce::Colour padColour( int colourId )
        {
            if( colourId < 0 || colourId > DCC::PadColourNeutralGrey || colourId == DCC::PadColourTrans )
                return juce::Colours::transparentBlack;

            auto c = ui::Theme::padButtonColour( colourId );
            if( c.isTransparent() )
            {
                // Theme not loaded: fall back to a rough palette.
                static const juce::uint32 fallback[] = { 0xff8b0000, 0xff9f00c5, 0xffb5a300, 0xff90ee90, 0xff3cb371, 0xff2e8b57,
                                                         0xff87cefa, 0xff00ffff, 0xff1e90ff, 0xffff69b4, 0xffff1493, 0xffdb7093,
                                                         0xffff0000, 0xffffa500, 0xffff8c00, 0x00000000, 0xfff5f5f5, 0xff808080 };
                c = juce::Colour( fallback[colourId] );
            }
            return c.withAlpha( 1.0f );
        }
    }

    //==============================================================================
    // SimButton
    //==============================================================================
    SimButton::SimButton( juce::String l, int c, juce::String sl, int sc )
        : label( std::move( l ) ), shiftLabel( std::move( sl ) ), code( c ), shiftCode( sc )
    {
        setMouseClickGrabsKeyboardFocus( false );
        setWantsKeyboardFocus( false );
        setRepaintsOnMouseActivity( true );
        setTooltip(label + (shiftLabel.isNotEmpty() ? " / Shift: " + shiftLabel : juce::String()));
    }

    void SimButton::setLed( bool on )
    {
        if( led != on )
        {
            led = on;
            repaint();
        }
    }

    void SimButton::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds().toFloat().reduced( 1.5f );
        if(hardwareSkin)
        {
            const auto colour = led ? ledColour : juce::Colours::white;
            g.setColour(colour.withAlpha(down ? 0.3f : (isMouseOver() ? 0.12f : 0.f)));
            if(roundHardware) g.fillEllipse(b); else g.fillRoundedRectangle(b, 2.f);
            if(led || down)
            {
                g.setColour(colour.withAlpha(led ? 0.9f : 0.5f));
                if(roundHardware) g.drawEllipse(b, 2.5f); else g.drawRoundedRectangle(b, 2.f, 2.5f);
            }
            return;
        }
        g.setColour( down ? palette::keyDown : ( isMouseOver() ? palette::key.brighter( 0.08f ) : palette::key ) );
        g.fillRoundedRectangle( b, 5.0f );
        g.setColour( led ? ledColour : palette::outline );
        g.drawRoundedRectangle( b, 5.0f, led ? 2.5f : 1.0f );

        // LED dot
        auto dot = juce::Rectangle<float>( 7.0f, 7.0f ).withPosition( b.getRight() - 12.0f, b.getY() + 5.0f );
        g.setColour( led ? ledColour : juce::Colour( 0xff151515 ) );
        g.fillEllipse( dot );

        auto textArea = b.reduced( 4.0f, 2.0f );
        if( shiftLabel.isNotEmpty() )
        {
            auto top = textArea.removeFromTop( textArea.getHeight() * 0.58f );
            g.setColour( palette::text );
            g.setFont( juce::Font( juce::jmin( 15.0f, top.getHeight() * 0.6f ), juce::Font::bold ) );
            g.drawFittedText( label, top.toNearestInt(), juce::Justification::centredBottom, 1 );
            g.setColour( ShiftLatch::isLatched() ? palette::shiftText : palette::shiftText.withAlpha( 0.6f ) );
            g.setFont( juce::Font( juce::jmin( 12.0f, textArea.getHeight() * 0.8f ) ) );
            g.drawFittedText( shiftLabel, textArea.toNearestInt(), juce::Justification::centredTop, 1 );
        }
        else
        {
            g.setColour( palette::text );
            g.setFont( juce::Font( juce::jmin( 15.0f, textArea.getHeight() * 0.45f ), juce::Font::bold ) );
            g.drawFittedText( label, textArea.toNearestInt(), juce::Justification::centred, 2 );
        }
    }

    void SimButton::mouseDown( const juce::MouseEvent& e ) { if(!interceptMapping(e)) press( true, e.mods.isShiftDown() ); }
    void SimButton::mouseUp( const juce::MouseEvent& ) { if(!mappingGesture) press( false ); mappingGesture = false; }

    void SimButton::press( bool isDown, bool shiftModifier )
    {
        if( isDown == down )
            return;
        down = isDown;
        repaint();

        if( onPress )
        {
            onPress( isDown );
            return;
        }

        if( isDown )
        {
            const bool shifted = ShiftLatch::isLatched() || shiftModifier;
            sentCode           = ( shifted && shiftCode != DCC::EN_INVALID_ID ) ? shiftCode : code;
            hw::send( hw::buttonMessage( sentCode, DCC::BTN_PRESSED ) );
            return;
        }

        if( sentCode != DCC::EN_INVALID_ID )
            hw::send( hw::buttonMessage( sentCode, DCC::BTN_RELEASED ) );
        sentCode = DCC::EN_INVALID_ID;

        // One-shot shift: release a latched SHIFT after it has been used.
        if( ShiftLatch::isLatched() )
            ShiftLatch::setLatched( false );
    }

    //==============================================================================
    // ShiftLatch
    //==============================================================================
    static bool                  shiftLatched = false;
    static bool                  shiftHeld = false;
    std::function<void( bool )> ShiftLatch::onChange;

    bool ShiftLatch::isLatched() { return shiftLatched || shiftHeld; }

    void ShiftLatch::setLatched( bool latched )
    {
        const bool before = isLatched();
        shiftLatched = latched;
        const bool active = isLatched();
        if(before == active) return;
        hw::send( hw::buttonMessage( DCC::EN_SHIFT, active ? DCC::BTN_PRESSED : DCC::BTN_RELEASED ) );
        if( onChange )
            onChange( active );
    }

    void ShiftLatch::setHeld(bool held)
    {
        const bool before = isLatched();
        shiftHeld = held;
        const bool active = isLatched();
        if(before == active) return;
        hw::send(hw::buttonMessage(DCC::EN_SHIFT, active ? DCC::BTN_PRESSED : DCC::BTN_RELEASED));
        if(onChange) onChange(active);
    }

    //==============================================================================
    // SimKnob
    //==============================================================================
    static constexpr double degreesPerStep = 15.0; // one encoder detent
    static constexpr int    pixelsPerStep  = 6;

    SimKnob::SimKnob( juce::String l, Mode m, bool angular )
        : label( std::move( l ) ), mode( m ), angularDrag( angular )
    {
        setMouseClickGrabsKeyboardFocus( false );
        setWantsKeyboardFocus( false );
        setTooltip(mode == Mode::absolute ? label + ": analog gain on the hardware; visual reference in the simulator"
            : label.isNotEmpty() ? label + ": drag vertically or scroll to turn"
            : "Jog: drag around the wheel or scroll to turn; click centre for Enter");
        if( mode == Mode::absolute )
            angle = -135.0 + value * 270.0;
    }

    juce::Rectangle<float> SimKnob::knobArea() const
    {
        auto b    = getLocalBounds().toFloat();
        if(hardwareSkin) return b.reduced(2.f);
        auto text = label.isNotEmpty() ? juce::jmin( 20.0f, b.getHeight() * 0.18f ) : 0.0f;
        b.removeFromBottom( text );
        auto side = juce::jmin( b.getWidth(), b.getHeight() ) - 6.0f;
        return juce::Rectangle<float>( side, side ).withCentre( b.getCentre() );
    }

    bool SimKnob::inCentre( juce::Point<float> p ) const
    {
        auto k = knobArea();
        return onCentrePress && p.getDistanceFrom( k.getCentre() ) < k.getWidth() * 0.2f;
    }

    bool SimKnob::hitTest(int x, int y)
    {
        if(!hardwareSkin) return true;
        const auto k = knobArea();
        return juce::Point<float>((float)x, (float)y).getDistanceFrom(k.getCentre()) <= k.getWidth() * 0.5f;
    }

    void SimKnob::paint( juce::Graphics& g )
    {
        auto k = knobArea();
        auto c = k.getCentre();
        auto r = k.getWidth() * 0.5f;

        if(hardwareSkin)
        {
            const auto a = juce::degreesToRadians((float)angle);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawLine({c.getPointOnCircumference(r * 0.65f, a), c.getPointOnCircumference(r * 0.91f, a)}, 2.5f);
            if(onCentrePress)
            {
                const auto cap = juce::Rectangle<float>(r * 0.6f, r * 0.6f).withCentre(c);
                g.setColour(centrePressed ? palette::led : juce::Colour(0xff19191b));
                g.fillEllipse(cap);
                g.setColour(palette::text);
                g.setFont(juce::Font(10.f, juce::Font::bold));
                g.drawText("ENTER", cap.expanded(5.f, 0.f), juce::Justification::centred);
            }
            return;
        }

        if( mode == Mode::absolute )
        {
            juce::Path arc;
            arc.addCentredArc( c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::degreesToRadians( -135.0f ), juce::degreesToRadians( (float)angle ), true );
            g.setColour( palette::led );
            g.strokePath( arc, juce::PathStrokeType( 3.0f ) );
            r -= 6.0f;
        }

        g.setGradientFill( juce::ColourGradient( juce::Colour( 0xff5a5a5a ), c.x, c.y - r, juce::Colour( 0xff1e1e1e ), c.x, c.y + r, false ) );
        g.fillEllipse( juce::Rectangle<float>( 2 * r, 2 * r ).withCentre( c ) );
        g.setColour( juce::Colour( 0xff0e0e0e ) );
        g.drawEllipse( juce::Rectangle<float>( 2 * r, 2 * r ).withCentre( c ), 1.5f );

        // Indicator
        auto a  = juce::degreesToRadians( (float)angle );
        auto p1 = c.getPointOnCircumference( r * ( onCentrePress ? 0.55f : 0.25f ), a );
        auto p2 = c.getPointOnCircumference( r * 0.88f, a );
        g.setColour( juce::Colours::white );
        g.drawLine( { p1, p2 }, juce::jmax( 2.0f, r * 0.06f ) );

        if( onCentrePress )
        {
            auto cap = juce::Rectangle<float>( r * 0.8f, r * 0.8f ).withCentre( c );
            g.setColour( centrePressed ? palette::keyDown : palette::key );
            g.fillEllipse( cap );
            g.setColour( palette::outline );
            g.drawEllipse( cap, 1.0f );
            g.setColour( palette::text );
            g.setFont( juce::Font( juce::jmin( 13.0f, r * 0.2f ), juce::Font::bold ) );
            g.drawText( "ENTER", cap, juce::Justification::centred );
        }

        if( label.isNotEmpty() )
        {
            g.setColour( palette::text );
            g.setFont( juce::Font( juce::jmin( 14.0f, getHeight() * 0.14f ), juce::Font::bold ) );
            g.drawText( label, getLocalBounds().removeFromBottom( (int)juce::jmin( 20.0f, getHeight() * 0.18f ) ), juce::Justification::centred );
        }
    }

    void SimKnob::rotateBy( double degrees )
    {
        if( mode == Mode::absolute )
        {
            value = juce::jlimit( 0.0, 1.0, value + degrees / 270.0 );
            angle = -135.0 + value * 270.0;
            if( onValue ) onValue( value );
            repaint();
            return;
        }

        angle = std::fmod( angle + degrees, 360.0 );
        accum += degrees;
        int steps = (int)( accum / degreesPerStep );
        if( steps != 0 )
        {
            accum -= steps * degreesPerStep;
            if( onStep ) onStep( steps );
        }
        repaint();
    }

    void SimKnob::turn( int steps ) { rotateBy( steps * degreesPerStep ); }

    void SimKnob::pushCentre( bool isDown )
    {
        if( !onCentrePress || centrePressed == isDown )
            return;
        centrePressed = isDown;
        repaint();
        onCentrePress( isDown );
    }

    void SimKnob::mouseDown( const juce::MouseEvent& e )
    {
        if(interceptMapping(e)) return;
        if( inCentre( e.position ) )
        {
            pushCentre( true );
            return;
        }
        auto c        = knobArea().getCentre();
        lastDragAngle = std::atan2( e.position.y - c.y, e.position.x - c.x );
        lastDragY     = e.getPosition().y;
    }

    void SimKnob::mouseDrag( const juce::MouseEvent& e )
    {
        if(mappingGesture) return;
        if( centrePressed )
            return;

        if( angularDrag )
        {
            auto   c     = knobArea().getCentre();
            double a     = std::atan2( e.position.y - c.y, e.position.x - c.x );
            double delta = a - lastDragAngle;
            if( delta > juce::MathConstants<double>::pi ) delta -= juce::MathConstants<double>::twoPi;
            if( delta < -juce::MathConstants<double>::pi ) delta += juce::MathConstants<double>::twoPi;
            lastDragAngle = a;
            rotateBy( juce::radiansToDegrees( delta ) );
        }
        else
        {
            int dy    = lastDragY - e.getPosition().y; // up = clockwise
            lastDragY = e.getPosition().y;
            rotateBy( dy * degreesPerStep / pixelsPerStep );
        }
    }

    void SimKnob::mouseUp( const juce::MouseEvent& )
    {
        if(!mappingGesture) pushCentre( false );
        mappingGesture = false;
    }

    void SimKnob::mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& w )
    {
        if(onMidiMappingGesture && onMidiMappingGesture(e)) return;
        wheelAccum += ( w.isReversed ? -w.deltaY : w.deltaY ) / 0.11; // ~0.11 per notch on Windows
        int notches = (int)wheelAccum;
        if( notches != 0 )
        {
            wheelAccum -= notches;
            rotateBy( notches * degreesPerStep );
        }
    }

    //==============================================================================
    // SimPad
    //==============================================================================
    SimPad::SimPad( int i )
        : index( i )
    {
        setTooltip("Pad " + juce::String(i + 1) + ": click higher for more velocity; drag up for pressure");
        setMouseClickGrabsKeyboardFocus( false );
        setWantsKeyboardFocus( false );
    }

    void SimPad::setLit( bool l, juce::Colour c )
    {
        if( lit != l || colour != c )
        {
            lit    = l;
            colour = c;
            repaint();
        }
    }

    void SimPad::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds().toFloat().reduced( 2.0f );
        if(hardwareSkin)
        {
            if(lit || down)
            {
                const auto c = lit ? colour : juce::Colours::white;
                g.setColour(c.withAlpha(down ? 0.5f : 0.24f));
                g.fillRoundedRectangle(b, 3.f);
                g.setColour(c.withAlpha(0.9f));
                g.drawRoundedRectangle(b, 3.f, down ? 4.f : 2.f);
            }
            return;
        }
        g.setColour( juce::Colour( 0xff222222 ) );
        g.fillRoundedRectangle( b, 6.0f );

        if( lit )
        {
            g.setColour( colour.withAlpha( down ? 0.55f : 0.28f ) );
            g.fillRoundedRectangle( b.reduced( 4.0f ), 4.0f );
        }
        else if( down )
        {
            g.setColour( juce::Colours::white.withAlpha( 0.15f ) );
            g.fillRoundedRectangle( b.reduced( 4.0f ), 4.0f );
        }

        g.setColour( lit ? colour : juce::Colour( 0xff4a4a4a ) );
        g.drawRoundedRectangle( b, 6.0f, lit ? 3.5f : 1.5f );

        g.setColour( palette::text.withAlpha( 0.8f ) );
        g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
        g.drawText( "PAD " + juce::String( index + 1 ), b.reduced( 8.0f ), juce::Justification::topLeft );
        if( down )
            g.drawText( "vel " + juce::String( lastVelocity ) + ( lastPressure ? "  prs " + juce::String( lastPressure ) : juce::String() ), b.reduced( 8.0f ),
                        juce::Justification::bottomRight );
    }

    void SimPad::mouseDown( const juce::MouseEvent& e )
    {
        if(interceptMapping(e)) return;
        // Velocity from click height: top = 127, bottom = 20.
        auto y = juce::jlimit( 0.0f, 1.0f, e.position.y / (float)juce::jmax( 1, getHeight() ) );
        hit( true, juce::jlimit( 1, 127, (int)std::round( 127.0f - y * 107.0f ) ) );
    }

    void SimPad::mouseUp( const juce::MouseEvent& ) { if(!mappingGesture) hit( false ); mappingGesture = false; }

    void SimPad::mouseDrag( const juce::MouseEvent& e )
    {
        if(mappingGesture) return;
        // Dragging up while held presses harder: a pad's height = full pressure.
        const auto up = (float)-e.getDistanceFromDragStartY() / (float)juce::jmax( 1, getHeight() );
        press( juce::jlimit( 0, 127, (int)std::round( up * 127.0f ) ) );
    }

    void SimPad::press( int pressure )
    {
        if( !down || pressure == lastPressure )
            return;
        lastPressure = pressure;
        repaint();
        // The panel's pad packet with the pressure byte set; neither a press nor a release.
        hw::send( hw::padMessage( index, DCC::BTN_LONGPRESSED, lastVelocity, pressure ) );
    }

    void SimPad::hit( bool isDown, int velocity )
    {
        if( down == isDown )
            return;
        down = isDown;
        if( isDown )
            lastVelocity = velocity;
        lastPressure = 0;
        repaint();
        hw::send( hw::padMessage( index, isDown ? DCC::BTN_PRESSED : DCC::BTN_RELEASED, isDown ? velocity : 0 ) );
    }

    //==============================================================================
    // SimPadGrid
    //==============================================================================
    SimPadGrid::SimPadGrid()
    {
        padColourId.fill( DCC::PadColourTrans );
        padState.fill( DCC::StateOff );
        for( int i = 0; i < 16; ++i )
        {
            pads[i] = std::make_unique<SimPad>( i );
            addAndMakeVisible( *pads[i] );
        }
    }

    void SimPadGrid::setBankName( const juce::String& name )
    {
        bankName = name;
        repaint();
    }

    void SimPadGrid::paint( juce::Graphics& g )
    {
        if(artwork) return;
        g.setColour( palette::well );
        g.fillRoundedRectangle( getLocalBounds().toFloat(), 8.0f );
        g.setColour( palette::text );
        g.setFont( juce::Font( 15.0f, juce::Font::bold ) );
        g.drawText( "PADS   BANK " + bankName, getLocalBounds().reduced( 12, 6 ).removeFromTop( 22 ), juce::Justification::centredLeft );
    }

    void SimPadGrid::resized()
    {
        if(artwork)
        {
            const auto origin = artwork->bounds("pads").getPosition();
            for(int i = 0; i < 16; ++i)
                pads[i]->setBounds(artwork->bounds("pad-" + juce::String(i)).translated(-origin.x, -origin.y));
            return;
        }
        auto area = getLocalBounds().reduced( 10 );
        area.removeFromTop( 26 );
        const int gap = 10;
        const int w   = ( area.getWidth() - 3 * gap ) / 4;
        const int h   = ( area.getHeight() - 3 * gap ) / 4;
        for( int i = 0; i < 16; ++i )
        {
            int row = 3 - i / 4; // pad 1 bottom-left
            int col = i % 4;
            pads[i]->setBounds( area.getX() + col * ( w + gap ), area.getY() + row * ( h + gap ), w, h );
        }
    }

    void SimPadGrid::setPad( int pad, int state, int colourId )
    {
        if( pad < 0 || pad >= 16 )
            return;
        padState[pad]    = state;
        padColourId[pad] = colourId;
    }

    void SimPadGrid::setPanelArtwork(const PanelArtwork* panel)
    {
        artwork = panel;
        for(auto& pad : pads) pad->setHardwareSkin(artwork != nullptr);
        resized();
        repaint();
    }

    void SimPadGrid::refresh()
    {
        for( int i = 0; i < 16; ++i )
        {
            auto c   = hw::padColour( padColourId[i] );
            bool lit = padState[i] != DCC::StateOff && !c.isTransparent();
            pads[i]->setLit( lit, c );
        }
    }

    void SimPadGrid::applyPadPacket( const CPktMgr::HWMsg_t& m )
    {
        const auto& d = m.data;
        auto        n = (int)d.size();
        switch( m.pktCmd )
        {
            case DCC::EN_PAD_SET_SINGLE_PAD: // { pad, state, colour }
            case DCC::EN_PAD_SET_STEP_SINGLE_PAD: // { pad, state, colour, velocity }
                if( n >= 3 ) setPad( d[0], d[1], d[2] );
                break;

            case DCC::EN_PAD_SET_BLINKING: // { pad, state, onColour, offColour }
                if( n >= 3 ) setPad( d[0], d[1], d[2] );
                break;

            case DCC::EN_PAD_SET_ALL_SINGLE_COLOUR: // { state, colour }
                if( n >= 2 )
                    for( int i = 0; i < 16; ++i ) setPad( i, d[0], d[1] );
                break;

            case DCC::EN_PAD_SET_ALL_MULTI_COLOUR: // { state, colour x16 }
                for( int i = 0; i < 16 && i + 1 < n; ++i ) setPad( i, d[0], d[i + 1] );
                break;

            case DCC::EN_PAD_SET_COLOUR_STATE: // { colour x N, state x N }
            {
                int count = n / 2;
                for( int i = 0; i < count && i < 16; ++i ) setPad( i, d[count + i], d[i] );
                break;
            }

            case DCC::EN_PAD_SET_STEP_ALL_PAD: // { state, colour x16, velocity x16 }
                for( int i = 0; i < 16 && i + 1 < n; ++i ) setPad( i, d[0], d[i + 1] );
                break;

            default: return; // settings / colour mode / aftertouch: nothing to show
        }
        refresh();
    }

    //==============================================================================
    // SimTouchSlider
    //==============================================================================
    SimTouchSlider::SimTouchSlider()
    {
        setMouseClickGrabsKeyboardFocus( false );
        setWantsKeyboardFocus( false );
        setTooltip("Touch strip: click or drag vertically");
    }

    void SimTouchSlider::setLedLevel( int percent )
    {
        ledLevel = juce::jlimit( 0, 100, percent );
        repaint();
    }

    void SimTouchSlider::paint( juce::Graphics& g )
    {
        if(hardwareSkin)
        {
            for(int i = 0; i < 12; ++i)
            {
                g.setColour((12 - i) * 100 / 12 <= ledLevel ? palette::led : juce::Colour(0xff4a4b4f));
                g.fillRoundedRectangle(82.f, 22.f + i * 20.f, 7.2f, 11.f, 1.2f);
            }
            if(touchValue >= 0)
            {
                g.setColour(juce::Colours::white.withAlpha(0.65f));
                const auto y = 10.f + (getHeight() - 20.f) * (1.f - touchValue / 100.f);
                g.drawLine(12.f, y, 70.f, y, 2.f);
            }
            return;
        }
        auto b = getLocalBounds().toFloat();
        g.setColour( palette::text );
        g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
        g.drawText( "SLIDER", b.removeFromTop( 20.0f ), juce::Justification::centred );

        auto strip = b.withSizeKeepingCentre( juce::jmin( 34.0f, b.getWidth() ), b.getHeight() ).reduced( 0.0f, 4.0f );
        g.setColour( juce::Colour( 0xff101010 ) );
        g.fillRoundedRectangle( strip, 6.0f );

        // LED segments
        const int segments = 20;
        auto      segH     = strip.getHeight() / segments;
        for( int i = 0; i < segments; ++i )
        {
            auto seg = juce::Rectangle<float>( strip.getX() + 6.0f, strip.getBottom() - ( i + 1 ) * segH + 1.5f, strip.getWidth() - 12.0f, segH - 3.0f );
            bool on  = ( i + 1 ) * 100 / segments <= ledLevel;
            g.setColour( on ? palette::led : juce::Colour( 0xff2b2b2b ) );
            g.fillRect( seg );
        }

        if( touchValue >= 0 )
        {
            auto y = strip.getBottom() - strip.getHeight() * touchValue / 100.0f;
            g.setColour( juce::Colours::white );
            g.drawLine( strip.getX() - 4.0f, y, strip.getRight() + 4.0f, y, 2.0f );
        }
        g.setColour( palette::outline );
        g.drawRoundedRectangle( strip, 6.0f, 1.0f );
    }

    void SimTouchSlider::touch( const juce::MouseEvent& e )
    {
        auto top    = hardwareSkin ? 10.0f : 24.0f;
        auto height = juce::jmax( 1.0f, (float)getHeight() - top - (hardwareSkin ? 10.0f : 4.0f) );
        setTouch( (int)std::round( 100.0f * ( 1.0f - ( e.position.y - top ) / height ) ) );
    }

    void SimTouchSlider::setTouch( int value )
    {
        int v = juce::jlimit( 0, 100, value );
        if( v != touchValue )
        {
            touchValue = v;
            repaint();
            hw::send( hw::sliderMessage( v ) );
        }
    }

    void SimTouchSlider::mouseDown( const juce::MouseEvent& e ) { if(!interceptMapping(e)) touch( e ); }
    void SimTouchSlider::mouseDrag( const juce::MouseEvent& e ) { if(!mappingGesture) touch( e ); }
}
