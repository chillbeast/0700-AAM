/*
 * KickEM - KICK front-panel simulator.
 *
 * Same application wiring as apps/kick/Main.cpp (monolith adapters for device control, audio engine and
 * audio interface), but the window shows a simulated hardware panel with the real Kick UI embedded.
 * Built with SIM_BUILD=1, so Controls::InputEvents talks to sim::SimulatorInterfaceImpl instead of a serial port.
 */

#include "kick_ui/kick_ui.h"
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"
#include "simulator/kick_Simulator.h"
#include "simulator/kick_SimulatorInterfaceImpl.h"

//==============================================================================
class KickEMApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "KickEM"; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    // Self-tests may run alongside a simulator the user already has open.
    bool moreThanOneInstanceAllowed() override { return juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ).isNotEmpty(); }

    //==============================================================================
    void initialise( const juce::String& /*commandLine*/ ) override
    {
        // Self-tests start from a copy of the preferences so they never change the user's
        // (recent projects, current project, audio device), and the copy is thrown away.
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ).isNotEmpty()
            && juce::SystemStats::getEnvironmentVariable( "KICK_PREFS_DIR", {} ).isEmpty() )
        {
            auto copy = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_selftest_prefs" );
            copy.deleteRecursively();
            getAppDirs( MTPrefs ).copyDirectoryTo( copy );
#if JUCE_WINDOWS
            _putenv_s( "KICK_PREFS_DIR", copy.getFullPathName().toRawUTF8() );
#else
            setenv( "KICK_PREFS_DIR", copy.getFullPathName().toRawUTF8(), 1 );
#endif
        }

        // With KICKEM_LOG=<file>, also route the app's juce::Logger output into the simulator trace file.
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_LOG", {} ).isNotEmpty() && juce::Logger::getCurrentLogger() == nullptr )
        {
            struct TraceLogger : juce::Logger
            {
                void logMessage( const juce::String& m ) override { sim::SimulatorInterfaceImpl::log( "app: " + m ); }
            };
            appLogger = std::make_unique<TraceLogger>();
            juce::Logger::setCurrentLogger( appLogger.get() );
        }

        juce::SystemStats::setApplicationCrashHandler( []( void* ) {
            juce::Logger::outputDebugString( "[KickEM] CRASH\n" + juce::SystemStats::getStackBacktrace() );
        } );

        audioInterface.reset( new KickAudioInterface() );
        engineAdapter.reset( new KickAudioEngineAdapter( audioInterface ) );

        // Calls Controls::InputEvents::initialize() -> creates the simulator interface (SIM_BUILD).
        adapter.reset( new KickDeviceAdapter() );

        // Trace what reaches the UI's hardware handler (KickHome::handleCommand listens to this signal).
        uiTrace = contracts::DclSignals::hardwareInputTriggered.connect( []( CPktMgr::HWMsg_t& m ) {
            sim::SimulatorInterfaceImpl::log( "UI hardwareInputTriggered " + sim::SimulatorInterfaceImpl::describe( m ) );
        } );

        juce::Desktop::getInstance().setKioskModeComponent( nullptr );

        mt::getSettings();
        mt::getAudioInterface();

        w = std::make_unique<SimWindow>( getApplicationName() + " " + getApplicationVersion() );
        w->setVisible( true );
    }

    void shutdown() override
    {
        // Same order as apps/kick: detach the UI from the engine before destroying any
        // of it, then the window, the static UI's screens, and the engine last, all
        // while JUCE is still alive.
        auto& ai = mt::getAudioInterface();
        ai.removeAllListeners();
        mt::signals::disconnectAll();

        w       = nullptr;
        uiTrace.disconnect();
        adapter = nullptr;

        kick_ui::KickHome::Instance().releaseScreens();
        ai.shutdown();
        if( appLogger && juce::Logger::getCurrentLogger() == appLogger.get() )
            juce::Logger::setCurrentLogger( nullptr );
        appLogger = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        auto mgr = juce::ModalComponentManager::getInstance();
        if( mgr->cancelAllModalComponents() )
            juce::Timer::callAfterDelay( 100, [this] { quit(); } );
        else
            quit();
    }

    void anotherInstanceStarted( const juce::String& ) override {}

    //==============================================================================
    class SimWindow : public juce::DocumentWindow
    {
    public:
        explicit SimWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colour( 0xff202020 ), juce::DocumentWindow::allButtons )
        {
            setUsingNativeTitleBar( true );
            auto* simulator = new sim::Simulator();
            setContentOwned( simulator, false );
            setResizable( true, false );
            setResizeLimits(360, 300, 3600, 3000);
            getConstrainer()->setFixedAspectRatio( 0.0 );
            simulator->onLayoutChanged = [this, simulator] { fitPanel(simulator->preferredSize()); };
            fitPanel(simulator->preferredSize());
        }

        void fitPanel(juce::Point<int> size)
        {
            const int bw = size.x, bh = size.y;
            // Fit ~90% of the primary display, keeping the panel's aspect ratio.
            juce::Rectangle<int> area( 0, 0, 1600, 900 );
            if( auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() )
                area = display->userArea;
            float scale = juce::jmin( 1.0f, area.getWidth() * 0.92f / bw, area.getHeight() * 0.88f / bh );
            centreWithSize( juce::roundToInt( bw * scale ), juce::roundToInt( bh * scale ) );
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            auto index    = juce::jmax( 0, numModal - 1 );
            if( auto p = getCurrentlyModalComponent( index ) )
                p->toFront( true );
        }
    };

private:
    std::unique_ptr<juce::DocumentWindow>      w;
    std::unique_ptr<KickDeviceAdapter>         adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
    sigslot::connection                        uiTrace;
    std::unique_ptr<juce::Logger>              appLogger;
};

//==============================================================================
START_JUCE_APPLICATION( KickEMApplication )
