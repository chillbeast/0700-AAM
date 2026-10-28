#pragma once

#include <functional>
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <PktMgr/PktMgr.hpp>
#include <contracts/dcl/dcl_contracts.h>

///
/// Code-drawn front-panel controls for the KickEM simulator.
/// Every interaction is turned into the CPktMgr::HWMsg_t the front-panel MCU would send
/// (layouts as read by contracts::DclContractsHelper) and injected via SimulatorInterfaceImpl.
///
namespace sim
{
    class PanelArtwork;
    using DCC = contracts::DeviceControlContracts;

    namespace hw
    {
        /// EN_PKT_TYPE_BUTTON / EN_BTN_STATUS : { buttonCode (BtnLedSequence), BTN_PRESSED|BTN_RELEASED }
        CPktMgr::HWMsg_t buttonMessage( int buttonCode, int btnState );
        /// EN_PKT_TYPE_PAD / EN_PAD_STATE : { pad 0..15, BTN_PRESSED|BTN_RELEASED, velocity, aftertouch }
        CPktMgr::HWMsg_t padMessage( int pad, int btnState, int velocity, int aftertouch = 0 );
        /// EN_PKT_TYPE_KNOBS / EN_KNOB_ROTARY : { knob (KnobSequence), EN_KNOB_CLOCKWISE|EN_KNOB_ANTICLOCKWISE, |steps|, 0 }
        CPktMgr::HWMsg_t knobMessage( int knob, int steps );
        /// EN_PKT_TYPE_SLIDER / EN_SLIDER_TOUCH_INPUT : { value 0..100 }
        CPktMgr::HWMsg_t sliderMessage( int value );

        /// Deliver to the app through Controls::InputEvents (same path as the serial port).
        void send( const CPktMgr::HWMsg_t& msg );

        /// Pad colour id (contracts::DeviceControlContracts::PadColourIds) -> display colour.
        juce::Colour padColour( int colourId );
    }

    namespace palette
    {
        const juce::Colour body { 0xff353535 };
        const juce::Colour well { 0xff1b1b1b };
        const juce::Colour key { 0xff2a2a2a };
        const juce::Colour keyDown { 0xff4a4a4a };
        const juce::Colour outline { 0xff707070 };
        const juce::Colour text { 0xffe8e8e8 };
        const juce::Colour shiftText { 0xffffb608 };
        const juce::Colour led { 0xffff5c00 };
    }

    ///
    /// Momentary hardware button with LED. Optional SHIFT function (alternate code).
    ///
    class SimButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SimButton( juce::String label, int code, juce::String shiftLabel = {}, int shiftCode = DCC::EN_INVALID_ID );

        int  getCode() const { return code; }
        int  getShiftCode() const { return shiftCode; }
        void setLed( bool on );
        void setLedColour( juce::Colour c ) { ledColour = c; repaint(); }
        bool isLedOn() const { return led; }
        void setHardwareSkin(bool enabled, bool round = false) { hardwareSkin = enabled; roundHardware = round; repaint(); }

        /// Press / release exactly as a mouse click does (used by mouse handlers and the self-test).
        void press( bool isDown, bool shiftModifier = false );

        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;

        /// Custom press handler (pressed=true/false). If unset, button messages are sent.
        std::function<void( bool pressed )> onPress;

    protected:
        juce::String label, shiftLabel;
        int          code, shiftCode;
        int          sentCode { DCC::EN_INVALID_ID };
        bool         down { false }, led { false };
        bool         hardwareSkin { false }, roundHardware { false };
        juce::Colour ledColour { palette::led };
    };

    ///
    /// Latching SHIFT state shared by all buttons. Latch with the SHIFT button (or hold the PC Shift key
    /// while clicking). A latched SHIFT is released automatically after one shifted press.
    ///
    struct ShiftLatch
    {
        static bool isLatched();
        static void setLatched( bool latched ); // sends EN_SHIFT pressed / released
        static std::function<void( bool )> onChange;
    };

    ///
    /// Rotary control. Endless encoders emit step counts; absolute pots report 0..1.
    ///
    class SimKnob : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum class Mode
        {
            endless, // Q-link / jog wheel: steps
            absolute // Master / Rec volume pots: value 0..1 (analog on the hardware)
        };

        SimKnob( juce::String label, Mode mode, bool angularDrag = false );

        std::function<void( int steps )>     onStep; // +cw / -ccw
        std::function<void( double value )>  onValue; // absolute mode
        std::function<void( bool pressed )>  onCentrePress; // optional push (jog wheel "enter")

        /// Turn by encoder detents (+cw / -ccw) / push the centre, exactly as the mouse does.
        void turn( int steps );
        void pushCentre( bool isDown );
        void setHardwareSkin(bool enabled) { hardwareSkin = enabled; repaint(); }
        bool hitTest(int x, int y) override;

        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& w ) override;

    private:
        juce::Rectangle<float> knobArea() const;
        bool                   inCentre( juce::Point<float> p ) const;
        void                   rotateBy( double degrees );

        juce::String label;
        Mode         mode;
        bool         angularDrag;
        bool         hardwareSkin { false };
        double       angle { 0.0 }; // display angle (degrees)
        double       value { 0.75 }; // absolute mode
        double       accum { 0.0 };
        double       lastDragAngle { 0.0 };
        int          lastDragY { 0 };
        bool         centrePressed { false };
        double       wheelAccum { 0.0 };
    };

    ///
    /// One drum pad. Colour/state are driven by the pad packets the app sends to the hardware.
    ///
    class SimPad : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit SimPad( int index );

        void setLit( bool lit, juce::Colour colour );
        /// Hit / release exactly as a mouse click does.
        void hit( bool isDown, int velocity = 100 );
        /// Pressure on a held pad, 0..127 (what dragging upwards does).
        void press( int pressure );
        void setHardwareSkin(bool enabled) { hardwareSkin = enabled; repaint(); }
        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;

    private:
        int          index;
        bool         lit { false }, down { false };
        bool         hardwareSkin { false };
        int          lastVelocity { 0 }, lastPressure { 0 };
        juce::Colour colour { juce::Colours::grey };
    };

    ///
    /// 4x4 pad grid (pad 1 bottom-left, as on the hardware).
    ///
    class SimPadGrid : public juce::Component
    {
    public:
        SimPadGrid();

        void resized() override;
        void paint( juce::Graphics& g ) override;
        void setBankName( const juce::String& name );
        SimPad& getPad( int index ) { return *pads[(size_t)index]; }
        void setPanelArtwork(const PanelArtwork* artwork);

        /// Apply an EN_PKT_TYPE_PAD packet sent by the app to the hardware.
        void applyPadPacket( const CPktMgr::HWMsg_t& m );

    private:
        void setPad( int pad, int state, int colourId );
        void refresh();

        std::array<std::unique_ptr<SimPad>, 16> pads;
        std::array<int, 16>                     padState {};
        std::array<int, 16>                     padColourId {};
        juce::String                            bankName { "A" };
        const PanelArtwork* artwork { nullptr };
    };

    ///
    /// Touch strip. Emits slider packets; shows the LED level the app sends back.
    ///
    class SimTouchSlider : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SimTouchSlider();
        void setLedLevel( int percent );
        void setTouch( int value ); // 0..100, sends a slider packet when it changes
        void setHardwareSkin(bool enabled) { hardwareSkin = enabled; repaint(); }
        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;

    private:
        void touch( const juce::MouseEvent& e );
        int  touchValue { -1 };
        int  ledLevel { 0 };
        bool hardwareSkin { false };
    };
}
