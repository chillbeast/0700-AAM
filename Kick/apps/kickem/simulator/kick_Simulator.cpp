#include <kick_ui/kick_ui.h>
#include <program_edit_ui/program_edit_ui.h>
#include <program_edit_ui/sources/instrument/kick_ModularEditor.h>
#include "kick_Simulator.h"
#include "kick_SimulatorInterfaceImpl.h"
#include <audio_midi/audio_midi.h>

namespace sim
{
    namespace
    {
        const juce::Rectangle<int> screenRect { 620, 20, 1024, 600 };

        /// Diagnostics (set KICKEM_TRACE_MOUSE=1): log every mouse-down the app receives.
        struct MouseTrace : juce::MouseListener
        {
            void mouseDown( const juce::MouseEvent& e ) override
            {
                SimulatorInterfaceImpl::log( "mouseDown on " + juce::String( typeid( *e.eventComponent ).name() ) + " at " + e.getScreenPosition().toString()
                                             + " modal=" + juce::String( juce::Component::getNumCurrentlyModalComponents() ) );
            }
        } mouseTrace;
    }

    //==============================================================================
    // SimulatorBoard
    //==============================================================================
    SimulatorBoard::SimulatorBoard()
        : screen( kick_ui::KickHome::Instance() )
    {
        addAndMakeVisible( screen );

        addAndMakeVisible( slider );
        addAndMakeVisible( masterKnob );
        addAndMakeVisible( recKnob );
        addAndMakeVisible( fullLevelBtn );
        addAndMakeVisible( levels16Btn );
        addAndMakeVisible( eraseBtn );
        addAndMakeVisible( pads );
        addAndMakeVisible( mainControl );

        for( int i = 0; i < 4; ++i )
        {
            qlinks[i] = std::make_unique<SimKnob>( "P" + juce::String( i + 1 ), SimKnob::Mode::endless );
            qlinks[i]->onStep = [i]( int steps ) {
                hw::send( hw::knobMessage( DCC::EN_KNOB_1 + i, steps ) );
            };
            addAndMakeVisible( *qlinks[i] );
        }
        for( auto* b : { &linkBtn, &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn } )
            addAndMakeVisible( b );

        recBtn.setLedColour( juce::Colour( 0xffff2020 ) );
        overdubBtn.setLedColour( juce::Colour( 0xffff2020 ) );
        playBtn.setLedColour( juce::Colour( 0xff20e040 ) );
        playStartBtn.setLedColour( juce::Colour( 0xff20e040 ) );

        // LED routing table: both functions of a dual-function button share its LED.
        std::vector<SimButton*> buttons { &fullLevelBtn, &levels16Btn, &eraseBtn, &linkBtn, &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn };
        mainControl.collectButtons( buttons );
        for( auto* b : buttons )
        {
            if( b->getCode() != DCC::EN_INVALID_ID ) ledMap.emplace( b->getCode(), b );
            if( b->getShiftCode() != DCC::EN_INVALID_ID ) ledMap.emplace( b->getShiftCode(), b );
        }

        ShiftLatch::onChange = [this]( bool latched ) {
            mainControl.getShiftButton().setLed( latched );
            repaint(); // shift legends brighten while latched
        };

        outputConnection = SimulatorInterfaceImpl::outputReceived.connect( [this]( const CPktMgr::HWMsg_t& m ) { handleOutput( m ); } );
        setupMidiLearn(buttons);

        setSize( boardWidth, boardHeight ); // last: resized() needs every child

        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_TRACE_MOUSE", {} ).isNotEmpty() )
            juce::Desktop::getInstance().addGlobalMouseListener( &mouseTrace );

        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ).isNotEmpty() )
            startSelfTest();
    }

    SimButton* SimulatorBoard::findButton( int code )
    {
        auto range = ledMap.equal_range( code );
        for( auto it = range.first; it != range.second; ++it )
            if( it->second->getCode() == code )
                return it->second;
        return nullptr;
    }

    void SimulatorBoard::startSelfTest()
    {
        if(juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}) == "midi-audit"
            || juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}).startsWith("startup"))
        { startMidiAuditTest(); return; }
        if(juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}).startsWith("midi-learn"))
        {
            startMidiLearnTest();
            return;
        }
        if(juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}).startsWith("panel"))
        {
            startPanelTest();
            return;
        }
        auto& s     = selfTestSteps;
        auto  note  = [&s]( juce::String text ) { s.push_back( [text] { SimulatorInterfaceImpl::log( "SELFTEST " + text ); } ); };
        auto  wait  = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        auto pad = [&s, this]( int i ) {
            s.push_back( [this, i] { pads.getPad( i ).hit( true, 100 ); } );
            s.push_back( [this, i] { pads.getPad( i ).hit( false ); } );
        };

        if(juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {}) == "audit")
        {
            auto failed = std::make_shared<int>(0);
            auto check = [failed](bool ok, juce::String text) {
                if(!ok) ++*failed;
                SimulatorInterfaceImpl::log("SELFTEST " + juce::String(ok ? "PASS " : "FAIL ") + text);
            };
            s.push_back([check] {
                auto selection = std::make_unique<ui::KickListBox>("Audit", std::vector<juce::String>{"A", "B"});
                selection->setSelected(1, juce::dontSendNotification);
                check(selection->getSelectedIndex() == 1, "P1: message-thread selection applies synchronously");
                auto* raw = selection.get();
                std::thread producer([raw] { raw->setSelected(0, juce::dontSendNotification); });
                producer.join();
                selection.reset(); // queued selection must observe the deleted owner
                for(int i = 0; i < 100; ++i)
                {
                    auto control = std::make_unique<ui::KickCyclicStateControl>();
                    control->setStates({"A", "B"});
                    control->setNormalized(1.f);
                }
                ui::themeChanged();
                check(true, "P2: theme emission after 100 control destructions is safe");
                int count = 3;
                ui::KickTableViewProvider provider;
                provider.numRows = [&] { return count; };
                provider.cellComponent = [](int, juce::Component* previous) { return previous ? previous : new juce::Label(); };
                ui::KickTableView table("Audit", provider);
                table.setBounds(0, 0, 300, 200);
                table.refresh();
                count = 1; table.refresh();
                check(table.getRowComponents().size() == 1, "U8: shrinking the table destroys stale owned rows");
                count = 0; table.refresh();
                check(table.getRowComponents().isEmpty(), "U8: empty table releases every row");
                kick_ui::ProgramMixer programs;
                kick_ui::TrackMidiMixer midi;
                programs.knobCommand(1, 1);
                midi.knobCommand(1, 1);
                check(true, "P3: empty program and MIDI mixers accept knob input");
            });
            wait(3);
            s.push_back([failed] {
                SimulatorInterfaceImpl::log("SELFTEST done: " + juce::String(*failed) + " failures");
                juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failed);
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            });
            startTimer(200);
            return;
        }

        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "instrument" )
        {
            startInstrumentTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "audio" )
        {
            startAudioSettingsTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "tour" )
        {
            startTourTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "popup" )
        {
            startPopupTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "picker" )
        {
            startPickerTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "resources" )
        {
            startResourcesTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "record" )
        {
            startRecordTest();
            return;
        }
        if( juce::SystemStats::getEnvironmentVariable( "KICKEM_SELFTEST", {} ) == "modular" )
        {
            startModularTest();
            return;
        }

        wait( 4 );
        note( "jog ENTER (load selected project)" );
        s.push_back( [this] { mainControl.getJog().pushCentre( true ); } );
        s.push_back( [this] { mainControl.getJog().pushCentre( false ); } );
        wait( 12 );
        note( "pads 1 and 6" );
        pad( 0 );
        pad( 5 );
        note( "bank B" );
        click( DCC::EN_BANK_B );
        wait( 2 );
        note( "SHIFT + bank C (-> bank G)" );
        s.push_back( [this] { mainControl.getShiftButton().press( true ); } );
        s.push_back( [this] { mainControl.getShiftButton().press( false ); } );
        click( DCC::EN_BANK_C );
        wait( 2 );
        note( "bank A" );
        click( DCC::EN_BANK_A );
        note( "PLAY" );
        click( DCC::EN_PLAY );
        wait( 3 );
        note( "STOP" );
        click( DCC::EN_STOP );
        note( "jog +3 / -2" );
        s.push_back( [this] { mainControl.getJog().turn( 3 ); } );
        s.push_back( [this] { mainControl.getJog().turn( -2 ); } );
        note( "Q-link P1..P4 +2 / -1" );
        for( int q = 0; q < 4; ++q )
        {
            s.push_back( [this, q] { qlinks[(size_t)q]->turn( 2 ); } );
            s.push_back( [this, q] { qlinks[(size_t)q]->turn( -1 ); } );
        }
        note( "slider 30 / 70" );
        s.push_back( [this] { slider.setTouch( 30 ); } );
        s.push_back( [this] { slider.setTouch( 70 ); } );
        note( "LINK, ERASE, MAIN" );
        click( DCC::EN_LINK );
        click( DCC::EN_ERASE );
        click( DCC::EN_MAINMODE );
        note( "done" );

        startTimer( 400 );
    }

    void SimulatorBoard::startInstrumentTest()
    {
        // KICKEM_SELFTEST=instrument: create an instrument program, open its edit screen,
        // drive Q-links and pads, then quit. KICKEM_SHOT=<folder> saves screen snapshots.
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        auto shot = [&s, this, log]( juce::String name ) {
            s.push_back( [this, log, name] {
                const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
                if( dir.isEmpty() )
                    return;
                auto file = juce::File( dir ).getChildFile( name + ".png" );
                file.deleteFile();
                if( auto out = file.createOutputStream() )
                {
                    juce::PNGImageFormat().writeImageToStream( screen.createComponentSnapshot( screen.getLocalBounds() ), *out );
                    log( "snapshot " + file.getFullPathName() );
                }
            } );
        };
        auto instrument = []() -> mt::MTInstrumentProgram* {
            auto& ae = mt::getAudioInterface();
            return dynamic_cast<mt::MTInstrumentProgram*>( ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() ) );
        };

        wait( 4 );
        // Work on a throwaway copy of a template: the app saves the open project on exit,
        // so loading the first project in the list would modify the user's own projects.
        s.push_back( [log] {
            auto exeDir  = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            // The engine opens <folder>/<folder name>.kickPrj.
            auto source  = exeDir.getChildFile( "templates/Template 01/Template 01.kickPrj" );
            auto scratch = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_selftest/SelfTest" );
            scratch.getParentDirectory().deleteRecursively();
            scratch.createDirectory();
            const bool ok = source.copyFileTo( scratch.getChildFile( "SelfTest.kickPrj" ) ) && mt::getAudioInterface().loadProject( scratch );
            log( "loaded scratch project " + scratch.getFullPathName() + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );

        s.push_back( [log] {
            auto& ae = mt::getAudioInterface();
            if( !ae.getNumSequences() )
                ae.createSequence();
            const auto seq  = ae.getSelectedSequence();
            const auto trk  = ae.createTrack( seq, "Synth" );
            const auto prog = ae.createProgram( mt::MTProgramType::Plugin, "Synth 1" );
            ae.setProgramToTrack( prog, seq, trk );
            ae.setSelectedSequenceAndTrack( seq, trk );
            ae.setSelectedProgram( prog );
            log( "created instrument program " + juce::String( prog ) + " on track " + juce::String( trk ) );
        } );
        wait( 2 );
        s.push_back( [log] { log( "PROGRAM EDIT" ); } );
        click( DCC::EN_PROG_EDIT );
        wait( 4 );
        s.push_back( [log] {
            log( "screen = " + juce::String( (int)kick_ui::KickHome::Instance().getCurrentScreen() ) + " (MenuProgramEdit = " + juce::String( (int)kick_ui::Screen::MenuProgramEdit ) + ")" );
        } );
        shot( "instrument_edit_page1" );

        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( "before: " + i->getParams()[0].name + "=" + juce::String( i->getParam( 0 ), 3 ) + "  " + i->getParams()[3].name + "=" + juce::String( i->getParam( 3 ), 1 ) );
        } );
        s.push_back( [log] { log( "Q-link P1 +20, P4 -30" ); } );
        s.push_back( [this] { qlinks[0]->turn( 20 ); } );
        s.push_back( [this] { qlinks[3]->turn( -30 ); } );
        wait( 2 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( "after:  " + i->getParams()[0].name + "=" + juce::String( i->getParam( 0 ), 3 ) + "  " + i->getParams()[3].name + "=" + juce::String( i->getParam( 3 ), 1 ) );
        } );
        shot( "instrument_edit_after_qlinks" );

        s.push_back( [log] { log( "bank D, pads 1-4 (C3 D3 E3 F3 in C major 4ths)" ); } );
        click( DCC::EN_BANK_D );
        for( int p = 0; p < 4; ++p )
        {
            s.push_back( [this, p, log] {
                log( "pad " + juce::String( p + 1 ) + " -> note " + juce::String( kick_ui::PadNotes::noteForPad( p ) ) );
                pads.getPad( p ).hit( true, 100 );
            } );
            wait( 1 );
            s.push_back( [this, p] { pads.getPad( p ).hit( false ); } );
        }

        // Modular: pick it in the ENGINE box and a preset in the PRESET box, as a user would.
        auto listWith = [this]( juce::String Option ) -> ui::KickListBox* {
            std::function<ui::KickListBox*( juce::Component& )> find = [&find, &Option]( juce::Component& c ) -> ui::KickListBox* {
                if( auto* lb = dynamic_cast<ui::KickListBox*>( &c ); lb && lb->isShowing() )
                {
                    const auto options = lb->getListOptions();
                    if( std::find( options.begin(), options.end(), Option ) != options.end() )
                        return lb;
                }
                for( auto* child : c.getChildren() )
                    if( auto* hit = find( *child ) )
                        return hit;
                return nullptr;
            };
            return find( screen );
        };
        auto choose = [&s, listWith, log]( juce::String Option ) {
            s.push_back( [listWith, log, Option] {
                if( auto* lb = listWith( Option ) )
                {
                    lb->setSelected( Option, juce::sendNotification );
                    log( "chose " + Option );
                }
                else
                    log( "no list box offers " + Option );
            } );
        };
        auto report = [&s, log, instrument]() {
            s.push_back( [log, instrument] {
                if( auto* i = instrument() )
                    log( "engine " + i->getEngineName() + ", patch " + i->getPatchName() + ( i->getPatch().isValid() ? " (stored)" : " (default)" ) + ", " +
                         juce::String( (int)i->getParams().size() ) + " params, presets: " + i->getPresetNames().joinIntoString( ", " ) );
            } );
        };
        auto playPads = [&s, this, wait]() {
            for( int p = 0; p < 4; ++p )
            {
                s.push_back( [this, p] { pads.getPad( p ).hit( true, 100 ); } );
                wait( 1 );
                s.push_back( [this, p] { pads.getPad( p ).hit( false ); } );
            }
        };

        choose( "Modular" );
        wait( 3 );
        report();
        shot( "modular_edit_init" );
        playPads();
        choose( "FM Bell" );
        wait( 3 );
        report();
        shot( "modular_edit_fm_bell" );
        playPads();
        s.push_back( [log] { log( "Q-link P1 +40 (Macro 1)" ); } );
        s.push_back( [this] { qlinks[0]->turn( 40 ); } );
        wait( 2 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( i->getParams()[0].name + "=" + juce::String( i->getParam( 0 ), 3 ) );
        } );
        playPads();
        shot( "modular_edit_macro" );
        s.push_back( [log] { log( "done" ); } );
        wait( 2 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );

        startTimer( 400 );
    }

    void SimulatorBoard::startModularTest()
    {
        auto& s     = selfTestSteps;
        auto  log   = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait  = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        auto shot = [&s, this, log]( juce::String name ) {
            s.push_back( [this, log, name] {
                const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
                if( dir.isEmpty() )
                    return;
                auto file = juce::File( dir ).getChildFile( name + ".png" );
                file.deleteFile();
                if( auto out = file.createOutputStream() )
                {
                    juce::PNGImageFormat().writeImageToStream( screen.createComponentSnapshot( screen.getLocalBounds() ), *out );
                    log( "snapshot " + file.getFullPathName() );
                }
            } );
        };
        auto instrument = []() -> mt::MTInstrumentProgram* {
            auto& ae = mt::getAudioInterface();
            return dynamic_cast<mt::MTInstrumentProgram*>( ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() ) );
        };
        // Finds a showing component anywhere on screen, popups included.
        auto findShowing = [this]( std::function<bool( juce::Component& )> Match ) -> juce::Component* {
            std::function<juce::Component*( juce::Component& )> find = [&find, &Match]( juce::Component& c ) -> juce::Component* {
                if( c.isShowing() && Match( c ) )
                    return &c;
                for( auto* child : c.getChildren() )
                    if( auto* hit = find( *child ) )
                        return hit;
                return nullptr;
            };
            return find( *getTopLevelComponent() );
        };
        auto editor = [findShowing]() { return dynamic_cast<kick_ui::ModularEditor*>( findShowing( []( juce::Component& c ) { return dynamic_cast<kick_ui::ModularEditor*>( &c ) != nullptr; } ) ); };
        auto tapButton = [&s, findShowing, log]( juce::String Text ) {
            s.push_back( [findShowing, log, Text] {
                auto* b = dynamic_cast<juce::Button*>( findShowing( [Text]( juce::Component& c ) {
                    auto* btn = dynamic_cast<juce::Button*>( &c );
                    return btn && btn->getButtonText() == Text;
                } ) );
                if( b )
                    b->triggerClick();
                log( ( b ? "tapped " : "no button " ) + Text );
            } );
        };
        auto describe = [&s, log, instrument]( juce::String When ) {
            s.push_back( [log, instrument, When] {
                auto* i = instrument();
                if( !i )
                    return;
                const auto                     patch = i->getPatch();
                const mt::modular::PatchEditor read( patch, nullptr );
                juce::StringArray              parts;
                for( const auto& m : patch )
                {
                    juce::String d = read.moduleLabel( m.getProperty( "id" ) ) + "@" + m.getProperty( "col" ).toString() + "," + m.getProperty( "row" ).toString();
                    if( m.hasProperty( "cutoff" ) )
                        d << " cutoff=" << juce::String( (float)m.getProperty( "cutoff" ), 1 );
                    for( const auto& r : m )
                        d << " [" << r.getProperty( "param" ).toString() << "<" << read.sourceLabel( r.getProperty( "source" ) ) << " "
                          << juce::String( (float)r.getProperty( r.hasType( "CABLE" ) ? "amount" : "depth" ), 2 ) << "]";
                    parts.add( d );
                }
                log( When + ": " + ( patch.isValid() ? patch.getProperty( "name" ).toString() : juce::String( "(default)" ) ) + " | " + parts.joinIntoString( " | " ) );
            } );
        };

        wait( 4 );
        s.push_back( [log] {
            auto exeDir  = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto source  = exeDir.getChildFile( "templates/Template 01/Template 01.kickPrj" );
            auto scratch = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_selftest/SelfTest" );
            scratch.getParentDirectory().deleteRecursively();
            scratch.createDirectory();
            const bool ok = source.copyFileTo( scratch.getChildFile( "SelfTest.kickPrj" ) ) && mt::getAudioInterface().loadProject( scratch );
            log( "loaded scratch project " + scratch.getFullPathName() + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );
        s.push_back( [log] {
            auto& ae = mt::getAudioInterface();
            if( !ae.getNumSequences() )
                ae.createSequence();
            const auto seq  = ae.getSelectedSequence();
            const auto trk  = ae.createTrack( seq, "Modular" );
            const auto prog = ae.createProgram( mt::MTProgramType::Plugin, "Modular 1" );
            ae.setProgramToTrack( prog, seq, trk );
            ae.setSelectedSequenceAndTrack( seq, trk );
            ae.setSelectedProgram( prog );
            if( auto* i = dynamic_cast<mt::MTInstrumentProgram*>( ae.getProgram( prog ) ) )
                i->setEngine( "Modular" );
            log( "created Modular program " + juce::String( prog ) );
        } );
        wait( 2 );
        click( DCC::EN_PROG_EDIT );
        wait( 4 );
        tapButton( "EDIT PATCH" );
        wait( 3 );
        s.push_back( [log, editor] { log( editor() ? "patch editor open" : "patch editor NOT open" ); } );
        describe( "start" );
        shot( "modular_editor_init" );

        // Select the filter and turn Q-link P2 (cutoff) down.
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 0, 1 ); } );
        wait( 1 );
        s.push_back( [this] { qlinks[1]->turn( -25 ); } );
        wait( 2 );
        describe( "after P2 -25 on the filter" );
        shot( "modular_editor_filter" );

        // Add an LFO in an empty cell through the module list, as a tap would.
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 1, 1 ); } );
        tapButton( "+ MODULE" );
        wait( 3 );
        shot( "modular_editor_picker" );
        tapButton( "LFO" );
        wait( 3 );
        describe( "after adding an LFO" );

        // Modulate the filter cutoff from it: + MOD, pick the knob, then the source.
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 0, 1 ); } );
        wait( 1 );
        tapButton( "+ MOD" );
        wait( 3 );
        tapButton( "Cutoff" );
        wait( 4 );
        tapButton( "LFO 1" );
        wait( 3 );
        s.push_back( [this] { qlinks[2]->turn( -20 ); } ); // the new modulation is third: P3, depth 0.5 -> 0.1
        wait( 2 );
        describe( "after modulating cutoff from the LFO" );
        shot( "modular_editor_mods" );

        // Move the LFO with the API a drag uses, play, then undo the move.
        s.push_back( [editor] {
            if( auto* e = editor() )
            {
                e->selectCell( 1, 1 );
                e->moveModule( 3, 2 );
            }
        } );
        wait( 2 );
        describe( "after moving the LFO" );
        click( DCC::EN_BANK_D );
        for( int p = 0; p < 4; ++p )
        {
            s.push_back( [this, p] { pads.getPad( p ).hit( true, 100 ); } );
            wait( 1 );
            s.push_back( [this, p] { pads.getPad( p ).hit( false ); } );
        }
        s.push_back( [] {
            mt::getAudioInterface().getUndoManager().beginNewTransaction();
            mt::getAudioInterface().performUndo();
        } );
        wait( 2 );
        describe( "after undo" );
        shot( "modular_editor_after_undo" );

        // Milestone 3: an effect, a cable, and saving a user preset.
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 0, kick_ui::ModularEditor::effectRow ); } );
        wait( 1 );
        tapButton( "+ EFFECT" );
        wait( 3 );
        tapButton( "Reverb" );
        wait( 3 );
        s.push_back( [this] { qlinks[3]->turn( 30 ); } ); // P4 = mix
        wait( 2 );
        describe( "after adding a reverb" );
        shot( "modular_editor_effect" );

        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 2, 0 ); } );
        tapButton( "+ MODULE" );
        wait( 3 );
        tapButton( "Oscillator" );
        wait( 3 );
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 0, 0 ); } );
        wait( 1 );
        tapButton( "IN (0)" );
        wait( 1 );
        tapButton( "+ CABLE" );
        wait( 3 );
        tapButton( "Oscillator 2" );
        wait( 3 );
        s.push_back( [this] { qlinks[0]->turn( 20 ); } ); // amount 0.5 -> 0.9 (2% of -1..1 per detent)
        wait( 2 );
        describe( "after cabling Oscillator 2 into Oscillator 1" );
        shot( "modular_editor_cable" );
        click( DCC::EN_BANK_D );
        for( int p = 0; p < 4; ++p )
        {
            s.push_back( [this, p] { pads.getPad( p ).hit( true, 100 ); } );
            wait( 1 );
            s.push_back( [this, p] { pads.getPad( p ).hit( false ); } );
        }

        tapButton( "SAVE" );
        wait( 3 );
        shot( "modular_editor_save" );
        tapButton( "Apply" );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( "after SAVE: patch " + i->getPatchName() + ", presets: " + i->getPresetNames().joinIntoString( ", " ) + " | macro 1 = " + i->getParamName( 0 ) );
        } );

        // Round 4: legato with glide, pressure, live activity, moving an effect, deleting a preset.
        tapButton( "POLY 8" );
        wait( 3 );
        tapButton( "Legato" );
        wait( 4 );
        tapButton( "100 ms" );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
            {
                const mt::modular::PatchEditor read( i->getPatch(), nullptr );
                log( "play mode " + juce::String( read.getPlayMode() ) + ", glide " + juce::String( read.getGlide(), 3 ) + " s" );
            }
        } );
        s.push_back( [editor] {
            if( auto* e = editor() )
            {
                e->selectCell( 0, kick_ui::ModularEditor::effectRow );
                e->moveEffect( 2 );
            }
        } );
        wait( 2 );
        describe( "after moving the reverb to FX 3" );
        s.push_back( [this] { pads.getPad( 0 ).hit( true, 110 ); } );
        wait( 2 );
        s.push_back( [this] { pads.getPad( 0 ).press( 100 ); } );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
            {
                juce::StringArray parts;
                for( const auto& m : i->getPatch() )
                    parts.add( m.getProperty( "type" ).toString() + "=" + juce::String( i->getActivity( m.getProperty( "id" ) ), 2 ) );
                log( "activity while held: " + parts.joinIntoString( " " ) );
            }
        } );
        shot( "modular_editor_activity" );
        s.push_back( [this] { pads.getPad( 0 ).hit( false ); } );
        wait( 2 );

        tapButton( "SAVE" );
        wait( 3 );
        tapButton( "Delete a user preset" );
        wait( 4 );
        tapButton( "Init 2" );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( "after delete: presets " + i->getPresetNames().joinIntoString( ", " ) );
        } );

        // Wave 5: a Formant module from the list, and the arpeggiator set up through ARP.
        s.push_back( [editor] { if( auto* e = editor() ) e->selectCell( 3, 0 ); } );
        tapButton( "+ MODULE" );
        wait( 3 );
        tapButton( "Formant" );
        wait( 3 );
        tapButton( "ARP OFF" );
        wait( 3 );
        tapButton( "Up" );
        wait( 4 );
        tapButton( "1/16" );
        wait( 4 );
        tapButton( "2" );
        wait( 4 );
        tapButton( "50%" );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
            {
                const mt::modular::PatchEditor read( i->getPatch(), nullptr );
                log( "arp mode " + juce::String( read.getArp() ) + ", rate " + juce::String( read.getArpRate() ) + ", octaves " + juce::String( read.getArpOctaves() ) + ", gate " +
                     juce::String( read.getArpGate(), 2 ) + " | modules: " + juce::String( i->getPatch().getNumChildren() ) );
            }
        } );
        for( int p : { 0, 2, 4 } ) // a chord: three pads together
            s.push_back( [this, p] { pads.getPad( p ).hit( true, 100 ); } );
        wait( 3 );
        shot( "modular_editor_arp" );
        wait( 2 );
        for( int p : { 0, 2, 4 } )
            s.push_back( [this, p] { pads.getPad( p ).hit( false ); } );
        wait( 2 );

        tapButton( "< BACK" );
        wait( 3 );
        s.push_back( [log, instrument] {
            if( auto* i = instrument() )
                log( "back on the instrument page, preset box shows: " + i->getPatchName() );
        } );
        shot( "modular_editor_closed" );
        s.push_back( [log] { log( "done" ); } );
        wait( 2 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );

        startTimer( 400 );
    }

    void SimulatorBoard::startAudioSettingsTest()
    {
        // KICKEM_SELFTEST=audio: list device types, open Settings > Audio, snapshot, quit.
        auto& s   = selfTestSteps;
        auto  log = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };

        wait( 6 ); // the engine opens its device asynchronously
        s.push_back( [log] {
            auto& dm = mt::getAudioInterface().getAudioDeviceManager();
            for( auto* type : dm.getAvailableDeviceTypes() )
            {
                type->scanForDevices();
                log( "device type " + type->getTypeName() + ": " + type->getDeviceNames().joinIntoString( " | " ) );
            }
            auto* dev = dm.getCurrentAudioDevice();
            log( "current: " + dm.getCurrentAudioDeviceType() + " / " + ( dev ? dev->getName() + " @ " + juce::String( dev->getCurrentSampleRate() ) + " Hz, " + juce::String( dev->getCurrentBufferSizeSamples() ) : juce::String( "none" ) ) );
        } );
        s.push_back( [] { kick_ui::signals::sigScreenClose( kick_ui::Screen::HomePreferencesView ); } );
        wait( 3 );
        s.push_back( [this, log] {
            // Click the "Audio" tab the way a user would.
            std::function<juce::Button*( juce::Component& )> find = [&find]( juce::Component& c ) -> juce::Button* {
                if( auto* b = dynamic_cast<juce::Button*>( &c ); b && b->getName() == "Audio" )
                    return b;
                for( auto* child : c.getChildren() )
                    if( auto* hit = find( *child ) )
                        return hit;
                return nullptr;
            };
            if( auto* tab = find( screen ) )
            {
                tab->triggerClick();
                log( "clicked Audio tab" );
            }
            else
                log( "Audio tab not found" );
        } );
        wait( 4 );
        s.push_back( [this, log] {
            const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
            if( dir.isEmpty() )
                return;
            auto file = juce::File( dir ).getChildFile( "settings_audio.png" );
            file.deleteFile();
            if( auto out = file.createOutputStream() )
            {
                juce::PNGImageFormat().writeImageToStream( screen.createComponentSnapshot( screen.getLocalBounds() ), *out );
                log( "snapshot " + file.getFullPathName() );
            }
        } );
        s.push_back( [log] { log( "done" ); } );
        wait( 2 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );

        startTimer( 400 );
    }

    void SimulatorBoard::startTourTest()
    {
        // KICKEM_SELFTEST=tour: load a scratch copy of a kit (KICKEM_TOUR_PROJECT=<project name>,
        // default "Vintage Soul Kit"), then press every panel button and its SHIFT function from
        // the main screen. Each stop logs the screen the app reports and, with KICKEM_SHOT, saves a snapshot.
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  screenName = []() -> juce::String {
            static const char* names[] = { "HomeOptions", "HomeListView", "HomeGridView", "HomeSequencerView", "HomeXYView", "HomePreferencesView",
                                           "HomePreferences", "MenuSampleEdit", "MenuProgramEdit", "MenuSampler", "MenuLooper", "MenuPadMute",
                                           "MenuTrackMute", "MenuPadMixer", "MenuChannelMixer", "MenuKnobEdit", "MenuListEdit", "MenuPadPerform",
                                           "MenuMidiControl", "MenuFileBrowser", "MenuNextSequence", "MenuSong", "MenuColorEdit", "MenuAssignSample",
                                           "RecentProject", "EditProgram", "SWUpdateView", "SWUpdateWindow", "ProcessEdit", "CVClib",
                                           "SystemResources", "Inserts", "Temp" };
            const int i = (int)kick_ui::KickHome::Instance().getCurrentScreen() - (int)kick_ui::Screen::HomeOptions;
            return i >= 0 && i < (int)std::size( names ) ? juce::String( names[i] ) : juce::String( i + (int)kick_ui::Screen::HomeOptions );
        };
        auto shot = [&s, this, log]( juce::String name ) {
            s.push_back( [this, log, name] {
                const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
                if( dir.isEmpty() )
                    return;
                auto file = juce::File( dir ).getChildFile( name + ".png" );
                file.deleteFile();
                if( auto out = file.createOutputStream() )
                    juce::PNGImageFormat().writeImageToStream( screen.createComponentSnapshot( screen.getLocalBounds() ), *out );
            } );
        };
        auto press = [&s, this]( int code, bool shifted ) {
            s.push_back( [this, code, shifted] {
                if( auto* b = findButton( code ) )
                    b->press( true, shifted );
            } );
            s.push_back( [this, code] {
                if( auto* b = findButton( code ) )
                    b->press( false );
            } );
        };
        auto visit = [&]( juce::String label, int code, bool shifted ) {
            s.push_back( [log, label] { log( "press " + label ); } );
            press( code, shifted );
            wait( 4 );
            s.push_back( [log, label, screenName] { log( "  " + label + " -> screen " + screenName() ); } );
            shot( "tour_" + label.replace( " ", "_" ).replace( "/", "_" ) );
            // Poke the screen's controls: every Q-link, the jog and a pad. Preferences are global
            // (not part of the scratch project), so leave that screen alone.
            if( label != "PREFS" )
            {
                for( int q = 0; q < 4; ++q )
                {
                    s.push_back( [this, q] { qlinks[(size_t)q]->turn( 1 ); } );
                    s.push_back( [this, q] { qlinks[(size_t)q]->turn( -1 ); } );
                }
                s.push_back( [this] { mainControl.getJog().turn( 1 ); } );
                s.push_back( [this] { mainControl.getJog().turn( -1 ); } );
                s.push_back( [this] { pads.getPad( 0 ).hit( true, 100 ); } );
                s.push_back( [this] { pads.getPad( 0 ).hit( false ); } );
                s.push_back( [log, label, screenName] { log( "  " + label + " controls ok, screen " + screenName() ); } );
            }
            // Back to the main screen for the next stop.
            press( DCC::EN_MAINMODE, false );
            wait( 3 );
        };

        wait( 4 );
        s.push_back( [log] {
            auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto name   = juce::SystemStats::getEnvironmentVariable( "KICKEM_TOUR_PROJECT", "Vintage Soul Kit" );
            auto source = exeDir.getChildFile( "projects" ).getChildFile( name );
            if( !source.isDirectory() )
                source = exeDir.getChildFile( "templates/Template 01" );
            // The engine opens <folder>/<folder name>.kickPrj, so keep the project's folder name.
            auto root = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_tour" );
            root.deleteRecursively();
            auto       scratch = root.getChildFile( source.getFileName() );
            const bool ok      = source.copyDirectoryTo( scratch ) && mt::getAudioInterface().loadProject( scratch );
            auto&      ae      = mt::getAudioInterface();
            log( "loaded scratch copy of " + source.getFileName() + ( ok ? "" : " (FAILED)" ) + ": " + juce::String( ae.getNumSequences() ) + " sequence(s), "
                 + juce::String( ae.getNumPrograms() ) + " program(s)" );
        } );
        wait( 8 );
        press( DCC::EN_MAINMODE, false );
        wait( 3 );
        s.push_back( [log, screenName] {
            auto& ae = mt::getAudioInterface();
            log( "main screen = " + screenName() + ", sequences " + juce::String( ae.getNumSequences() ) );
        } );
        shot( "tour_MAIN" );

        // Transport from the main screen.
        s.push_back( [log] { log( "press PLAY" ); } );
        press( DCC::EN_PLAY, false );
        wait( 5 );
        s.push_back( [log] { log( juce::String( "  playing = " ) + ( mt::getAudioInterface().isSequencePlaying() ? "yes" : "no" ) ); } );
        shot( "tour_PLAYING" );
        s.push_back( [log] { log( "press STOP" ); } );
        press( DCC::EN_STOP, false );
        wait( 2 );
        s.push_back( [log] { log( juce::String( "  playing = " ) + ( mt::getAudioInterface().isSequencePlaying() ? "yes" : "no" ) ); } );

        struct Stop
        {
            const char* label;
            int         code;
            bool        shifted;
        };
        const Stop stops[] = {
            { "GRID", DCC::EN_MAINMODE, true },          { "BROWSE", DCC::EN_BROWSE, false },
            { "TRACK MIX", DCC::EN_TRACK_MIX, false },   { "PAD MIX", DCC::EN_TRACK_MIX, true },
            { "TRACK MUTE", DCC::EN_TRACK_MUTE, false }, { "PAD MUTE", DCC::EN_TRACK_MUTE, true },
            { "NEXT SEQ", DCC::EN_NEXT_SEQ, false },     { "FX", DCC::EN_NEXT_SEQ, true },
            { "STEP SEQ", DCC::EN_STEP_SEQ, false },     { "AUTOMATION", DCC::EN_STEP_SEQ, true },
            { "TAP TEMPO", DCC::EN_TAP_TEMPO, false },   { "MASTER", DCC::EN_TAP_TEMPO, true },
            { "NOTE REPEAT", DCC::EN_NOTE_REPEAT, false }, { "LATCH", DCC::EN_NOTE_REPEAT, true },
            { "TC", DCC::EN_TC, false },                 { "ON-OFF", DCC::EN_TC, true },
            { "SAMPLER", DCC::EN_SAMPLER, false },       { "LOOPER", DCC::EN_SAMPLER, true },
            { "SAMPLE EDIT", DCC::EN_SAMPLE_EDIT, false }, { "LINK EDIT", DCC::EN_SAMPLE_EDIT, true },
            { "PROGRAM EDIT", DCC::EN_PROG_EDIT, false }, { "MIDI CONTROL", DCC::EN_PROG_EDIT, true },
            { "MENU", DCC::EN_MENU, false },             { "PREFS", DCC::EN_MENU, true },
            { "FULL LEVEL", DCC::EN_FULL_LEVEL, false }, { "HALF LEVEL", DCC::EN_FULL_LEVEL, true },
            { "16 LEVELS", DCC::EN_16LEVELS, false },    { "NOTES", DCC::EN_16LEVELS, true },
            { "COPY", DCC::EN_COPY, false },             { "LINK", DCC::EN_LINK, false },
            { "REC", DCC::EN_RECORD, false },            { "OVERDUB", DCC::EN_OVERDUB, false },
            { "PLAY START", DCC::EN_PLAYSTART, false },  { "STOP", DCC::EN_STOP, false },
        };
        // KICKEM_TOUR_FROM=<label> starts the button tour at that stop.
        auto from    = juce::SystemStats::getEnvironmentVariable( "KICKEM_TOUR_FROM", {} );
        bool started = from.isEmpty();
        for( const auto& stop : stops )
        {
            started = started || from == stop.label;
            if( started )
                visit( stop.label, stop.code, stop.shifted );
        }

        s.push_back( [log] { log( "done" ); } );
        wait( 2 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );

        startTimer( 400 );
    }

    void SimulatorBoard::startPopupTest()
    {
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        auto windowShot = [&s, this, log]( juce::String name ) {
            s.push_back( [this, log, name] {
                const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
                auto*      top = getTopLevelComponent();
                if( dir.isEmpty() || !top )
                    return;
                auto file = juce::File( dir ).getChildFile( name + ".png" );
                file.deleteFile();
                if( auto out = file.createOutputStream() )
                    juce::PNGImageFormat().writeImageToStream( top->createComponentSnapshot( top->getLocalBounds() ), *out );
                log( "window snapshot " + file.getFullPathName() );
            } );
        };

        wait( 4 );
        s.push_back( [log] {
            auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto source = exeDir.getChildFile( "templates/Template 01" );
            auto root   = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_popup" );
            root.deleteRecursively();
            auto scratch = root.getChildFile( source.getFileName() );
            const bool ok = source.copyDirectoryTo( scratch ) && mt::getAudioInterface().loadProject( scratch );
            log( juce::String( "loaded scratch template" ) + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );
        click( DCC::EN_MAINMODE ); // the Main screen's track row has list boxes (I/P MIDI PORT, channel)
        wait( 4 );
        s.push_back( [this, log] {
            std::function<ui::KickListBox*( juce::Component& )> find = [&find]( juce::Component& c ) -> ui::KickListBox* {
                if( auto* l = dynamic_cast<ui::KickListBox*>( &c ); l && l->isShowing() && l->isEnabled() )
                    return l;
                for( auto* child : c.getChildren() )
                    if( auto* hit = find( *child ) )
                        return hit;
                return nullptr;
            };
            if( auto* l = find( screen ) )
            {
                log( "opening list '" + l->getName() + "'" );
                l->knobClicked();
            }
            else
            {
                // Every pop-up goes through the same dialog code, so any content will do.
                log( "no list box showing; opening a dialog through mt::utils::showOptions" );
                auto list = std::make_unique<ui::KickList>( "Test", std::vector<juce::String> { "One", "Two", "Three" } );
                list->show( "TEST" );
                popupTestContent = std::move( list );
            }
        } );
        wait( 4 );
        windowShot( "popup_window" );
        s.push_back( [this, log] {
            auto* top = getTopLevelComponent();
            log( "window " + top->getLocalBounds().toString() + ", KICK screen in window " + top->getLocalArea( &screen, screen.getLocalBounds() ).toString() );
            for( auto* c : top->getChildren() )
                log( "  window child " + juce::String( typeid( *c ).name() ) + " bounds " + c->getBounds().toString() + ( c->isVisible() ? " visible" : "" ) );
            if( auto* modal = juce::Component::getCurrentlyModalComponent() )
            {
                log( "modal " + juce::String( typeid( *modal ).name() ) + " on desktop=" + juce::String( (int)modal->isOnDesktop() ) + " bounds in window "
                     + top->getLocalArea( modal, modal->getLocalBounds() ).toString() );
                for( auto* c : modal->getChildren() )
                    if( c->isVisible() )
                    {
                        auto r   = top->getLocalArea( c, c->getLocalBounds() );
                        auto hit = top->getComponentAt( r.getCentre() );
                        log( "  content " + juce::String( typeid( *c ).name() ) + " at " + r.toString() + "; a click at its centre reaches "
                             + ( hit ? juce::String( typeid( *hit ).name() ) : juce::String( "nothing" ) ) );
                    }
            }
            else
                log( "no modal component" );
            if( auto* modal = juce::Component::getCurrentlyModalComponent() )
                log( juce::String( "panel PLAY button clickable while open: " ) + ( modal->canModalEventBeSentToComponent( findButton( DCC::EN_PLAY ) ) ? "yes" : "no" )
                     + ", screen behind the dialog clickable: " + ( modal->canModalEventBeSentToComponent( &screen ) ? "yes" : "no" ) );
        } );
        // Drive the open list from the panel: jog +1 then ENTER should pick "Two" and close it.
        s.push_back( [this] { mainControl.getJog().turn( 1 ); } );
        wait( 1 );
        s.push_back( [this] { mainControl.getJog().pushCentre( true ); } );
        s.push_back( [this] { mainControl.getJog().pushCentre( false ); } );
        wait( 2 );
        s.push_back( [this, log] {
            auto* list = dynamic_cast<ui::KickList*>( popupTestContent.get() );
            log( "after jog +1 / ENTER: selected " + ( list ? list->getSelected() : juce::String( "?" ) ) + ", dialog "
                 + ( juce::Component::getCurrentlyModalComponent() ? "still open" : "closed" ) );
        } );
        s.push_back( [] { juce::Component::getCurrentlyModalComponent() ? juce::Component::getCurrentlyModalComponent()->exitModalState( 0 ) : void(); } );
        wait( 2 );
        s.push_back( [log] { log( "done" ); } );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );
        startTimer( 400 );
    }

    void SimulatorBoard::startPickerTest()
    {
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        // All components of a type under Root, in tree order.
        auto findAll = []( juce::Component* root, auto* typeTag ) {
            using T = std::remove_pointer_t<decltype( typeTag )>;
            std::vector<T*>                        found;
            std::function<void( juce::Component* )> walk = [&]( juce::Component* c ) {
                if( auto* t = dynamic_cast<T*>( c ) )
                    found.push_back( t );
                for( auto* child : c->getChildren() )
                    walk( child );
            };
            if( root )
                walk( root );
            return found;
        };

        wait( 4 );
        s.push_back( [log] {
            auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto source = exeDir.getChildFile( "projects/Trap Kit" );
            auto root   = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_picker" );
            root.deleteRecursively();
            auto       scratch = root.getChildFile( source.getFileName() );
            const bool ok      = source.copyDirectoryTo( scratch ) && mt::getAudioInterface().loadProject( scratch );
            log( juce::String( "loaded scratch Trap Kit" ) + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );
        click( DCC::EN_MAINMODE );
        wait( 4 );
        s.push_back( [this, log, findAll] {
            // The track row's program picker: its icon button opens the "Select Program" dialog.
            auto pickers = findAll( &screen, (kick_ui::KickProgramSelectorWithDisplay*)nullptr );
            for( auto* p : pickers )
                if( p->isShowing() )
                    for( auto* b : findAll( p, (ui::KickImageButton*)nullptr ) )
                        if( b->getName() == "Program" )
                        {
                            log( "opening the track's program picker" );
                            b->triggerClick();
                            return;
                        }
            log( "no program picker showing (" + juce::String( (int)pickers.size() ) + " found)" );
        } );
        wait( 4 );
        s.push_back( [this, log, findAll] {
            auto* modal = juce::Component::getCurrentlyModalComponent();
            auto  rows  = findAll( modal, (kick_ui::ProgramSelectorDisplay*)nullptr );
            auto  table = findAll( modal, (ui::KickTableView*)nullptr );
            if( rows.size() < 5 || table.empty() )
            {
                log( "picker dialog not found" );
                return;
            }
            // Click the centre of the Plugin row (row 4) the way the canvas forwards a real click:
            // the event's position is in the row component, which the simulator draws scaled.
            auto* row = rows[4];
            auto  src = juce::Desktop::getInstance().getMainMouseSource();
            auto  pos = row->getLocalBounds().getCentre().toFloat();
            juce::MouseEvent e( src, pos, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, row, row, juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false );
            static_cast<juce::Component*>( table[0] )->mouseDown( e );
            log( "clicked the Plugin row; table selected row " + juce::String( table[0]->getSelectedRow() ) + " (expected 4)" );
        } );
        wait( 3 );
        s.push_back( [log, findAll] {
            auto* modal = juce::Component::getCurrentlyModalComponent();
            for( auto* b : findAll( modal, (ui::KickToggleButton*)nullptr ) )
                if( b->getButtonText() == "+ New Plugin" && b->isShowing() )
                {
                    log( "picking '+ New Plugin'" );
                    b->triggerClick();
                    return;
                }
            log( "'+ New Plugin' option not shown" );
        } );
        wait( 2 );
        s.push_back( [log, findAll] {
            auto* modal = juce::Component::getCurrentlyModalComponent();
            for( auto* b : findAll( modal, (ui::KickImageButton*)nullptr ) )
                if( b->getName() == "Apply" && b->isShowing() )
                {
                    log( "Apply" );
                    b->triggerClick();
                    return;
                }
            log( "Apply button not found" );
        } );
        wait( 4 );
        s.push_back( [log] {
            auto& ae   = mt::getAudioInterface();
            auto* prog = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
            log( "track now plays " + ( prog ? prog->name.get() + " (" + mt::toString( prog->type.get() ) + ")" : juce::String( "nothing" ) ) );
        } );
        s.push_back( [log] { log( "done" ); } );
        wait( 1 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );
        startTimer( 400 );
    }

    void SimulatorBoard::startResourcesTest()
    {
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };

        wait( 4 );
        s.push_back( [log] {
            auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto source = exeDir.getChildFile( "projects/Vintage Soul Kit" );
            auto root   = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_resources" );
            root.deleteRecursively();
            auto       scratch = root.getChildFile( source.getFileName() );
            const bool ok      = source.copyDirectoryTo( scratch ) && mt::getAudioInterface().loadProject( scratch );
            log( juce::String( "loaded scratch kit" ) + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );
        click( DCC::EN_MAINMODE );
        wait( 3 );
        click( DCC::EN_PLAY );
        wait( 2 );
        // GRID (SHIFT + MAIN): its header has the chip button; Main's top bar doesn't.
        s.push_back( [this] { if( auto* b = findButton( DCC::EN_MAINMODE ) ) b->press( true, true ); } );
        s.push_back( [this] { if( auto* b = findButton( DCC::EN_MAINMODE ) ) b->press( false ); } );
        wait( 3 );
        s.push_back( [this, log] {
            // Open the page the way the header's chip button does.
            std::function<ui::KickImageButton*( juce::Component& )> find = [&find]( juce::Component& c ) -> ui::KickImageButton* {
                if( auto* b = dynamic_cast<ui::KickImageButton*>( &c ); b && b->getName() == "Resources" && b->isShowing() )
                    return b;
                for( auto* child : c.getChildren() )
                    if( auto* hit = find( *child ) )
                        return hit;
                return nullptr;
            };
            if( auto* b = find( screen ) )
            {
                log( "opening System Resources" );
                b->triggerClick();
            }
            else
                log( "Resources button not found" );
        } );
        // The page refreshes once a second; let it take a few readings while the kit plays.
        wait( 10 );
        s.push_back( [this, log] {
            std::function<void( juce::Component& )> dump = [&]( juce::Component& c ) {
                if( auto* sl = dynamic_cast<ui::KickSlider*>( &c ); sl && sl->isShowing() )
                    log( "  " + sl->getName() + " = " + juce::String( sl->getValue(), 1 ) + " %" );
                for( auto* child : c.getChildren() )
                    dump( *child );
            };
            if( auto* modal = juce::Component::getCurrentlyModalComponent() )
                dump( *modal );
            log( juce::String( "playing = " ) + ( mt::getAudioInterface().isSequencePlaying() ? "yes" : "no" ) );
            const auto dir = juce::SystemStats::getEnvironmentVariable( "KICKEM_SHOT", {} );
            if( dir.isNotEmpty() )
                if( auto out = juce::File( dir ).getChildFile( "system_resources.png" ).createOutputStream() )
                    juce::PNGImageFormat().writeImageToStream( screen.createComponentSnapshot( screen.getLocalBounds() ), *out );
        } );
        // Leave time for an outside reading of this process before quitting.
        wait( 8 );
        click( DCC::EN_STOP );
        s.push_back( [] { juce::Component::getCurrentlyModalComponent() ? juce::Component::getCurrentlyModalComponent()->exitModalState( 0 ) : void(); } );
        s.push_back( [log] { log( "done" ); } );
        wait( 1 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );
        startTimer( 400 );
    }

    void SimulatorBoard::startRecordTest()
    {
        auto& s    = selfTestSteps;
        auto  log  = []( juce::String text ) { SimulatorInterfaceImpl::log( "SELFTEST " + text ); };
        auto  wait = [&s]( int n ) { for( int i = 0; i < n; ++i ) s.push_back( [] {} ); };
        auto  click = [&s, this]( int code ) {
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( true ); } );
            s.push_back( [this, code] { if( auto* b = findButton( code ) ) b->press( false ); } );
        };
        // Starts of the selected track's notes on Note.
        auto starts = []( int Note ) {
            auto&                    ae = mt::getAudioInterface();
            std::vector<mt::MTNote>  notes;
            std::vector<int>         result;
            ae.getNotesInRange( ae.getSelected(), notes, mt::NotesInRange( Note, Note ) );
            for( auto& n : notes )
                result.push_back( n.start.get() );
            std::sort( result.begin(), result.end() );
            return result;
        };
        static std::vector<int> before; // shared between steps
        constexpr int           pad  = 2; // bank A pad 3 = note 2
        constexpr int           note = 2;

        wait( 4 );
        s.push_back( [log] {
            auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto source = exeDir.getChildFile( "projects/Trap Kit" );
            auto root   = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kickem_record" );
            root.deleteRecursively();
            auto       scratch = root.getChildFile( source.getFileName() );
            const bool ok      = source.copyDirectoryTo( scratch ) && mt::getAudioInterface().loadProject( scratch );
            log( juce::String( "loaded scratch Trap Kit" ) + ( ok ? "" : " (FAILED)" ) );
        } );
        wait( 8 );
        click( DCC::EN_MAINMODE );
        wait( 3 );
        s.push_back( [log, starts] {
            auto& ae = mt::getAudioInterface();
            before   = starts( note );
            auto* tm = ae.getTempoMgr( ae.getSelectedSequence() );
            log( "note " + juce::String( note ) + " has " + juce::String( (int)before.size() ) + " events before recording; grid "
                 + juce::String( tm ? tm->getTicksPerQuantization() : 0 ) + " ticks, " + juce::String( tm ? tm->getBPM() : 0 ) + " BPM" );
        } );
        s.push_back( [log] { log( "press REC" ); } );
        click( DCC::EN_RECORD );
        wait( 1 );
        s.push_back( [this, log] {
            auto& ae = mt::getAudioInterface();
            log( juce::String( "recording = " ) + ( ae.isSequenceRecording() ? "yes" : "no" ) + ", playing = " + ( ae.isSequencePlaying() ? "yes" : "no" )
                 + ", REC LED = " + ( findButton( DCC::EN_RECORD ) && findButton( DCC::EN_RECORD )->isLedOn() ? "on" : "off" ) );
        } );
        // Five hits, a little under a second apart (timer steps are 400 ms), held ~200-400 ms.
        for( int i = 0; i < 5; ++i )
        {
            s.push_back( [this] { pads.getPad( pad ).hit( true, 90 + 5 * 0 ); } );
            s.push_back( [this] { pads.getPad( pad ).hit( false ); } );
        }
        wait( 2 );
        s.push_back( [log] { log( "press STOP" ); } );
        click( DCC::EN_STOP );
        wait( 2 );
        s.push_back( [this, log, starts] {
            auto&      ae    = mt::getAudioInterface();
            const auto after = starts( note );
            auto*      tm    = ae.getTempoMgr( ae.getSelectedSequence() );
            const int  grid  = tm ? juce::jmax( 1, tm->getTicksPerQuantization() ) : 1;
            juce::StringArray added;
            int               offGrid = 0;
            for( auto t : after )
                if( std::find( before.begin(), before.end(), t ) == before.end() )
                {
                    added.add( juce::String( t ) );
                    offGrid += ( t % grid ) != 0;
                }
            log( "recording = " + juce::String( ae.isSequenceRecording() ? "yes" : "no" ) + ", REC LED = "
                 + ( findButton( DCC::EN_RECORD ) && findButton( DCC::EN_RECORD )->isLedOn() ? "on" : "off" ) );
            log( "note " + juce::String( note ) + " now has " + juce::String( (int)after.size() ) + " events; new at ticks [" + added.joinIntoString( ", " ) + "], "
                 + juce::String( offGrid ) + " off the grid" );
            ae.getUndoManager().undo();
            log( "after one undo: " + juce::String( (int)starts( note ).size() ) + " events (expected " + juce::String( (int)before.size() ) + ")" );
        } );

        // Sampler: panel REC arms, recording starts when the input crosses the threshold, REC again stops
        // (or cancels while still waiting for signal).
        auto inputLevel = [] {
            float peak = -100.f;
            for( auto& m : mt::getAudioInterface().getRecorderInputMeter() )
                peak = juce::jmax( peak, m.peak );
            return peak;
        };
        auto samplerState = [log, inputLevel]( juce::String when ) {
            auto& ae = mt::getAudioInterface();
            log( "sampler " + when + ": running = " + juce::String( ae.isRecorderRunning() ? "yes" : "no" ) + ", progress " + juce::String( ae.getRecorderProgress(), 3 )
                 + ", input peak " + juce::String( inputLevel(), 1 ) + " dB, threshold " + juce::String( ae.getRecorderThresholdDB(), 1 ) + " dB" );
        };
        s.push_back( [log] { log( "press SAMPLER, then REC" ); } );
        click( DCC::EN_SAMPLER );
        wait( 3 );
        click( DCC::EN_RECORD );
        wait( 5 );
        s.push_back( [samplerState] { samplerState( "2 s after REC" ); } );
        s.push_back( [log, inputLevel] {
            auto& ae = mt::getAudioInterface();
            if( ae.getRecorderProgress() <= 0.0f )
                log( "nothing above the threshold yet; REC again should cancel the arm" );
        } );
        click( DCC::EN_RECORD );
        wait( 2 );
        s.push_back( [samplerState] { samplerState( "after second REC" ); } );
        // If the first take never triggered (quiet room), retry with the threshold at its minimum.
        s.push_back( [log] {
            auto& ae = mt::getAudioInterface();
            if( ae.getRecorderProgress() <= 0.0f )
            {
                log( "retrying with the threshold at -100 dB" );
                ae.setRecorderThresholdDB( -100.f );
            }
        } );
        click( DCC::EN_RECORD );
        wait( 5 );
        s.push_back( [samplerState] { samplerState( "2 s into retry" ); } );
        click( DCC::EN_RECORD );
        wait( 2 );
        s.push_back( [samplerState] { samplerState( "stopped" ); } );
        s.push_back( [log] { log( "done" ); } );
        wait( 1 );
        s.push_back( [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); } );
        startTimer( 400 );
    }

    void SimulatorBoard::startPanelTest()
    {
        auto failures = std::make_shared<int>(0);
        auto check = [failures](bool ok, const juce::String& what) {
            if(!ok) ++*failures;
            SimulatorInterfaceImpl::log("SELFTEST " + juce::String(ok ? "PASS " : "FAIL ") + what);
        };
        auto& steps = selfTestSteps;
        steps.push_back([] {}); // wait for the wrapper and window to finish construction
        steps.push_back([this, check] {
            const auto mode = juce::SystemStats::getEnvironmentVariable("KICKEM_SELFTEST", {});
            const auto overrideLayout = juce::SystemStats::getEnvironmentVariable("KICKEM_LAYOUT", {});
            const bool saved = mt::getSettings().getState().getChildWithName("SIMULATOR").getProperty("hardwarePanel", true);
            const bool expected = mode == "panel-preference" ? false : overrideLayout == "hardware" ? true : overrideLayout == "original" ? false : saved;
            check(isHardwareLayout() == expected, "startup layout / saved preference / environment override");
            check(artwork.isValid(), "embedded SVG and all 57 named regions load");
        });
        steps.push_back([check] {
            const auto source = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("templates/Template 01");
            auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kickem_panel", "", false).getChildFile("Template 01");
            check(source.copyDirectoryTo(scratch) && mt::getAudioInterface().loadProject(scratch), "load scratch project for live-screen checks");
        });
        for(int i = 0; i < 10; ++i) steps.push_back([] {});
        steps.push_back([this] { findButton(DCC::EN_MAINMODE)->press(true); findButton(DCC::EN_MAINMODE)->press(false); });
        for(int i = 0; i < 4; ++i) steps.push_back([] {});
        for(bool hardware : {true, false, true})
        {
            steps.push_back([this, check, hardware] {
                auto* wrapper = findParentComponentOfClass<Simulator>();
                check(wrapper != nullptr, "simulator wrapper exists");
                if(!wrapper) return;
                auto& engine = mt::getAudioInterface();
                const std::array<int, 3> selection {engine.getSelectedSequence(), engine.getSelectedTrack(), engine.getSelectedProgram()};
                // Exercise the actual toolbar handlers, including persistence and window resizing.
                bool clicked = false;
                for(auto* child : wrapper->getChildren())
                    if(auto* button = dynamic_cast<SimButton*>(child))
                        if(button->getTooltip() == (hardware ? "Use the hardware panel layout" : "Use the original wide simulator layout"))
                        {
                            button->press(true);
                            button->press(false);
                            clicked = true;
                        }
                check(clicked && isHardwareLayout() == hardware, "toolbar switches to " + juce::String(hardware ? "hardware" : "original"));
                check(engine.isProjectLoaded() && selection == std::array<int, 3>{engine.getSelectedSequence(), engine.getSelectedTrack(), engine.getSelectedProgram()},
                    "loaded project and selected sequence / track / program survive layout switch");
                check(screen.getWidth() == 1024 && screen.getHeight() == 600, "firmware screen retains native resolution");
                check(getLocalArea(&screen, screen.getLocalBounds()) == (hardware ? artwork.bounds("screen") : screenRect), "screen occupies the correct aperture");

                auto reachable = [this](juce::Component& control, juce::Point<int> local) {
                    auto* target = getComponentAt(getLocalPoint(&control, local));
                    return target == &control;
                };
                for(int i = 0; i < 16; ++i)
                {
                    auto& pad = pads.getPad(i);
                    check(reachable(pad, pad.getLocalBounds().getCentre()), "pad " + juce::String(i + 1) + " receives clicks");
                    if(hardware) check(getLocalArea(&pad, pad.getLocalBounds()) == artwork.bounds("pad-" + juce::String(i)), "pad numbering follows hardware");
                }
                std::vector<SimButton*> buttons {&fullLevelBtn, &levels16Btn, &eraseBtn, &linkBtn, &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn};
                mainControl.collectButtons(buttons);
                for(auto* button : buttons)
                {
                    auto point = button->getLocalBounds().getCentre();
                    if(hardware)
                    {
                        const int code = button->getCode();
                        if(code == DCC::EN_BACK) point = {6, 6};
                        if(code == DCC::EN_COPY) point = {button->getWidth() - 6, 6};
                        if(code == DCC::EN_MINUS) point = {6, button->getHeight() - 6};
                        if(code == DCC::EN_PLUS) point = {button->getWidth() - 6, button->getHeight() - 6};
                    }
                    check(reachable(*button, point), "button " + button->getTooltip() + " receives clicks");
                }
                for(auto& knob : qlinks) check(reachable(*knob, knob->getLocalBounds().getCentre()), "Q-link receives clicks");
                check(reachable(mainControl.getJog(), mainControl.getJog().getLocalBounds().getCentre()), "jog centre receives clicks");
                check(reachable(slider, slider.getLocalBounds().getCentre()), "touch strip receives clicks");
            });
            steps.push_back([this, check, hardware] {
                auto* wrapper = findParentComponentOfClass<Simulator>();
                if(!wrapper) return;
                // Hit-test in window coordinates too: covers both layers of screen/panel scaling.
                auto* top = getTopLevelComponent();
                const auto point = top->getLocalPoint(&pads.getPad(0), pads.getPad(0).getLocalBounds().getCentre());
                check(top->getComponentAt(point) == &pads.getPad(0), "scaled window click reaches pad 1");
                const auto path = juce::SystemStats::getEnvironmentVariable("KICKEM_SHOT", {});
                if(path.isNotEmpty())
                {
                    const auto file = juce::File(path).getChildFile(hardware ? "hardware-panel.png" : "original-panel.png");
                    if(auto output = file.createOutputStream())
                        check(juce::PNGImageFormat().writeImageToStream(wrapper->createComponentSnapshot(wrapper->getLocalBounds()), *output), "panel screenshot saved");
                }
            });
        }
        steps.push_back([this, check] {
            std::vector<CPktMgr::HWMsg_t> packets;
            sigslot::scoped_connection capture = Controls::InputEvents::dispatchHardwareEvent.connect([&packets](CPktMgr::HWMsg_t& msg) { packets.push_back(msg); });
            ShiftLatch::setLatched(false);
            packets.clear();
            mainControl.getShiftButton().press(true);
            mainControl.getShiftButton().press(false);
            findButton(DCC::EN_BANK_A)->press(true);
            findButton(DCC::EN_BANK_A)->press(false);
            check(packets.size() == 4 && packets[0].data == hw::buttonMessage(DCC::EN_SHIFT, DCC::BTN_PRESSED).data
                && packets[1].data == hw::buttonMessage(DCC::EN_BANK_E, DCC::BTN_PRESSED).data
                && packets[2].data == hw::buttonMessage(DCC::EN_BANK_E, DCC::BTN_RELEASED).data
                && packets[3].data == hw::buttonMessage(DCC::EN_SHIFT, DCC::BTN_RELEASED).data, "Shift bank E press/release enters firmware input path");
            packets.clear();
            pads.getPad(0).hit(true, 96);
            pads.getPad(0).press(72);
            pads.getPad(0).hit(false);
            check(packets.size() == 3 && packets[0].data == hw::padMessage(0, DCC::BTN_PRESSED, 96).data
                && packets[1].data == hw::padMessage(0, DCC::BTN_LONGPRESSED, 96, 72).data
                && packets[2].data == hw::padMessage(0, DCC::BTN_RELEASED, 0).data, "pad velocity / pressure / release enter firmware input path");
            packets.clear();
            qlinks[0]->turn(2);
            mainControl.getJog().turn(-3);
            mainControl.getJog().pushCentre(true);
            mainControl.getJog().pushCentre(false);
            slider.setTouch(37);
            check(packets.size() == 5 && packets[0].data == hw::knobMessage(DCC::EN_KNOB_1, 2).data
                && packets[1].data == hw::knobMessage(DCC::EN_KNOB_CENTER, -3).data
                && packets[2].data == hw::buttonMessage(DCC::EN_KNOB_ENTER, DCC::BTN_PRESSED).data
                && packets[3].data == hw::buttonMessage(DCC::EN_KNOB_ENTER, DCC::BTN_RELEASED).data
                && packets[4].data == hw::sliderMessage(37).data, "Q-link / jog / Enter / touch strip enter firmware input path");
        });
        steps.push_back([this, check] {
            auto* wrapper = findParentComponentOfClass<Simulator>();
            auto* originalScreen = &screen;
            auto* originalPad = &pads.getPad(0);
            const auto screenChildren = screen.getChildren();
            auto* button = findButton(DCC::EN_LINK);
            button->setLed(true);
            ShiftLatch::setLatched(true);
            wrapper->setHardwareLayout(false);
            wrapper->setHardwareLayout(true);
            check(button->isLedOn() && ShiftLatch::isLatched() && mainControl.getShiftButton().isLedOn(), "LEDs and latched Shift survive layout changes");
            check(&screen == originalScreen && &pads.getPad(0) == originalPad && screen.getChildren() == screenChildren, "layout changes retain screen and control instances");
            ShiftLatch::setLatched(false);
            wrapper->setHardwareLayout(false); // verified by a second process using panel-preference
        });
        steps.push_back([this] {
            auto list = std::make_unique<ui::KickList>("Layout switch", std::vector<juce::String>{"One", "Two"});
            list->show("LAYOUT SWITCH");
            popupTestContent = std::move(list);
        });
        steps.push_back([this, check] {
            auto* wrapper = findParentComponentOfClass<Simulator>();
            auto* modal = juce::Component::getCurrentlyModalComponent();
            check(wrapper && modal, "open a dialog before changing layouts");
            if(!wrapper || !modal) return;
            auto* top = getTopLevelComponent();
            for(bool hardware : {true, false})
            {
                wrapper->setHardwareLayout(hardware);
                check(juce::Component::getCurrentlyModalComponent() == modal, "layout switch retains the open dialog");
                check(top->getLocalArea(modal, modal->getLocalBounds()) == top->getLocalArea(&screen, screen.getLocalBounds()),
                    "open dialog follows the screen aperture through layout changes");
                bool allowed = true;
                for(auto* child : wrapper->getChildren())
                    if(dynamic_cast<SimButton*>(child)) allowed &= modal->canModalEventBeSentToComponent(child);
                check(allowed, "layout toolbar remains usable with a dialog open");
            }
            modal->exitModalState(0);
        });
        steps.push_back([] {});
        steps.push_back([failures] {
            SimulatorInterfaceImpl::log("SELFTEST panel done: " + juce::String(*failures) + " failures");
            juce::JUCEApplication::getInstance()->setApplicationReturnValue(*failures);
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        });
        startTimer(250);
    }

    void SimulatorBoard::timerCallback()
    {
        if( selfTestIndex < selfTestSteps.size() )
            selfTestSteps[selfTestIndex++]();
        else
            stopTimer();
    }

    SimulatorBoard::~SimulatorBoard()
    {
        mt::MTMidiDeviceManager::simulatorInputFilter = {};
        midiDevicesConnection.disconnect();
        midiOverlay.reset();
        midiLearn.reset(); // release held controls while panel/engine still exist
        ShiftLatch::onChange = nullptr;
        juce::Desktop::getInstance().removeGlobalMouseListener( &mouseTrace );
        // KickHome is a process-wide singleton: detach it, don't let it be destroyed with the board.
        removeChildComponent( &screen );
    }

    void SimulatorBoard::handleOutput( const CPktMgr::HWMsg_t& m )
    {
        const auto& d = m.data;
        switch( m.pktType )
        {
            case CPktMgr::EN_PKT_TYPE_LED:
                // EN_LED_SET_SINGLE_LED / EN_LED_BLINK_SINGLE : { id, state, colour }
                if( ( m.pktCmd == DCC::EN_LED_SET_SINGLE_LED || m.pktCmd == DCC::EN_LED_BLINK_SINGLE ) && d.size() >= 2 )
                    setButtonLed( d[0], d[1] != DCC::StateOff );
                break;

            case CPktMgr::EN_PKT_TYPE_PAD:
                pads.applyPadPacket( m );
                break;

            case CPktMgr::EN_PKT_TYPE_SLIDER:
                if( ( m.pktCmd == DCC::EN_SLIDER_BAR_LED || m.pktCmd == DCC::EN_SLIDER_LEVEL_LED ) && !d.empty() )
                    slider.setLedLevel( d[0] );
                break;

            default: break;
        }
    }

    void SimulatorBoard::setButtonLed( int id, bool on )
    {
        if( id >= DCC::EN_BANK_A && id <= DCC::EN_BANK_H )
        {
            if( !on )
                return;
            // Bank LEDs are exclusive; E-H share the A-D buttons.
            auto banks = mainControl.getBankButtons();
            for( int i = 0; i < 4; ++i )
                banks[(size_t)i]->setLed( i == id % 4 );
            pads.setBankName( juce::String::charToString( (juce::juce_wchar)( 'A' + id ) ) );
            return;
        }

        if( on && ( id == DCC::EN_PLAY || id == DCC::EN_STOP || id == DCC::EN_PLAYSTART ) )
        {
            playBtn.setLed( id == DCC::EN_PLAY );
            stopBtn.setLed( id == DCC::EN_STOP );
            playStartBtn.setLed( id == DCC::EN_PLAYSTART );
            return;
        }

        if( on && ( id == DCC::EN_FULL_LEVEL || id == DCC::EN_HALF_LEVEL ) )
        {
            fullLevelBtn.setLed( true );
            return;
        }

        auto range = ledMap.equal_range( id );
        for( auto it = range.first; it != range.second; ++it )
            it->second->setLed( on );
    }

    void SimulatorBoard::paint( juce::Graphics& g )
    {
        if(hardwareLayout)
        {
            artwork.draw(g);
            // KickHome has transparent areas. Cover the SVG's standalone boot graphic
            // before the live screen paints, just as the physical LCD replaces it.
            g.setColour(juce::Colours::black);
            g.fillRect(artwork.bounds("screen"));
            return;
        }
        g.fillAll( palette::body );

        // Brand
        g.setColour( palette::text );
        g.setFont( juce::Font( 15.0f, juce::Font::bold ) );
        g.drawText( "KLARK TEKNIK", juce::Rectangle<int>( 24, 14, 300, 20 ), juce::Justification::centredLeft );
        g.setFont( juce::Font( 30.0f, juce::Font::bold ) );
        g.drawText( "KICK", juce::Rectangle<int>( 24, 32, 200, 32 ), juce::Justification::centredLeft );
        g.setColour( palette::text.withAlpha( 0.5f ) );
        g.setFont( juce::Font( 12.0f ) );
        g.drawText( "KickEM simulator - controls send front-panel MCU packets", juce::Rectangle<int>( 230, 38, 370, 24 ), juce::Justification::centredRight );

        // Wells
        g.setColour( palette::well );
        g.fillRoundedRectangle( juce::Rectangle<int>( 20, 70, 580, 310 ).toFloat(), 8.0f );
        g.fillRoundedRectangle( juce::Rectangle<int>( 1656, 14, 132, 556 ).toFloat(), 8.0f );
        g.fillRoundedRectangle( juce::Rectangle<int>( 1656, 580, 132, 360 ).toFloat(), 8.0f );

        // Screen bezel
        g.setColour( juce::Colours::black );
        g.fillRoundedRectangle( screenRect.expanded( 8 ).toFloat(), 6.0f );

        g.setColour( palette::text );
        g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
        g.drawText( "Q-LINK", juce::Rectangle<int>( 1656, 16, 132, 18 ), juce::Justification::centred );
        g.drawText( "TRANSPORT", juce::Rectangle<int>( 1656, 582, 132, 18 ), juce::Justification::centred );
    }

    void SimulatorBoard::resized()
    {
        if(midiOverlay) midiOverlay->setBounds(getLocalBounds());
        screen.setTransform({});
        if(hardwareLayout)
        {
            const auto aperture = artwork.bounds("screen");
            screen.setBounds(0, 0, 1024, 600);
            screen.setTransform(juce::AffineTransform::scale(aperture.getWidth() / 1024.f, aperture.getHeight() / 600.f)
                                .translated((float)aperture.getX(), (float)aperture.getY()));
            slider.setBounds(artwork.bounds("slider"));
            masterKnob.setBounds(artwork.bounds("master"));
            recKnob.setBounds(artwork.bounds("rec-gain"));
            fullLevelBtn.setBounds(artwork.bounds("full-level"));
            levels16Btn.setBounds(artwork.bounds("16-levels"));
            eraseBtn.setBounds(artwork.bounds("erase"));
            pads.setBounds(artwork.bounds("pads"));
            mainControl.setBounds(getLocalBounds());
            for(int i = 0; i < 4; ++i) qlinks[i]->setBounds(artwork.bounds("p" + juce::String(i + 1)));
            linkBtn.setBounds(artwork.bounds("link"));
            recBtn.setBounds(artwork.bounds("record"));
            overdubBtn.setBounds(artwork.bounds("overdub"));
            stopBtn.setBounds(artwork.bounds("stop"));
            playBtn.setBounds(artwork.bounds("play"));
            playStartBtn.setBounds(artwork.bounds("play-start"));
            return;
        }
        screen.setBounds( screenRect );

        slider.setBounds( 30, 78, 80, 296 );
        masterKnob.setBounds( 120, 80, 150, 140 );
        recKnob.setBounds( 120, 230, 150, 140 );
        fullLevelBtn.setBounds( 300, 90, 280, 80 );
        levels16Btn.setBounds( 300, 185, 280, 80 );
        eraseBtn.setBounds( 300, 280, 280, 80 );
        pads.setBounds( 20, 390, 580, 550 );

        mainControl.setBounds( 620, 640, 1024, 300 );

        for( int i = 0; i < 4; ++i )
            qlinks[(size_t)i]->setBounds( 1664, 36 + i * 118, 116, 112 );
        linkBtn.setBounds( 1666, 512, 112, 50 );

        int y = 604;
        for( auto* b : { &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn } )
        {
            b->setBounds( 1666, y, 112, 58 );
            y += 66;
        }
    }

    //==============================================================================
    // Simulator (scaling wrapper)
    //==============================================================================
    void SimulatorBoard::setHardwareLayout(bool enabled)
    {
        hardwareLayout = enabled && artwork.isValid();
        mainControl.setPanelArtwork(hardwareLayout ? &artwork : nullptr);
        pads.setPanelArtwork(hardwareLayout ? &artwork : nullptr);
        for(const auto& item : ledMap) item.second->setHardwareSkin(hardwareLayout);
        for(auto* b : {&recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn})
            b->setHardwareSkin(hardwareLayout, true);
        slider.setHardwareSkin(hardwareLayout);
        masterKnob.setHardwareSkin(hardwareLayout);
        recKnob.setHardwareSkin(hardwareLayout);
        for(auto& q : qlinks) q->setHardwareSkin(hardwareLayout);
        setSize(hardwareLayout ? PanelArtwork::width : boardWidth, hardwareLayout ? PanelArtwork::height : boardHeight);
        resized();
        repaint();
    }

    Simulator::Simulator()
    {
        addAndMakeVisible( board );
        addAndMakeVisible(hardwareButton);
        addAndMakeVisible(originalButton);
        addAndMakeVisible(learnButton);
        addAndMakeVisible(midiButton);
        addAndMakeVisible(midiStatus);
        setWantsKeyboardFocus(true);
        midiStatus.setFont(juce::Font(13.f));
        midiStatus.setColour(juce::Label::textColourId, palette::text);
        midiStatus.setInterceptsMouseClicks(false, false);
        board.onMidiChanged = [this] {
            learnButton.setLed(board.midi().isLearning());
            midiButton.setLed(board.midi().isEnabled() && board.midi().mappingCount() > 0);
            midiStatus.setText(board.midi().getStatus(), juce::dontSendNotification);
            midiStatus.setTooltip(board.midi().getStatus());
            if(board.midi().isLearning() && isShowing()) grabKeyboardFocus();
        };
        learnButton.onPress = [this](bool down) { if(!down) board.midi().setLearning(!board.midi().isLearning()); };
        midiButton.onPress = [this](bool down) { if(!down) board.midi().showMenu(&midiButton); };
        learnButton.setTooltip("Toggle MIDI learn: click a panel control, then move your controller. Esc cancels.");
        midiButton.setTooltip("View/remove MIDI mappings, bypass mappings or release held controls");
        board.onMidiChanged();
        hardwareButton.onPress = [this](bool down) { if(!down) setHardwareLayout(true); };
        originalButton.onPress = [this](bool down) { if(!down) setHardwareLayout(false); };
        hardwareButton.setTooltip("Use the hardware panel layout");
        originalButton.setTooltip("Use the original wide simulator layout");
        auto state = mt::getSettings().getState().getOrCreateChildWithName("SIMULATOR", nullptr);
        bool hardware = state.getProperty("hardwarePanel", true);
        const auto overrideLayout = juce::SystemStats::getEnvironmentVariable("KICKEM_LAYOUT", {});
        if(overrideLayout == "hardware" || overrideLayout == "original") hardware = overrideLayout == "hardware";
        setHardwareLayout(hardware, false);
        const auto size = preferredSize();
        setSize(size.x, size.y);
    }

    Simulator::~Simulator() { board.onMidiChanged = {}; }

    bool Simulator::keyPressed(const juce::KeyPress& key)
    {
        if(key == juce::KeyPress::escapeKey && board.midi().isLearning())
        { board.midi().setLearning(false); return true; }
        return false;
    }

    juce::Point<int> Simulator::preferredSize() const
    {
        return {board.getWidth(), board.getHeight() + toolbarHeight};
    }

    void Simulator::setHardwareLayout(bool enabled, bool persist)
    {
        const bool previous = board.isHardwareLayout();
        board.setHardwareLayout(enabled);
        hardwareButton.setLed(board.isHardwareLayout());
        originalButton.setLed(!board.isHardwareLayout());
        if(persist)
        {
            mt::getSettings().getState().getOrCreateChildWithName("SIMULATOR", nullptr)
                .setProperty("hardwarePanel", board.isHardwareLayout(), nullptr);
            mt::getSettings().save();
        }
        resized();
        if(onLayoutChanged && board.isHardwareLayout() != previous) onLayoutChanged();
    }

    void Simulator::paint( juce::Graphics& g )
    {
        g.fillAll( juce::Colour( 0xff202020 ) );
        g.setColour(juce::Colour(0xff151618));
        g.fillRect(0, 0, getWidth(), toolbarHeight);
        g.setColour(palette::text);
        g.setFont(juce::Font(14.f, juce::Font::bold));
        if(getWidth() >= 650) g.drawText("KICK SIMULATOR", 14, 0, getWidth() - 496, 44, juce::Justification::centredLeft);
    }

    void Simulator::resized()
    {
        const int buttonWidth = juce::jmin(112, (getWidth() - 38) / 4);
        const int left = getWidth() - 14 - (buttonWidth * 4 + 24);
        learnButton.setBounds(left, 7, buttonWidth, 30);
        midiButton.setBounds(left + buttonWidth + 8, 7, buttonWidth, 30);
        hardwareButton.setBounds(left + (buttonWidth + 8) * 2, 7, buttonWidth, 30);
        originalButton.setBounds(left + (buttonWidth + 8) * 3, 7, buttonWidth, 30);
        midiStatus.setBounds(10, 42, getWidth() - 20, 30);
        const float bw = (float)board.getWidth(), bh = (float)board.getHeight();
        const float availableHeight = (float)juce::jmax(1, getHeight() - toolbarHeight);
        const float scale = juce::jmax(0.01f, juce::jmin(getWidth() / bw, availableHeight / bh));
        board.setTransform(juce::AffineTransform::scale(scale).translated((getWidth() - bw * scale) * 0.5f,
                           toolbarHeight + (availableHeight - bh * scale) * 0.5f));
    }
}
