/*
 * KickEngineTest: headless checks for the Kick audio engine on a desktop.
 *
 *  1. Unit tests for the reconstructed AudIO_juce objects (SampleRecorder,
 *     Looper, Filter, ADSR), fed with synthetic audio.
 *  2. An end-to-end run through mt::getAudioInterface(): load a drum kit onto
 *     pads, hit the pads the way the hardware does, and measure what reaches
 *     the audio device; then drive the sampler and looper through the API.
 *
 * Results go to engine_test_results.txt next to the executable. The exit code
 * is the number of failed checks. Pass a folder of WAVs as the first argument
 * to choose the kit (default: samples/Test Kit next to the executable).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "audio_engine/audio_engine.h"
#include "hardware/hardware.h"
#include "input_events/input_events.h"
#include "audio_engine/engine/mt_sample_editor_actions.h"
#include "audio_engine/engine/mt_sample_player.h"
#include "../../../AudIO/AudIO/Core/Chain.h"

#if JUCE_WINDOWS
    #include <windows.h>
    #include <dbghelp.h>
    #pragma comment( lib, "dbghelp.lib" )

// Writes a symbolised stack trace next to the executable if the process crashes.
static LONG WINAPI writeCrashTrace( EXCEPTION_POINTERS* Info )
{
    auto process = GetCurrentProcess();
    SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_UNDNAME );
    SymInitialize( process, nullptr, TRUE );

    CONTEXT      ctx   = *Info->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset    = ctx.Rip;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

    juce::String trace;
    trace << "exception 0x" << juce::String::toHexString( (int)Info->ExceptionRecord->ExceptionCode ) << "\n";
    for( int i = 0; i < 40 && StackWalk64( IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &ctx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr ); ++i )
    {
        char         buf[sizeof( SYMBOL_INFO ) + 512] = {};
        auto*        sym = (SYMBOL_INFO*)buf;
        sym->SizeOfStruct = sizeof( SYMBOL_INFO );
        sym->MaxNameLen   = 511;
        DWORD64         disp = 0;
        DWORD           lineDisp = 0;
        IMAGEHLP_LINE64 line = { sizeof( IMAGEHLP_LINE64 ) };
        trace << "  " << ( SymFromAddr( process, frame.AddrPC.Offset, &disp, sym ) ? juce::String( sym->Name ) : juce::String( "?" ) );
        if( SymGetLineFromAddr64( process, frame.AddrPC.Offset, &lineDisp, &line ) )
            trace << "  " << juce::File( line.FileName ).getFileName() << ":" << (int)line.LineNumber;
        trace << "\n";
    }
    juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getSiblingFile( "engine_test_crash.txt" ).replaceWithText( trace );
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

namespace
{
    constexpr uint32_t testRate  = 48000;
    constexpr uint32_t testBlock = 128;

    struct Results
    {
        juce::StringArray lines;
        int               failed{ 0 }, passed{ 0 };

        void section( const juce::String& Name ) { lines.add( "\n== " + Name ); }
        void note( const juce::String& Text ) { lines.add( "   " + Text ); }
        bool check( bool Ok, const juce::String& Name, const juce::String& Detail = {} )
        {
            ( Ok ? passed : failed )++;
            lines.add( juce::String( Ok ? "PASS " : "FAIL " ) + Name + ( Detail.isNotEmpty() ? "  [" + Detail + "]" : juce::String() ) );
            return Ok;
        }
    };

    void pump( int Ms ) { juce::MessageManager::getInstance()->runDispatchLoopUntil( Ms ); }

    float rms( const float* d, int n )
    {
        double s = 0;
        for( int i = 0; i < n; ++i )
            s += (double)d[i] * d[i];
        return n > 0 ? (float)std::sqrt( s / n ) : 0.f;
    }

    juce::String f3( double v ) { return juce::String( v, 3 ); }

    // Feeds a sine into a two-channel AudIO buffer, one block at a time.
    struct SineSource
    {
        // 375 Hz is exactly one cycle per 128-sample block, so every loop pass
        // lines up in phase and overdubs sum predictably.
        double phase{ 0 }, freq{ 375 };
        float  ampL{ 0.5f }, ampR{ 0.25f };

        void fill( AudIO::Buffer<float>& B, bool Silent = false )
        {
            auto* l = B.getInputBuffer( 0 );
            auto* r = B.getInputBuffer( 1 );
            for( uint32_t i = 0; i < testBlock; ++i )
            {
                const auto s = (float)std::sin( phase );
                phase += juce::MathConstants<double>::twoPi * freq / testRate;
                l[i] = Silent ? 0.f : s * ampL;
                r[i] = Silent ? 0.f : s * ampR;
            }
        }
    };

    struct TestSampleRecorder : public AudIO::SampleRecorder
    {
        using SampleRecorder::SampleRecorder;
        void prep() { prepareToPlay( testRate, testBlock ); }
        void run( AudIO::Buffer<float>& B ) { process( B ); }
    };

    struct TestLooper : public AudIO::Looper
    {
        using Looper::Looper;
        void prep() { prepareToPlay( testRate, testBlock ); }
        void run( AudIO::Buffer<float>& B ) { process( B ); }
    };

    //==============================================================================
    // Objects are made in different modules (AudIO buses, AudIO_juce recorders, engine
    // instruments). They must still get distinct IDs: a Design refuses an ID it already holds.
    void testObjectIds( Results& R )
    {
        R.section( "AudIO object IDs" );

        AudIO::Design      design( 2, 2 );
        std::set<uint32_t> ids;
        int                made = 0;
        auto               add  = [&]( uint32_t id ) {
            ids.insert( id );
            ++made;
        };
        for( int i = 0; i < 32; ++i )
        {
            add( design.createBus() );
            {
                TestSampleRecorder rec( design );
                add( rec.getId() );
            }
            {
                mt::InstrumentObject ins( design, "id test" );
                add( ins.getId() );
            }
        }
        R.check( (int)ids.size() == made, "objects from different modules get distinct IDs", juce::String( (int)ids.size() ) + " distinct of " + juce::String( made ) );
    }

    void testSampleRecorder( Results& R )
    {
        R.section( "SampleRecorder (synthetic input)" );

        AudIO::Design        design( 2, 2 );
        TestSampleRecorder   rec( design );
        AudIO::Buffer<float> buf( 2, true, 2, true );
        SineSource           sine;
        rec.prep();

        bool   stopped = false;
        double lengthCb{ -1 };
        rec.onRecorderStopped       = [&]() { stopped = true; };
        rec.onRecorderLengthChanged = [&]( double t ) { lengthCb = t; };

        // Fill a 0.5 s buffer to the end.
        rec.setRecordingMode( AudIO::Recorder::LR_STEREO );
        rec.setThreshold( 0.f );
        rec.setGainAndPan( 1.f, 0.f );
        rec.setLength( 0.5 );
        rec.startRecording( true );
        R.check( rec.isRecording(), "starts recording" );

        int blocks = 0;
        while( rec.isRecording() && blocks++ < 400 )
        {
            sine.fill( buf );
            rec.run( buf );
        }
        pump( 50 ); // let the async stop run on the message thread

        auto& data = rec.getAudioBuffer();
        R.check( !rec.isRecording(), "stops by itself when the buffer is full", "blocks=" + juce::String( blocks ) );
        R.check( stopped, "onRecorderStopped fired" );
        R.check( data.getNumChannels() == 2 && data.getNumSamples() == (int)( 0.5 * testRate ), "buffer is 0.5 s stereo", juce::String( data.getNumChannels() ) + "ch x " + juce::String( data.getNumSamples() ) );
        R.check( std::abs( lengthCb - 0.5 ) < 0.001, "onRecorderLengthChanged reports 0.5 s", f3( lengthCb ) );

        const auto rmsL = rms( data.getReadPointer( 0 ), data.getNumSamples() );
        const auto rmsR = rms( data.getReadPointer( 1 ), data.getNumSamples() );
        R.check( std::abs( rmsL - 0.3536f ) < 0.01f && std::abs( rmsR - 0.1768f ) < 0.01f, "recorded levels match input (L 0.354, R 0.177)", "L " + f3( rmsL ) + " R " + f3( rmsR ) );
        R.check( std::abs( rec.readProgress() - 1.f ) < 0.01f, "progress is 100%", f3( rec.readProgress() ) );

        // Preview plays back what was recorded.
        rec.startPreview( 0.0 );
        sine.fill( buf, true );
        rec.run( buf );
        float maxErr = 0;
        for( uint32_t i = 0; i < testBlock; ++i )
            maxErr = juce::jmax( maxErr, std::abs( buf.getOutputBuffer( 0 )[i] - data.getSample( 0, (int)i ) ) );
        R.check( rec.isPreviewing() && maxErr < 1.0e-6f, "preview outputs the recording", "max error " + juce::String( maxErr ) );
        rec.stopPreview();

        // Saving writes a readable WAV of the same length.
        auto wavFile = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_recorder_test.wav" );
        rec.saveRecording( wavFile.getFullPathName().toStdString() );
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader( fm.createReaderFor( wavFile ) );
        R.check( reader && reader->lengthInSamples == data.getNumSamples() && reader->numChannels == 2, "saveRecording writes a matching WAV", reader ? juce::String( reader->lengthInSamples ) + " samples" : "unreadable" );
        reader.reset();
        wavFile.deleteFile();

        // Stopping early trims to what was recorded.
        rec.reset();
        rec.setLength( 1.0 );
        rec.startRecording( true );
        for( int b = 0; b < 100; ++b )
        {
            sine.fill( buf );
            rec.run( buf );
        }
        rec.stopRecording();
        R.check( data.getNumSamples() == 100 * (int)testBlock, "stopping early trims the buffer", juce::String( data.getNumSamples() ) + " samples" );

        // Mono modes record one channel; L_MONO takes the left input.
        rec.reset();
        rec.setRecordingMode( AudIO::Recorder::L_MONO );
        rec.setLength( 0.1 );
        rec.startRecording( true );
        while( rec.isRecording() )
        {
            sine.fill( buf );
            rec.run( buf );
        }
        pump( 20 );
        R.check( data.getNumChannels() == 1 && std::abs( rms( data.getReadPointer( 0 ), data.getNumSamples() ) - 0.3536f ) < 0.01f, "L_MONO records the left input into one channel", juce::String( data.getNumChannels() ) + "ch rms " + f3( rms( data.getReadPointer( 0 ), data.getNumSamples() ) ) );

        // Threshold holds recording back until the input is loud enough.
        rec.reset();
        rec.setRecordingMode( AudIO::Recorder::LR_STEREO );
        rec.setThreshold( 0.3f );
        rec.setLength( 0.5 );
        rec.startRecording( true );
        for( int b = 0; b < 20; ++b )
        {
            sine.fill( buf, true );
            rec.run( buf );
        }
        const auto progressQuiet = rec.readProgress();
        for( int b = 0; b < 20; ++b )
        {
            sine.fill( buf );
            rec.run( buf );
        }
        R.check( progressQuiet == 0.f && rec.readProgress() > 0.f, "threshold waits for loud input", "quiet " + f3( progressQuiet ) + " loud " + f3( rec.readProgress() ) );
        rec.stopRecording();
        rec.setThreshold( 0.f );

        // Reverse recording fills the buffer from the end.
        rec.reset();
        rec.setDirection( AudIO::Recorder::REVERSE );
        rec.setLength( 0.05 );
        rec.startRecording( true );
        float ramp = 0;
        while( rec.isRecording() )
        {
            for( uint32_t i = 0; i < testBlock; ++i, ramp += 1.f / testRate )
                buf.getInputBuffer( 0 )[i] = buf.getInputBuffer( 1 )[i] = ramp;
            rec.run( buf );
        }
        pump( 20 );
        R.check( data.getNumSamples() > 0 && data.getSample( 0, 0 ) > data.getSample( 0, data.getNumSamples() - 1 ), "reverse recording runs backwards", "first " + f3( data.getNumSamples() ? data.getSample( 0, 0 ) : 0 ) + " last " + f3( data.getNumSamples() ? data.getSample( 0, data.getNumSamples() - 1 ) : 0 ) );
        rec.setDirection( AudIO::Recorder::FORWARD );
    }

    //==============================================================================
    void testLooper( Results& R )
    {
        R.section( "Looper (synthetic input)" );

        AudIO::Design        design( 2, 2 );
        TestLooper           lp( design );
        AudIO::Buffer<float> buf( 2, true, 2, true );
        SineSource           sine;
        sine.ampL = sine.ampR = 0.5f;
        lp.prep();

        const int loopBlocks = 100;
        lp.setLength( loopBlocks * testBlock / (double)testRate );

        // Plays one loop with silent input; returns output RMS for L and R.
        auto playLoop = [&]() {
            std::vector<float> l, r;
            for( int b = 0; b < loopBlocks; ++b )
            {
                sine.fill( buf, true );
                lp.run( buf );
                l.insert( l.end(), buf.getOutputBuffer( 0 ), buf.getOutputBuffer( 0 ) + testBlock );
                r.insert( r.end(), buf.getOutputBuffer( 1 ), buf.getOutputBuffer( 1 ) + testBlock );
            }
            return std::make_pair( rms( l.data(), (int)l.size() ), rms( r.data(), (int)r.size() ) );
        };
        auto recordLoop = [&]() {
            for( int b = 0; b < loopBlocks; ++b )
            {
                sine.fill( buf );
                lp.run( buf );
            }
        };

        R.check( lp.isEmpty(), "starts empty" );
        lp.setRecord( true );
        lp.start( true, true );
        R.check( lp.isRunning() && lp.isRecording(), "record starts the loop" );
        recordLoop();
        R.check( !lp.isEmpty(), "has content after one pass" );

        lp.setPlay();
        R.check( lp.isPreviewing() && !lp.isRecording(), "setPlay switches to playback" );
        auto [l1, r1] = playLoop();
        R.check( std::abs( l1 - 0.3536f ) < 0.01f && std::abs( r1 - 0.3536f ) < 0.01f, "plays back what was recorded", "L " + f3( l1 ) + " R " + f3( r1 ) );

        lp.setOverdub( true );
        lp.start( false, true );
        recordLoop();
        lp.setPlay();
        auto [l2, r2] = playLoop();
        R.check( std::abs( l2 - 0.7071f ) < 0.02f, "overdub layers on top (same-phase sine doubles)", "L " + f3( l2 ) );

        lp.undo();
        auto [l3, r3] = playLoop();
        R.check( std::abs( l3 - 0.3536f ) < 0.01f, "undo removes the overdub", "L " + f3( l3 ) );
        lp.redo();
        auto [l4, r4] = playLoop();
        R.check( std::abs( l4 - 0.7071f ) < 0.02f, "redo restores it", "L " + f3( l4 ) );

        lp.setPan( 1.f );
        auto [l5, r5] = playLoop();
        R.check( l5 < 0.001f && std::abs( r5 - 0.7071f ) < 0.02f, "pan hard right silences the left", "L " + f3( l5 ) + " R " + f3( r5 ) );
        lp.setPan( 0.f );
        lp.setGain( 0.5f );
        auto [l6, r6] = playLoop();
        R.check( std::abs( l6 - 0.3536f ) < 0.01f, "playback gain applies", "L " + f3( l6 ) );
        lp.setGain( 1.f );

        lp.stop();
        auto [l7, r7] = playLoop();
        R.check( !lp.isRunning() && l7 < 0.001f, "stop silences the loop", "L " + f3( l7 ) );

        lp.clear();
        R.check( lp.isEmpty(), "clear empties it" );

        lp.setThreshold( 0.3f );
        lp.setRecord( true );
        lp.start( true, false );
        for( int b = 0; b < loopBlocks; ++b )
        {
            sine.fill( buf, true );
            lp.run( buf );
        }
        const bool emptyAfterQuiet = lp.isEmpty();
        recordLoop();
        R.check( emptyAfterQuiet && !lp.isEmpty(), "threshold waits for loud input" );
        lp.stop();
        lp.setThreshold( 0.f );

        // Undo of the very first take goes back to empty; redo brings it back.
        lp.clear();
        lp.setRecord( true );
        lp.start( true, true );
        recordLoop();
        lp.stop();
        lp.undo();
        const bool emptyAfterUndo = lp.isEmpty();
        lp.redo();
        R.check( emptyAfterUndo && !lp.isEmpty(), "first take can be undone and redone" );

        // Mono input + new length must still give a stereo loop (used to read past the buffer).
        lp.clear();
        lp.setRecordingMode( AudIO::Recorder::L_MONO );
        lp.setLength( 0.5 * loopBlocks * testBlock / (double)testRate );
        R.check( lp.getAudioBuffer().getNumChannels() == 2, "loop stays stereo with a mono input", juce::String( lp.getAudioBuffer().getNumChannels() ) + " ch" );
        lp.setRecord( true );
        lp.start( true, true );
        recordLoop();
        lp.setPlay();
        auto [lm, rm] = playLoop();
        R.check( lm > 0.1f && rm > 0.1f, "mono input plays on both sides", "L " + f3( lm ) + " R " + f3( rm ) );
        lp.stop();
        lp.setRecordingMode( AudIO::Recorder::LR_STEREO );
        lp.setLength( loopBlocks * testBlock / (double)testRate );

        lp.setMonitorOn( true );
        lp.setMonitorGain( 0.5f );
        sine.fill( buf );
        lp.run( buf );
        const auto mon = rms( buf.getOutputBuffer( 0 ), testBlock );
        R.check( std::abs( mon - 0.177f ) < 0.03f, "monitor passes input at monitor gain", f3( mon ) );
    }

    //==============================================================================
    float filterGain( mt::FilterType Type, float CutoffHz, float SignalHz, double Resonance = 0 )
    {
        mt::Filter f;
        f.setSampleRate( testRate );
        f.setFilterType( Type );
        f.setResonance( Resonance );
        f.setCutoff( mt::Filter::freqRange.convertTo0to1( CutoffHz ) );

        juce::AudioBuffer<float> frame( 2, 1 );
        double                   in2 = 0, out2 = 0;
        for( int i = 0; i < (int)testRate; ++i )
        {
            const auto x = (float)std::sin( juce::MathConstants<double>::twoPi * SignalHz * i / testRate );
            frame.setSample( 0, 0, x );
            frame.setSample( 1, 0, x );
            f.process( frame );
            if( i > (int)testRate / 4 ) // skip the settling time
            {
                in2 += x * x;
                out2 += frame.getSample( 0, 0 ) * frame.getSample( 0, 0 );
            }
        }
        return (float)std::sqrt( out2 / in2 );
    }

    void testFilter( Results& R )
    {
        R.section( "Voice filter" );
        using T = mt::FilterType;

        const auto lpStop = filterGain( T::LowPass4, 500, 5000 ), lpPass = filterGain( T::LowPass4, 500, 100 );
        R.check( lpStop < 0.01f && lpPass > 0.9f, "Lowpass 4 at 500 Hz: passes 100 Hz, cuts 5 kHz", "100Hz " + f3( lpPass ) + " 5kHz " + f3( lpStop ) );

        const auto hpStop = filterGain( T::HighPass2, 2000, 100 ), hpPass = filterGain( T::HighPass2, 2000, 10000 );
        R.check( hpStop < 0.01f && hpPass > 0.9f, "Highpass 2 at 2 kHz: cuts 100 Hz, passes 10 kHz", "100Hz " + f3( hpStop ) + " 10kHz " + f3( hpPass ) );

        const auto bpCentre = filterGain( T::BandPass2, 1000, 1000 ), bpOff = filterGain( T::BandPass2, 1000, 100 );
        R.check( bpCentre > 0.9f && bpOff < 0.2f, "Bandpass 2 at 1 kHz", "1kHz " + f3( bpCentre ) + " 100Hz " + f3( bpOff ) );

        const auto notch = filterGain( T::BandStop2, 1000, 1000 );
        R.check( notch < 0.05f, "Bandstop 2 notches its centre", f3( notch ) );

        const auto reso = filterGain( T::LowPass2, 1000, 1000, 90 );
        R.check( reso > 2.f, "resonance boosts the cutoff frequency", f3( reso ) );

        const auto lp1 = filterGain( T::LowPass1, 1000, 1000 );
        R.check( std::abs( lp1 - 0.707f ) < 0.03f, "Lowpass 1 is -3 dB at cutoff", f3( lp1 ) );

        // Every type stays finite with full resonance on noise.
        juce::Random rnd( 1 );
        bool         allFinite = true;
        juce::String bad;
        for( int t = 0; t <= (int)T::Kick3000LPF; ++t )
        {
            mt::Filter f;
            f.setSampleRate( testRate );
            f.setFilterType( (T)t );
            f.setResonance( 100 );
            juce::AudioBuffer<float> frame( 2, 1 );
            for( int i = 0; i < (int)testRate; ++i )
            {
                f.setCutoff( 0.5 + 0.5 * std::sin( i * 0.001 ) ); // sweep, as modulation does
                frame.setSample( 0, 0, rnd.nextFloat() * 2 - 1 );
                frame.setSample( 1, 0, rnd.nextFloat() * 2 - 1 );
                f.process( frame );
                if( !std::isfinite( frame.getSample( 0, 0 ) ) || std::abs( frame.getSample( 0, 0 ) ) > 100.f )
                {
                    allFinite = false;
                    bad << t << " ";
                    break;
                }
            }
        }
        R.check( allFinite, "all 27 types stay stable at full resonance with a swept cutoff", bad );
    }

    void testADSR( Results& R )
    {
        R.section( "Voice envelope" );
        mt::ADSR env;
        env.setParameters( 10, 5, 10, 10, 0.5 );
        env.gate( 1 );

        std::vector<float> v;
        for( int i = 0; i < 40; ++i )
            v.push_back( env() );
        R.check( std::abs( v[4] - 0.5f ) < 0.01f && v[9] == 1.f, "attack ramps to 1 over 10 samples", f3( v[4] ) + " " + f3( v[9] ) );
        R.check( v[12] == 1.f, "hold stays at 1" );
        R.check( std::abs( v[39] - 0.5f ) < 0.001f, "decays to sustain", f3( v[39] ) );

        env.gate( 0 );
        float last = 1;
        for( int i = 0; i < 10; ++i )
            last = env();
        R.check( last == 0.f && !env.isActive(), "release reaches 0 and goes idle", f3( last ) );

        mt::ADSR instant;
        instant.setParameters( 0, 0, 0, 0, 1 );
        instant.gate( 1 );
        R.check( instant() == 1.f, "zero attack is instant (pitch envelope default)" );
    }

    //==============================================================================
    void testBasicSynth( Results& R )
    {
        R.section( "Instrument engine: Basic Synth" );

        mt::BasicSynthEngine synth;
        synth.prepare( testRate, (int)testBlock );

        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        auto               render  = [&]( int Blocks ) {
            float peak = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( auto v : l )
                    peak = juce::jmax( peak, std::abs( v ) );
            }
            return peak;
        };

        R.check( render( 10 ) == 0.f, "silent with no notes" );
        R.check( synth.getParams().size() == mt::BasicSynthEngine::kNumParams, "exposes its parameter list", juce::String( (int)synth.getParams().size() ) + " params" );

        synth.noteOn( 60, 1.f );
        const auto held = render( 100 );
        R.check( held > 0.02f && held < 1.f, "note-on makes sound within range", "peak " + f3( held ) );

        synth.noteOff( 60 );
        render( (int)( 2.0 * testRate / testBlock ) ); // default release 0.3 s
        R.check( render( 10 ) < 1.0e-3f, "note-off releases to silence" );

        // Lower cutoff = darker = less high-frequency energy. Compare a high and low cutoff.
        auto energyAt = [&]( float Cutoff ) {
            synth.reset();
            synth.setParam( mt::BasicSynthEngine::kCutoff, Cutoff );
            synth.setParam( mt::BasicSynthEngine::kEnvAmount, 0.f );
            synth.noteOn( 48, 1.f );
            render( 40 );
            double diff = 0; // first-difference energy ~ high-frequency content
            for( int b = 0; b < 40; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( size_t i = 1; i < l.size(); ++i )
                    diff += ( l[i] - l[i - 1] ) * ( l[i] - l[i - 1] );
            }
            synth.allNotesOff();
            return diff;
        };
        const auto bright = energyAt( 12000.f ), dark = energyAt( 200.f );
        R.check( dark < bright * 0.2, "cutoff darkens the sound", "hf energy " + juce::String( bright, 2 ) + " -> " + juce::String( dark, 2 ) );

        // Eight voices: a ninth note steals instead of crashing or going silent.
        synth.reset();
        synth.setParam( mt::BasicSynthEngine::kCutoff, 4000.f );
        for( int n = 0; n < 9; ++n )
            synth.noteOn( 48 + n, 0.8f );
        const auto chord = render( 20 );
        R.check( chord > 0.02f && chord < 2.f, "nine notes on eight voices stay sane", "peak " + f3( chord ) );
    }

    //==============================================================================
    void testModularEngine( Results& R )
    {
        R.section( "Instrument engine: Modular" );
        using B = mt::modular::PatchBuilder;

        mt::ModularEngine synth;
        synth.prepare( testRate, (int)testBlock );

        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        bool               finite  = true;
        auto               render  = [&]( int Blocks ) {
            float peak = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( size_t i = 0; i < l.size(); ++i )
                {
                    finite = finite && std::isfinite( l[i] ) && std::isfinite( r[i] );
                    peak   = juce::jmax( peak, std::abs( l[i] ), std::abs( r[i] ) );
                }
            }
            return peak;
        };
        auto blocksFor = []( double Seconds ) { return (int)( Seconds * testRate / testBlock ); };

        R.check( synth.getParams().size() == mt::ModularEngine::kNumParams, "exposes eight macros and a level", juce::String( (int)synth.getParams().size() ) + " params" );
        R.check( render( 10 ) == 0.f, "silent with no notes" );

        // Every factory preset plays, stays in range and frees its voices after release.
        const auto names = synth.getPresetNames();
        R.check( names.size() >= 8, "factory presets", names.joinIntoString( ", " ) );
        for( int p = 0; p < names.size(); ++p )
        {
            synth.reset();
            synth.setPatch( synth.createPreset( p ) );
            synth.noteOn( 60, 0.9f );
            synth.noteOn( 67, 0.7f );
            const auto held = render( blocksFor( 1.0 ) );
            synth.noteOff( 60 );
            synth.noteOff( 67 );
            render( blocksFor( 5.0 ) );
            auto tail = render( blocksFor( 0.1 ) );
            for( int extra = 0; extra < 7 && tail >= 1.0e-4f; ++extra ) // long reverb and delay tails (Drone)
            {
                render( blocksFor( 0.9 ) );
                tail = render( blocksFor( 0.1 ) );
            }
            R.check( finite && held > 0.01f && held < 1.5f && tail < 1.0e-4f && synth.getNumActiveVoices() == 0, "preset \"" + names[p] + "\" plays and releases",
                     "peak " + f3( held ) + ", tail " + juce::String( tail, 6 ) + ", voices left " + juce::String( synth.getNumActiveVoices() ) );
        }

        // Pitch: a bare sine at A4 is 440 Hz, and with no envelope the note ends at note-off.
        {
            B b( "Sine" );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            synth.setPatch( b.tree );
            synth.noteOn( 69, 1.f );
            render( 4 );
            int   crossings = 0;
            float prev      = 0;
            for( int blk = 0; blk < blocksFor( 1.0 ); ++blk )
            {
                synth.process( outs, 2, (int)testBlock );
                for( auto v : l )
                {
                    crossings += prev < 0.f && v >= 0.f ? 1 : 0;
                    prev = v;
                }
            }
            R.check( std::abs( crossings - 440 ) <= 1, "A4 sine is 440 Hz", juce::String( crossings ) + " Hz" );
            synth.noteOff( 69 );
            render( 4 );
            R.check( synth.getNumActiveVoices() == 0 && render( 2 ) == 0.f, "a patch without envelopes ends at note-off" );
        }

        // Knob edits change values in place; structural edits rebuild and keep held notes.
        {
            B          b( "Edit" );
            const auto osc = b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            synth.setPatch( b.tree );
            const auto builds = synth.getBuildCount();
            synth.noteOn( 60, 1.f );
            const auto loud = render( 20 );
            b.tree.getChildWithProperty( mt::modular::PatchIDs::id, osc ).setProperty( "level", 0.25f, nullptr );
            synth.setPatch( b.tree );
            render( 2 );
            const auto quiet = render( 20 );
            R.check( synth.getBuildCount() == builds && std::abs( quiet / loud - 0.25f ) < 0.02f, "a knob edit applies without a rebuild",
                     "level ratio " + f3( quiet / loud ) + ", rebuilds " + juce::String( synth.getBuildCount() - builds ) );

            b.add( "drive", 0, 1, { { "drive", 0.5f } } );
            synth.setPatch( b.tree );
            const auto after = render( 20 );
            R.check( synth.getBuildCount() == builds + 1 && after > 0.02f && synth.getNumActiveVoices() == 1, "adding a module rebuilds and keeps the held note",
                     "peak " + f3( after ) + ", voices " + juce::String( synth.getNumActiveVoices() ) );
            synth.allNotesOff();
            render( 10 );
        }

        // Modulation: a macro opens a VCA.
        {
            B b( "Macro" );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            const auto vca = b.add( "vca", 0, 1, { { "gain", 0 } } );
            b.mod( vca, "gain", "macro1", 1.f );
            synth.setPatch( b.tree );
            synth.noteOn( 60, 1.f );
            const auto closed = render( 10 );
            synth.setParam( mt::ModularEngine::kMacro1, 1.f );
            render( 2 );
            const auto open = render( 10 );
            R.check( closed < 1.0e-4f && open > 0.05f, "a macro modulates a knob", f3( closed ) + " -> " + f3( open ) );
            synth.setParam( mt::ModularEngine::kMacro1, 0.f );
            synth.allNotesOff();
            render( 10 );
        }

        // Bad input is skipped, not fatal.
        {
            synth.setPatch( juce::ValueTree( mt::modular::PatchIDs::MODULAR_PATCH ) );
            synth.noteOn( 60, 1.f );
            const auto empty = render( 10 );
            synth.allNotesOff();
            render( 4 );

            B          b( "Bad" );
            const auto osc = b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            b.mod( osc, "level", "module:99", 1.f ).mod( osc, "nope", "velocity", 1.f ).mod( osc, "level", "nope", 1.f );
            juce::ValueTree unknown( mt::modular::PatchIDs::MODULE );
            unknown.setProperty( mt::modular::PatchIDs::type, "nope", nullptr );
            b.tree.appendChild( unknown, nullptr );
            synth.setPatch( b.tree );
            synth.noteOn( 60, 1.f );
            const auto bad = render( 10 );
            synth.allNotesOff();
            render( 4 );
            R.check( finite && empty == 0.f && bad > 0.05f, "empty patch is silent; unknown modules and modulations are skipped", f3( empty ) + " / " + f3( bad ) );
        }

        // Twenty notes on eight voices: stealing stays sane.
        synth.setPatch( synth.createPreset( 1 ) );
        for( int n = 0; n < 20; ++n )
            synth.noteOn( 40 + n, 0.8f );
        const auto chord = render( 40 );
        R.check( finite && chord > 0.02f && chord < 2.f && synth.getNumActiveVoices() == 8, "twenty notes on eight voices steal sanely",
                 "peak " + f3( chord ) + ", voices " + juce::String( synth.getNumActiveVoices() ) );
        synth.allNotesOff();
        render( blocksFor( 3.0 ) );

        // CPU: eight held voices of Basic Poly (five audio modules and two envelopes each).
        {
            for( int n = 0; n < 8; ++n )
                synth.noteOn( 48 + n * 3, 0.8f );
            const auto seconds = 4.0;
            const auto t0      = juce::Time::getMillisecondCounterHiRes();
            render( blocksFor( seconds ) );
            const auto load = ( juce::Time::getMillisecondCounterHiRes() - t0 ) / ( seconds * 1000.0 );
            R.check( load < 1.0, "eight voices of Basic Poly render in real time", juce::String( load * 100.0, 1 ) + "% of one core in this build" );
            synth.allNotesOff();
        }
    }

    //==============================================================================
    void testModularPatchEditing( Results& R )
    {
        R.section( "Modular patch editing (what the editor screen does)" );
        namespace mm = mt::modular;

        juce::UndoManager um;
        auto              patch = mm::createPreset( 0 ); // Init: osc 0,0  filter 0,1  vca 0,2  envelope 1,0
        mm::PatchEditor   ed( patch, &um );
        mt::ModularEngine synth;
        synth.prepare( testRate, (int)testBlock );
        synth.setPatch( patch );

        R.check( ed.addModule( "lfo", 0, 0 ) == 0 && ed.addModule( "nope", 3, 3 ) == 0, "no module into a taken cell or of an unknown type" );

        um.beginNewTransaction();
        const auto lfo = ed.addModule( "lfo", 2, 0 );
        R.check( lfo > 0 && ed.moduleAt( 2, 0 ).isValid() && patch.getProperty( mm::PatchIDs::name ).toString() == "Init *", "adding a module marks the preset edited",
                 "id " + juce::String( lfo ) + ", name " + patch.getProperty( mm::PatchIDs::name ).toString() );

        const auto filter = (int)ed.moduleAt( 0, 1 ).getProperty( mm::PatchIDs::id );
        um.beginNewTransaction();
        auto route = ed.addMod( filter, "cutoff", mm::PatchBuilder::module( lfo ), 0.3f );
        R.check( route.isValid() && !ed.addMod( filter, "nope", "velocity", 1.f ).isValid(), "modulation added; unknown knobs refused" );

        // Sources: 13 built-ins, then the modulators in grid order, named by type.
        const auto sources = ed.getSources();
        juce::StringArray names;
        for( const auto& src : sources )
            names.add( src.second );
        R.check( sources.size() == 16 && names[14] == "Envelope 1" && names[15] == "LFO 1" && ed.sourceLabel( mm::PatchBuilder::module( lfo ) ) == "LFO 1",
                 "modulation sources listed and named", names.joinIntoString( ", " ) );

        um.beginNewTransaction();
        const auto osc2 = ed.addModule( "osc", 0, 3 );
        R.check( ed.moduleLabel( osc2 ) == "Oscillator 2", "modules of one type are numbered in grid order", ed.moduleLabel( osc2 ) );

        // Values clamp, and stepped knobs round.
        ed.setValue( filter, "cutoff", 99999.f );
        ed.setValue( filter, "mode", 1.6f );
        R.check( ed.getValue( filter, "cutoff" ) == 20000.f && ed.getValue( filter, "mode" ) == 2.f, "values clamp to range and steps round",
                 f3( ed.getValue( filter, "cutoff" ) ) + " / " + f3( ed.getValue( filter, "mode" ) ) );

        // A knob edit reaches the engine without a rebuild; a move rebuilds.
        auto builds = synth.getBuildCount();
        synth.setPatch( patch ); // structure changed by the edits above
        builds = synth.getBuildCount();
        ed.setValue( filter, "cutoff", 500.f );
        synth.setPatch( patch );
        const auto valueOnly = synth.getBuildCount() == builds;

        // Moving onto a taken cell swaps the two.
        um.beginNewTransaction();
        ed.moveModule( lfo, 0, 0 );
        const auto swapped = ed.moduleAt( 0, 0 ).getProperty( mm::PatchIDs::id ) == juce::var( lfo ) && ed.moduleAt( 2, 0 ).getProperty( mm::PatchIDs::type ).toString() == "osc";
        synth.setPatch( patch );
        R.check( valueOnly && swapped && synth.getBuildCount() == builds + 1, "knob edits skip the rebuild; moving onto a module swaps them",
                 "rebuilds " + juce::String( synth.getBuildCount() - builds ) );

        // Removing a modulator takes the modulations it fed with it, and undo brings both back.
        const auto env = (int)ed.moduleAt( 1, 0 ).getProperty( mm::PatchIDs::id );
        auto       countMods = [&patch]() {
            int n = 0;
            for( const auto& m : patch )
                n += m.getNumChildren();
            return n;
        };
        const auto before = countMods();
        um.beginNewTransaction();
        ed.removeModule( env );
        const auto after = countMods();
        um.beginNewTransaction();
        um.undo();
        R.check( after == before - 1 && ed.moduleAt( 1, 0 ).isValid() && countMods() == before, "deleting a modulator drops its modulations; undo restores both",
                 juce::String( before ) + " -> " + juce::String( after ) + " -> " + juce::String( countMods() ) );

        um.beginNewTransaction();
        ed.setVoices( 99 );
        R.check( ed.getVoices() == mm::maxVoices, "voices clamp to the maximum", juce::String( ed.getVoices() ) );

        // The edited patch still plays.
        synth.setPatch( patch );
        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        synth.noteOn( 60, 1.f );
        float peak = 0;
        for( int b = 0; b < 40; ++b )
        {
            synth.process( outs, 2, (int)testBlock );
            for( auto v : l )
                peak = juce::jmax( peak, std::abs( v ) );
        }
        R.check( peak > 0.01f && std::isfinite( peak ), "the edited patch plays", "peak " + f3( peak ) );
    }

    //==============================================================================
    void testModularCablesAndEffects( Results& R )
    {
        R.section( "Modular: cables, effects, user presets, macro names" );
        namespace mm = mt::modular;
        using B      = mm::PatchBuilder;

        mt::ModularEngine synth;
        synth.prepare( testRate, (int)testBlock );
        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        bool               finite  = true;
        // Peak, and first-difference energy (~ high-frequency content), over some blocks.
        auto render = [&]( int Blocks, double* Hf = nullptr ) {
            float  peak = 0;
            double hf   = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( size_t i = 0; i < l.size(); ++i )
                {
                    finite = finite && std::isfinite( l[i] ) && std::isfinite( r[i] );
                    peak   = juce::jmax( peak, std::abs( l[i] ), std::abs( r[i] ) );
                    if( i > 0 )
                        hf += ( l[i] - l[i - 1] ) * ( l[i] - l[i - 1] );
                }
            }
            if( Hf )
                *Hf = hf;
            return peak;
        };
        auto blocksFor = []( double Seconds ) { return (int)( Seconds * testRate / testBlock ); };

        // Cross FM: column 1's sine (muted in the mix by a VCA at 0) phase-modulates column 0's.
        B          b( "Cable" );
        const auto carrier = b.add( "osc", 0, 0, { { "wave", 0 }, { "input", 1 }, { "pm", 1 }, { "level", 1 } } );
        const auto modOsc  = b.add( "osc", 1, 0, { { "wave", 0 }, { "tune", 19 }, { "level", 1 } } );
        b.add( "vca", 1, 1, { { "gain", 0 } } );
        auto plain = b.tree.createCopy();
        b.cable( carrier, modOsc, 1.f );

        auto hfOf = [&]( const juce::ValueTree& Patch ) {
            synth.reset();
            synth.setPatch( Patch );
            synth.reset(); // Measure steady patch timbre, excluding a graph-edit crossfade.
            synth.noteOn( 60, 1.f );
            render( 4 );
            double hf = 0;
            render( 20, &hf );
            synth.allNotesOff();
            render( 4 );
            return hf;
        };
        const auto withoutCable = hfOf( plain ), withCable = hfOf( b.tree );
        R.check( finite && withCable > withoutCable * 3.0, "a cable phase-modulates another column (more harmonics)",
                 "hf " + juce::String( withoutCable, 3 ) + " -> " + juce::String( withCable, 3 ) );

        // Its amount is a value: no rebuild, and 0 sounds like no cable.
        const auto builds = synth.getBuildCount();
        b.tree.getChildWithProperty( mm::PatchIDs::id, carrier ).getChildWithName( mm::PatchIDs::CABLE ).setProperty( mm::PatchIDs::amount, 0.f, nullptr );
        synth.setPatch( b.tree );
        R.check( synth.getBuildCount() == builds, "cable amount changes without a rebuild" );
        synth.noteOn( 60, 1.f );
        render( 4 );
        double zeroed = 0;
        render( 20, &zeroed );
        synth.allNotesOff();
        render( 4 );
        R.check( std::abs( zeroed - withoutCable ) < withoutCable * 0.05, "amount 0 sounds like no cable", juce::String( zeroed, 3 ) + " vs " + juce::String( withoutCable, 3 ) );

        // A feedback loop (each module cabled from the other) stays finite.
        {
            B          fb( "Feedback" );
            const auto a = fb.add( "osc", 0, 0, { { "wave", 0 }, { "input", 1 }, { "pm", 2 }, { "level", 1 } } );
            const auto d = fb.add( "drive", 1, 0, { { "drive", 0.8f } } );
            fb.cable( a, d, 1.f ).cable( d, a, 1.f );
            synth.setPatch( fb.tree );
            synth.noteOn( 48, 1.f );
            const auto peak = render( blocksFor( 1.0 ) );
            synth.allNotesOff();
            render( 4 );
            R.check( finite && peak > 0.01f && peak < 2.f, "a feedback cable loop stays finite", "peak " + f3( peak ) );
        }

        // Effects: a delay repeats a short blip after the voice has ended.
        {
            B          fx( "Echo" );
            const auto vca = fx.add( "vca", 0, 1, { { "gain", 0 } } );
            const auto env = fx.add( "adsr", 1, 0, { { "attack", 0.001f }, { "decay", 0.03f }, { "sustain", 0 }, { "release", 0.03f } } );
            fx.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            fx.mod( vca, "gain", B::module( env ), 1.f );
            auto dry = fx.tree.createCopy();
            fx.addEffect( "fxdelay", 0, { { "time", 300 }, { "feedback", 0.4f }, { "pingpong", 0 }, { "mix", 0.5f } } );

            auto echoAt = [&]( const juce::ValueTree& Patch ) {
                synth.reset();
                synth.setPatch( Patch );
                synth.noteOn( 69, 1.f );
                render( blocksFor( 0.05 ) );
                synth.noteOff( 69 );
                render( blocksFor( 0.22 ) ); // to ~270 ms: the voice is long gone
                const auto voices = synth.getNumActiveVoices();
                const auto echo   = render( blocksFor( 0.1 ) ); // 270..370 ms: the first repeat
                return std::make_pair( echo, voices );
            };
            const auto [dryEcho, dryVoices] = echoAt( dry );
            const auto [wetEcho, wetVoices] = echoAt( fx.tree );
            R.check( dryEcho < 1.0e-4f && wetEcho > 0.01f && wetVoices == 0, "the delay effect repeats after the voice ended",
                     "dry " + juce::String( dryEcho, 5 ) + ", wet " + f3( wetEcho ) + ", voices " + juce::String( wetVoices ) );
            render( blocksFor( 6.0 ) );
            R.check( render( 10 ) < 1.0e-4f, "and dies away" );
        }

        // A macro can modulate an effect: reverb mix from 0 to full with Macro 3.
        {
            B          fx( "Verb" );
            const auto vca = fx.add( "vca", 0, 1, { { "gain", 0 } } );
            const auto env = fx.add( "adsr", 1, 0, { { "attack", 0.001f }, { "decay", 0.05f }, { "sustain", 0 }, { "release", 0.05f } } );
            fx.add( "noise", 0, 0, { { "level", 1 } } );
            fx.mod( vca, "gain", B::module( env ), 1.f );
            const auto verb = fx.addEffect( "reverb", 0, { { "size", 0.8f }, { "mix", 0 } } );
            fx.mod( verb, "mix", "macro3", 1.f ).mod( verb, "size", "velocity", 1.f ); // velocity is per voice: ignored on an effect
            synth.setPatch( fx.tree );

            auto tail = [&]( float Macro ) {
                synth.reset();
                synth.setParam( mt::ModularEngine::kMacro1 + 2, Macro );
                synth.noteOn( 60, 1.f );
                render( blocksFor( 0.05 ) );
                synth.noteOff( 60 );
                render( blocksFor( 0.2 ) );
                return render( blocksFor( 0.2 ) );
            };
            const auto off = tail( 0.f ), on = tail( 1.f );
            synth.setParam( mt::ModularEngine::kMacro1 + 2, 0.f );
            R.check( finite && off < 1.0e-4f && on > 0.002f, "a macro modulates an effect (reverb mix)", f3( off ) + " -> " + f3( on ) );
        }

        // Macro names: stored, else what they modulate, else "Macro n".
        {
            const auto init = mm::createPreset( 0 ), bell = mm::createPreset( 3 );
            mm::PatchEditor initRead( init, nullptr ), bellRead( bell, nullptr );
            R.check( initRead.macroLabel( 0 ) == "Cutoff" && initRead.macroLabel( 3 ) == "Macro 4" && bellRead.macroLabel( 0 ) == "Brightness",
                     "macros named by what they modulate, or by the patch", initRead.macroLabel( 0 ) + ", " + initRead.macroLabel( 3 ) + ", " + bellRead.macroLabel( 0 ) );

            auto            edited = init.createCopy();
            mm::PatchEditor ed( edited, nullptr );
            const auto      vca = (int)ed.moduleAt( 0, 2 ).getProperty( mm::PatchIDs::id );
            ed.addMod( vca, "gain", "macro1", 0.2f );
            const auto two = ed.macroLabel( 0 );
            ed.setMacroName( 0, "Tone" );
            const auto named = ed.macroLabel( 0 );
            ed.setMacroName( 0, "" );
            R.check( two == "Cutoff +1" && named == "Tone" && ed.macroLabel( 0 ) == "Cutoff +1", "several targets, a set name, and back to automatic", two + " / " + named );
        }

        // User presets, in a temporary folder.
        {
            auto dir = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_test_modular_presets" );
            dir.deleteRecursively();
            mm::UserPresets::setDirectory( dir );

            const auto savedOk    = mm::UserPresets::save( b.tree, "My Cable FM" );
            const auto factoryNo  = !mm::UserPresets::save( b.tree, "Init" ) && !mm::UserPresets::save( b.tree, "  " );
            const auto names      = mm::UserPresets::getNames();
            const auto loaded     = mm::UserPresets::load( "My Cable FM" );
            const auto sameModule = loaded.getChildWithProperty( mm::PatchIDs::id, carrier ).getChildWithName( mm::PatchIDs::CABLE ).isValid();
            const auto removed    = mm::UserPresets::remove( "My Cable FM" ) && mm::UserPresets::getNames().isEmpty();
            R.check( savedOk && factoryNo && names.contains( "My Cable FM" ) && loaded.getProperty( mm::PatchIDs::name ).toString() == "My Cable FM" && sameModule && removed,
                     "user presets save, list, load (with cables) and delete; factory names refused", names.joinIntoString( ", " ) );

            mm::UserPresets::setDirectory( {} );
            dir.deleteRecursively();
        }
    }

    //==============================================================================
    void testModularPlayModes( Results& R )
    {
        R.section( "Modular: mono/legato, glide, pressure, unison, ladder, activity" );
        namespace mm = mt::modular;
        using B      = mm::PatchBuilder;

        mt::ModularEngine synth;
        synth.prepare( testRate, (int)testBlock );
        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        bool               finite  = true;
        auto render = [&]( int Blocks ) {
            float peak = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( size_t i = 0; i < l.size(); ++i )
                {
                    finite = finite && std::isfinite( l[i] ) && std::isfinite( r[i] );
                    peak   = juce::jmax( peak, std::abs( l[i] ), std::abs( r[i] ) );
                }
            }
            return peak;
        };
        // Frequency from rising zero crossings over Blocks.
        auto hz = [&]( int Blocks ) {
            int   crossings = 0;
            float prev      = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( auto v : l )
                {
                    crossings += prev < 0.f && v >= 0.f ? 1 : 0;
                    prev = v;
                }
            }
            return crossings * (double)testRate / ( Blocks * (double)testBlock );
        };
        auto blocksFor = []( double Seconds ) { return (int)( Seconds * testRate / testBlock ); };

        // A bare sine, so the pitch can be read back.
        auto sine = []( int Mode, float Glide ) {
            B b( "Sine", 8 );
            b.tree.setProperty( mm::PatchIDs::playMode, Mode, nullptr ).setProperty( mm::PatchIDs::glide, Glide, nullptr );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            return b.tree;
        };

        // Mono: the last note wins, letting it go returns to the one still held.
        synth.setPatch( sine( mm::Mono, 0.f ) );
        synth.noteOn( 57, 1.f ); // A3 220 Hz
        render( 4 );
        synth.noteOn( 69, 1.f ); // A4 440 Hz
        render( 4 );
        const auto top    = hz( blocksFor( 0.5 ) );
        const auto voices = synth.getNumActiveVoices();
        synth.noteOff( 69 );
        render( 4 );
        const auto back = hz( blocksFor( 0.5 ) );
        synth.noteOff( 57 );
        render( 10 );
        R.check( std::abs( top - 440 ) < 3 && voices == 1 && std::abs( back - 220 ) < 3 && synth.getNumActiveVoices() == 0, "mono: last note wins, release returns to the held note",
                 juce::String( top, 1 ) + " Hz, voices " + juce::String( voices ) + ", back to " + juce::String( back, 1 ) + " Hz" );

        // Glide: 200 ms from A3 to A4.
        synth.setPatch( sine( mm::Legato, 0.2f ) );
        synth.noteOn( 57, 1.f );
        render( 10 );
        synth.noteOn( 69, 1.f );
        const auto early = hz( blocksFor( 0.04 ) );
        render( blocksFor( 0.4 ) );
        const auto settled = hz( blocksFor( 0.5 ) );
        synth.allNotesOff();
        render( 10 );
        R.check( early > 225 && early < 400 && std::abs( settled - 440 ) < 3, "legato glide slides to the new note", juce::String( early, 1 ) + " Hz early, " + juce::String( settled, 1 ) + " Hz settled" );

        // Mono restarts the envelope on each note (a pluck jumps back to full level);
        // legato keeps it where it is (at the sustain level).
        auto secondNote = [&]( int Mode ) {
            B b( "Env", 8 );
            b.tree.setProperty( mm::PatchIDs::playMode, Mode, nullptr );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            const auto vca = b.add( "vca", 0, 1, { { "gain", 0 } } );
            const auto env = b.add( "adsr", 1, 0, { { "attack", 0.001f }, { "decay", 0.1f }, { "sustain", 0.2f }, { "velocity", 0 } } );
            b.mod( vca, "gain", B::module( env ), 1 );
            synth.setPatch( b.tree );
            synth.noteOn( 60, 1.f );
            render( blocksFor( 0.5 ) ); // decayed to the sustain level
            synth.noteOn( 64, 1.f );
            const auto after = render( blocksFor( 0.05 ) );
            synth.allNotesOff();
            render( blocksFor( 1.0 ) );
            return after;
        };
        const auto legatoLevel = secondNote( mm::Legato ), monoLevel = secondNote( mm::Mono );
        R.check( monoLevel > 0.1f && legatoLevel < monoLevel * 0.4f, "mono retriggers the envelope, legato keeps it going", "mono " + f3( monoLevel ) + ", legato " + f3( legatoLevel ) );

        // Pressure: a VCA opened only by aftertouch.
        {
            B b( "Pressure" );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            const auto vca = b.add( "vca", 0, 1, { { "gain", 0 } } );
            b.mod( vca, "gain", "pressure", 1 );
            synth.setPatch( b.tree );
            synth.noteOn( 60, 1.f );
            const auto none = render( 10 );
            synth.aftertouch( 60, 1.f );
            render( 2 );
            const auto poly = render( 10 );
            synth.aftertouch( 60, 0.f );
            render( 4 );
            synth.channelPressure( 0.5f );
            render( 2 );
            const auto channel = render( 10 );
            synth.allNotesOff();
            render( 10 );
            R.check( none < 1.0e-4f && poly > 0.1f && channel > 0.03f && channel < poly, "poly aftertouch and channel pressure modulate", f3( none ) + " / " + f3( poly ) + " / " + f3( channel ) );
        }

        // Unison: seven detuned saws beat against each other (the level moves), one does not.
        auto levelSpread = [&]( int Unison ) {
            B b( "Unison" );
            b.add( "osc", 0, 0, { { "wave", 2 }, { "unison", (float)Unison }, { "spread", 30 }, { "level", 1 } } );
            synth.setPatch( b.tree );
            synth.noteOn( 48, 1.f );
            render( 10 );
            float lo = 1e9f, hi = 0;
            for( int blk = 0; blk < 60; ++blk )
            {
                const auto p = render( 4 );
                lo           = juce::jmin( lo, p );
                hi           = juce::jmax( hi, p );
            }
            synth.allNotesOff();
            render( 10 );
            return std::make_pair( lo, hi );
        };
        const auto one = levelSpread( 1 ), seven = levelSpread( 7 );
        R.check( finite && ( one.second - one.first ) < 0.02f && ( seven.second - seven.first ) > 0.03f && seven.second < 1.5f, "unison detunes copies (they beat), at a similar level",
                 "1: " + f3( one.first ) + ".." + f3( one.second ) + ", 7: " + f3( seven.first ) + ".." + f3( seven.second ) );

        // Ladder: darker at a low cutoff, and sane at full resonance.
        {
            B          b( "Ladder" );
            b.add( "osc", 0, 0, { { "wave", 2 }, { "level", 1 } } );
            const auto flt = b.add( "filter", 0, 1, { { "mode", 4 }, { "cutoff", 300 }, { "reso", 1 } } );
            synth.setPatch( b.tree );
            synth.noteOn( 48, 1.f );
            const auto loud = render( blocksFor( 1.0 ) );
            synth.allNotesOff();
            render( 10 );
            R.check( finite && loud > 0.01f && loud < 2.f, "ladder filter at full resonance stays finite", "peak " + f3( loud ) );
            juce::ignoreUnused( flt );
        }

        // Activity: an LFO's value moves and an oscillator's level shows while a note plays.
        {
            B          b( "Activity" );
            const auto osc = b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            const auto lfo = b.add( "lfo", 1, 0, { { "rate", 4 } } );
            const auto verb = b.addEffect( "reverb", 0 );
            synth.setPatch( b.tree );
            const auto idle = synth.getActivity( osc );
            synth.noteOn( 60, 1.f );
            float lo = 1, hi = -1;
            for( int i = 0; i < 40; ++i )
            {
                render( 4 );
                lo = juce::jmin( lo, synth.getActivity( lfo ) );
                hi = juce::jmax( hi, synth.getActivity( lfo ) );
            }
            const auto level = synth.getActivity( osc ), fx = synth.getActivity( verb );
            synth.allNotesOff();
            render( 10 );
            R.check( idle == 0.f && level > 0.5f && fx > 0.01f && hi - lo > 1.f && synth.getActivity( 999 ) == 0.f, "activity: audio peaks and modulator values for the editor",
                     "osc " + f3( level ) + ", fx " + f3( fx ) + ", lfo " + f3( lo ) + ".." + f3( hi ) );
        }

        // Effects swap slots.
        {
            auto            patch = mm::createPreset( 7 ); // Random Pad: chorus 0, reverb 1
            mm::PatchEditor ed( patch, nullptr );
            const auto      chorus = (int)ed.effectAt( 0 ).getProperty( mm::PatchIDs::id );
            ed.moveEffect( chorus, 1 );
            R.check( ed.effectAt( 1 ).getProperty( mm::PatchIDs::type ).toString() == "chorus" && ed.effectAt( 0 ).getProperty( mm::PatchIDs::type ).toString() == "reverb",
                     "moving an effect onto another swaps them" );
        }
    }

    //==============================================================================
    void testModularTempoArpAndModules( Results& R )
    {
        R.section( "Modular: tempo sync, arpeggiator, Formant, Ring Mod" );
        namespace mm = mt::modular;
        using B      = mm::PatchBuilder;

        mt::ModularEngine synth;
        synth.prepare( testRate, (int)testBlock );
        std::vector<float> l( testBlock ), r( testBlock );
        float*             outs[2] = { l.data(), r.data() };
        bool               finite  = true;
        auto render = [&]( int Blocks, double* Hf = nullptr ) {
            float  peak = 0;
            double hf   = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( size_t i = 0; i < l.size(); ++i )
                {
                    finite = finite && std::isfinite( l[i] ) && std::isfinite( r[i] );
                    peak   = juce::jmax( peak, std::abs( l[i] ), std::abs( r[i] ) );
                    if( i > 0 )
                        hf += ( l[i] - l[i - 1] ) * ( l[i] - l[i - 1] );
                }
            }
            if( Hf )
                *Hf = hf;
            return peak;
        };
        auto hz = [&]( int Blocks ) {
            int   crossings = 0;
            float prev      = 0;
            for( int b = 0; b < Blocks; ++b )
            {
                synth.process( outs, 2, (int)testBlock );
                for( auto v : l )
                {
                    crossings += prev < 0.f && v >= 0.f ? 1 : 0;
                    prev = v;
                }
            }
            return crossings * (double)testRate / ( Blocks * (double)testBlock );
        };
        auto blocksFor = []( double Seconds ) { return (int)std::round( Seconds * testRate / testBlock ); };

        // A synced LFO: 1/4 notes = 2 Hz at 120 BPM, 1 Hz at 60.
        {
            B          b( "Sync" );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            const auto lfo = b.add( "lfo", 1, 0, { { "sync", 5 }, { "rate", 7 } } ); // the free rate is ignored
            synth.setPatch( b.tree );
            auto cycles = [&]( double Bpm ) {
                synth.setTempo( Bpm );
                synth.noteOn( 60, 1.f );
                int   rises = 0;
                float prev  = synth.getActivity( lfo );
                for( int i = 0; i < blocksFor( 4.0 ); ++i )
                {
                    render( 1 );
                    const auto v = synth.getActivity( lfo );
                    rises += prev < 0.f && v >= 0.f ? 1 : 0;
                    prev = v;
                }
                synth.allNotesOff();
                render( 10 );
                return rises / 4.0;
            };
            const auto at120 = cycles( 120 ), at60 = cycles( 60 );
            R.check( std::abs( at120 - 2.0 ) < 0.3 && std::abs( at60 - 1.0 ) < 0.3, "a synced LFO follows the tempo", juce::String( at120, 2 ) + " Hz at 120, " + juce::String( at60, 2 ) + " Hz at 60" );
        }

        // A synced delay: a 1/4 echo comes 500 ms after the note at 120 BPM.
        {
            B          b( "Echo" );
            const auto vca = b.add( "vca", 0, 1, { { "gain", 0 } } );
            const auto env = b.add( "adsr", 1, 0, { { "attack", 0.001f }, { "decay", 0.03f }, { "sustain", 0 }, { "release", 0.03f } } );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            b.mod( vca, "gain", B::module( env ), 1.f );
            b.addEffect( "fxdelay", 0, { { "sync", 5 }, { "feedback", 0 }, { "pingpong", 0 }, { "mix", 0.5f } } );
            synth.setPatch( b.tree );
            synth.setTempo( 120 );
            synth.reset();
            synth.noteOn( 69, 1.f );
            render( blocksFor( 0.05 ) );
            synth.noteOff( 69 );
            render( blocksFor( 0.3 ) );                       // to 350 ms: nothing yet
            const auto before = render( blocksFor( 0.1 ) );   // 350..450 ms
            const auto echo   = render( blocksFor( 0.15 ) );  // 450..600 ms
            render( blocksFor( 1.0 ) );
            R.check( before < 1.0e-3f && echo > 0.02f, "a synced delay repeats on the beat (1/4 = 500 ms at 120 BPM)", f3( before ) + " then " + f3( echo ) );
        }

        // Arpeggiator: hold C-E-G; 1/16 at 120 BPM = a step every 125 ms.
        auto arpPitches = [&]( int Mode, int Octaves, std::initializer_list<int> Notes, int Steps ) {
            B b( "Arp", 4 );
            b.tree.setProperty( mm::PatchIDs::arp, Mode, nullptr ).setProperty( mm::PatchIDs::arpRate, 2, nullptr ).setProperty( mm::PatchIDs::arpOctaves, Octaves, nullptr ).setProperty( mm::PatchIDs::arpGate, 1.f, nullptr );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            synth.setPatch( b.tree );
            synth.setTempo( 120 );
            synth.reset();
            for( auto n : Notes )
                synth.noteOn( n, 1.f );
            juce::StringArray seq;
            const auto        step = blocksFor( 0.125 ); // 46.875 blocks: measure inside each step
            for( int i = 0; i < Steps; ++i )
            {
                render( 4 );
                const auto f = hz( (int)step - 8 );
                render( (int)std::round( 0.125 * testRate / testBlock * ( i + 1 ) ) - (int)std::round( 0.125 * testRate / testBlock * i ) - ( (int)step - 4 ) );
                seq.add( juce::String( juce::roundToInt( 12.0 * std::log2( f / 440.0 ) + 69.0 ) ) );
            }
            for( auto n : Notes )
                synth.noteOff( n );
            render( blocksFor( 0.3 ) );
            return seq.joinIntoString( " " );
        };
        const auto up     = arpPitches( mm::ArpUp, 2, { 67, 60, 64 }, 7 );
        const auto down   = arpPitches( mm::ArpDown, 1, { 60, 64, 67 }, 4 );
        const auto upDown = arpPitches( mm::ArpUpDown, 1, { 60, 64, 67 }, 6 );
        const auto played = arpPitches( mm::ArpAsPlayed, 1, { 67, 60, 64 }, 4 );
        R.check( up == "60 64 67 72 76 79 60", "arpeggio Up over two octaves, on the 1/16 grid", up );
        R.check( down == "67 64 60 67" && upDown == "60 64 67 64 60 64" && played == "67 60 64 67", "Down, Up-Down and As Played orders", down + " | " + upDown + " | " + played );
        R.check( synth.getNumActiveVoices() == 0, "letting go stops the arpeggio" );

        // Formant: the vowel changes the spectrum (I is brighter than U).
        auto formantHf = [&]( float Vowel ) {
            B b( "Vowel" );
            b.add( "osc", 0, 0, { { "wave", 2 }, { "level", 1 } } );
            b.add( "formant", 0, 1, { { "vowel", Vowel }, { "reso", 0.6f } } );
            synth.setPatch( b.tree );
            synth.reset();
            synth.noteOn( 48, 1.f );
            render( 10 );
            double hf   = 0;
            const auto peak = render( 40, &hf );
            synth.allNotesOff();
            render( 10 );
            return std::make_pair( hf, peak );
        };
        const auto vowelI = formantHf( 2 ), vowelU = formantHf( 4 );
        R.check( finite && vowelI.first > vowelU.first * 3.0 && vowelI.second > 0.01f && vowelU.second > 0.01f, "the formant filter's vowel shapes the sound (I brighter than U)",
                 "hf I " + juce::String( vowelI.first, 3 ) + ", U " + juce::String( vowelU.first, 3 ) );

        // Ring Mod: mixing it in changes the timbre, and stays in range.
        auto ringHf = [&]( float Mix ) {
            B b( "Ring" );
            b.add( "osc", 0, 0, { { "wave", 0 }, { "level", 1 } } );
            b.add( "ring", 0, 1, { { "tune", 7.4f }, { "mix", Mix } } );
            synth.setPatch( b.tree );
            synth.reset();
            synth.noteOn( 60, 1.f );
            render( 10 );
            double     hf   = 0;
            const auto peak = render( 40, &hf );
            synth.allNotesOff();
            render( 10 );
            return std::make_pair( hf, peak );
        };
        const auto dry = ringHf( 0.f ), ringed = ringHf( 1.f );
        R.check( finite && ringed.first > dry.first * 1.5 && ringed.second < 1.f, "ring modulation adds sidebands", "hf " + juce::String( dry.first, 3 ) + " -> " + juce::String( ringed.first, 3 ) );

        // Any named patch is marked once when edited (user presets too).
        {
            B b( "My Bass" );
            const auto osc = b.add( "osc", 0, 0 );
            mm::PatchEditor ed( b.tree, nullptr );
            ed.setValue( osc, "level", 0.2f );
            ed.setValue( osc, "level", 0.3f );
            R.check( b.tree.getProperty( mm::PatchIDs::name ).toString() == "My Bass *", "editing a user preset marks it once", b.tree.getProperty( mm::PatchIDs::name ).toString() );
        }
    }

    //==============================================================================
    void testPadLayout( Results& R )
    {
        R.section( "Pad layouts (Push/Launchpad style)" );
        using Mode = mt::PadLayout::Mode;

        auto row = []( const mt::PadLayout& L, int Row, int Bank ) {
            juce::String s;
            for( int c = 0; c < 4; ++c )
                s << L.noteFor( Row * 4 + c, Bank ) << " ";
            return s.trim();
        };

        mt::PadLayout l; // defaults: 4ths, in key, C major
        R.check( l.noteFor( 0, 3 ) == 60, "bank D, pad 1 is C3 (60)", juce::String( l.noteFor( 0, 3 ) ) );
        R.check( row( l, 0, 3 ) == "60 62 64 65", "in-key bottom row walks the scale", row( l, 0, 3 ) );
        R.check( row( l, 1, 3 ) == "65 67 69 71", "4ths in key: next row starts 3 degrees up (F)", row( l, 1, 3 ) );
        R.check( l.noteFor( 0, 4 ) == 72, "next bank is an octave up", juce::String( l.noteFor( 0, 4 ) ) );

        l.mode = Mode::Thirds;
        R.check( row( l, 1, 3 ) == "64 65 67 69", "3rds in key: next row starts 2 degrees up (E)", row( l, 1, 3 ) );
        l.mode = Mode::Sequential;
        R.check( row( l, 1, 3 ) == "67 69 71 72", "sequential in key continues the scale", row( l, 1, 3 ) );

        l.mode  = Mode::Fourths;
        l.inKey = false;
        R.check( row( l, 0, 3 ) == "60 61 62 63" && row( l, 1, 3 ) == "65 66 67 68", "chromatic 4ths: rows 5 semitones apart", row( l, 0, 3 ) + " / " + row( l, 1, 3 ) );

        l.inKey      = true;
        l.root       = 9; // A
        l.scaleIndex = 1; // Natural Minor
        R.check( row( l, 0, 3 ) == "69 71 72 74", "A minor from A3", row( l, 0, 3 ) );
        R.check( l.isRoot( 57 ) && l.isRoot( 69 ) && !l.isRoot( 60 ), "root detection by pitch class" );
        R.check( l.isInScale( 60 ) && !l.isInScale( 61 ), "scale membership" );

        l.scaleIndex = 4; // Pentatonic Major: 5 notes, so degree 5 wraps to the next octave
        R.check( l.noteFor( 5, 3 ) == 9 + 60 + 12 - 12 + 12 || l.noteFor( 5, 3 ) > l.noteFor( 4, 3 ), "pentatonic wraps octaves upward", row( l, 1, 3 ) );

        mt::PadLayout high;
        high.inKey = false;
        R.check( high.noteFor( 15, 7 ) == -1 || high.noteFor( 15, 7 ) <= 127, "notes above 127 are dropped", juce::String( high.noteFor( 15, 7 ) ) );

        juce::ValueTree t( "PAD_LAYOUT" );
        mt::PadLayout   saved;
        saved.mode       = Mode::Thirds;
        saved.inKey      = false;
        saved.root       = 7;
        saved.scaleIndex = 13;
        saved.toTree( t );
        const auto back = mt::PadLayout::fromTree( t );
        R.check( back.mode == Mode::Thirds && !back.inKey && back.root == 7 && back.scaleIndex == 13, "settings round-trip through the program tree" );
        R.check( mt::PadLayout::fromTree( juce::ValueTree{} ).mode == Mode::Fourths, "missing settings default to 4ths, in key, C major" );
    }

    //==============================================================================
    // Peak device output level over a window, in linear gain.
    float measureOutput( juce::AudioDeviceManager::LevelMeter::Ptr Meter, int Ms )
    {
        float peak = 0;
        auto  end  = juce::Time::getMillisecondCounter() + (juce::uint32)Ms;
        while( juce::Time::getMillisecondCounter() < end )
        {
            pump( 5 );
            peak = juce::jmax( peak, (float)Meter->getCurrentLevel() );
        }
        return peak;
    }

    void testEngine( Results& R, const juce::File& KitDir )
    {
        R.section( "Engine end to end (real audio device)" );

        auto& ai = mt::getAudioInterface();
        pump( 1500 ); // the device is opened asynchronously

        auto& dm  = ai.getAudioDeviceManager();
        auto* dev = dm.getCurrentAudioDevice();
        if( !R.check( dev != nullptr, "audio device opened" ) )
            return;

        R.note( "device: " + dm.getCurrentAudioDeviceType() + " / " + dev->getName() + " @ " + juce::String( dev->getCurrentSampleRate() ) + " Hz, " + juce::String( dev->getCurrentBufferSizeSamples() ) + " samples" );
        R.note( "inputs active: " + juce::String( dev->getActiveInputChannels().countNumberOfSetBits() ) + ", outputs active: " + juce::String( dev->getActiveOutputChannels().countNumberOfSetBits() ) );

        auto meter = dm.getOutputLevelGetter();
        R.check( measureOutput( meter, 300 ) < 0.001f, "silent at idle" );

        // Build a sequence -> track -> drum program with the kit on pads 0..N.
        const auto seq  = ai.createSequence();
        const auto prog = ai.createProgram( mt::MTProgramType::Drum, "TestKit" );
        const auto trk  = ai.createTrack( seq );
        R.check( seq >= 0 && prog >= 0 && trk >= 0, "created sequence, drum program, track", "seq " + juce::String( seq ) + " prog " + juce::String( prog ) + " track " + juce::String( trk ) );
        R.check( ai.setProgramToTrack( prog, seq, trk ) == 0, "program assigned to track" );
        ai.setSelectedSequenceAndTrack( seq, trk );
        R.note( "program index by name: " + juce::String( ai.getProgramIndexByName( "TestKit" ) ) + ", selected program " + juce::String( ai.getSelectedProgram() ) );

        auto files = KitDir.findChildFiles( juce::File::findFiles, false, "*.wav" );
        files.sort();
        if( !R.check( !files.isEmpty(), "found kit samples", KitDir.getFullPathName() ) )
            return;

        // Same order as the UI: import into the project's assets, then assign.
        for( int pad = 0; pad < files.size(); ++pad )
        {
            ai.addAsset( files[pad].getFullPathName(), mt::AssetTypes::Samples );
            ai.addSampleToProgram( prog, pad, files[pad].getFullPathName() );
        }
        pump( 1000 ); // samples load in the background
        R.check( ai.getNumProgramSamples( prog ) == files.size(), "samples assigned to pads", juce::String( ai.getNumProgramSamples( prog ) ) + "/" + juce::String( files.size() ) );

        // Hit each pad exactly as AppMainmode does for a hardware pad press.
        for( int pad = 0; pad < files.size(); ++pad )
        {
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, pad, (juce::uint8)127 ) );
            const auto peak = measureOutput( meter, 350 );
            float      trackPeak{ -100 };
            for( auto& m : ai.getTrackMeter( seq, trk ) )
                trackPeak = juce::jmax( trackPeak, m.peak );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, pad ) );
            R.check( peak > 0.05f, "pad " + juce::String( pad + 1 ) + " (" + files[pad].getFileNameWithoutExtension() + ") makes sound", "device peak " + f3( peak ) + ", track meter " + f3( trackPeak ) + " dB" );
            measureOutput( meter, 250 );
        }
        // Pads are one-shot: the last test sample is a 1.5 s tone that rings past note-off.
        measureOutput( meter, 1500 ); // let the tone finish
        const auto tail = measureOutput( meter, 1000 );
        R.check( tail < 0.01f, "silent again after the hits", "peak " + f3( tail ) );

        // Same hit, straight into the program (skips the track).
        ai.addLiveMidiNote( prog, 2, 127 );
        const auto direct = measureOutput( meter, 350 );
        ai.stopLiveMidiNote( prog, 2 );
        R.check( direct > 0.05f, "note sent straight to the program makes sound", "peak " + f3( direct ) );
        measureOutput( meter, 500 );

        // Velocity: a soft hit should be quieter than a hard one.
        ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 0, (juce::uint8)30 ) );
        const auto soft = measureOutput( meter, 350 );
        measureOutput( meter, 500 );
        ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 0, (juce::uint8)127 ) );
        const auto hard = measureOutput( meter, 350 );
        R.check( soft < hard * 0.8f, "velocity affects level", "v30 " + f3( soft ) + " v127 " + f3( hard ) );
        measureOutput( meter, 500 );

        // Sequencer: two notes in the track, then play the sequence.
        ai.addOrModifyNotesInTrack( seq, trk, 0, 0, 96 );
        ai.addOrModifyNotesInTrack( seq, trk, 1, 384, 96 );
        ai.setSequencePlay( seq, true, true );
        const auto seqPeak = measureOutput( meter, 2500 );
        ai.setSequenceStop();
        R.check( seqPeak > 0.05f, "sequencer plays programmed notes", "peak " + f3( seqPeak ) );
        measureOutput( meter, 500 );

        //--------------------------------------------------------------------------
        R.section( "Instrument (Plugin) program through the engine" );
        {
            const auto ip   = ai.createProgram( mt::MTProgramType::Plugin, "TestSynth" );
            auto*      inst = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( ip ) );
            if( R.check( inst != nullptr, "Plugin program creates an instrument", "index " + juce::String( ip ) ) )
            {
                R.check( inst->getEngineName() == "Basic Synth", "default engine is Basic Synth", inst->getEngineName() );

                const auto itrk = ai.createTrack( seq );
                ai.setProgramToTrack( ip, seq, itrk );
                ai.setSelectedSequenceAndTrack( seq, itrk );

                // Same path as a hardware pad press.
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 60, (juce::uint8)110 ) );
                const auto playing = measureOutput( meter, 400 );
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 60 ) );
                R.check( playing > 0.02f, "pad note plays the synth", "peak " + f3( playing ) );

                measureOutput( meter, 800 ); // release
                const auto released = measureOutput( meter, 300 );
                R.check( released < 0.005f, "note-off releases", "peak " + f3( released ) );

                // Parameters persist in the program's tree and survive an engine reload.
                inst->setParam( mt::BasicSynthEngine::kCutoff, 400.f );
                inst->setEngine( "Basic Synth" );
                R.check( std::abs( inst->getParam( mt::BasicSynthEngine::kCutoff ) - 400.f ) < 0.01f, "parameter is stored with the program", f3( inst->getParam( mt::BasicSynthEngine::kCutoff ) ) );
                R.check( inst->getState().getChildWithName( mt::IDs::INSTRUMENT_PARAMS ).hasProperty( "cutoff" ), "parameter saved in project tree" );

                // The pads read the layout through the interface's program data.
                mt::PadLayout dMinor;
                dMinor.root       = 2;
                dMinor.scaleIndex = 1;
                dMinor.toTree( inst->getState().getOrCreateChildWithName( mt::IDs::PAD_LAYOUT, nullptr ) );
                auto*      data   = ai.getProgramDataForTrack( seq, itrk );
                const auto seen   = mt::PadLayout::fromTree( data ? data->programState.getChildWithName( mt::IDs::PAD_LAYOUT ) : juce::ValueTree{} );
                R.check( data && data->type.get() == mt::MTProgramType::Plugin && seen.root == 2 && seen.scaleIndex == 1, "pad layout readable via track's program data", "root " + juce::String( seen.root ) + " scale " + juce::String( seen.scaleIndex ) );
                R.check( seen.noteFor( 0, 3 ) == 62, "D minor layout: bank D pad 1 = D3 (62)", juce::String( seen.noteFor( 0, 3 ) ) );

                // Chord through the program.
                for( int n : { 60, 64, 67 } )
                    ai.addLiveMidiNote( ip, n, 100 );
                const auto chord = measureOutput( meter, 400 );
                for( int n : { 60, 64, 67 } )
                    ai.stopLiveMidiNote( ip, n );
                R.check( chord > 0.02f, "chord plays", "peak " + f3( chord ) );
                measureOutput( meter, 1000 );
            }
        }

        //--------------------------------------------------------------------------
        R.section( "Modular engine through a program" );
        {
            const auto mp   = ai.createProgram( mt::MTProgramType::Plugin, "TestModular" );
            auto*      inst = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( mp ) );
            if( R.check( inst != nullptr, "Plugin program for the Modular engine", "index " + juce::String( mp ) ) )
            {
                auto play = [&]( int Note ) {
                    ai.addLiveMidiNote( mp, Note, 110 );
                    const auto peak = measureOutput( meter, 400 );
                    ai.stopLiveMidiNote( mp, Note );
                    measureOutput( meter, 1200 );
                    return peak;
                };

                inst->setEngine( "Modular" );
                R.check( inst->getEngineName() == "Modular" && inst->hasPatch() && !inst->getPatch().isValid() && inst->getPatchName() == "Init",
                         "a new Modular program plays preset 0 until a patch is stored", inst->getPatchName() );
                const auto init = play( 60 );
                R.check( init > 0.02f, "default patch plays", "peak " + f3( init ) );

                inst->loadPreset( 3 );
                const auto bell = play( 72 );
                measureOutput( meter, 2500 ); // the bell rings on
                R.check( inst->getPatchName() == "FM Bell" && inst->getPatch().isValid() && bell > 0.02f, "a preset is stored in the program and plays",
                         inst->getPatchName() + ", peak " + f3( bell ) );

                inst->loadPreset( 0 );
                auto osc = inst->getPatch().getChildWithProperty( mt::modular::PatchIDs::type, "osc" );
                osc.setProperty( "level", 0.f, nullptr );
                const auto muted = play( 60 );
                osc.setProperty( "level", 0.5f, nullptr );
                const auto restored = play( 60 );
                R.check( muted < 0.002f && restored > 0.02f, "a module edit in the stored patch reaches the engine", f3( muted ) + " -> " + f3( restored ) );

                {
                    namespace mm = mt::modular;
                    auto dir     = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_test_modular_presets_program" );
                    dir.deleteRecursively();
                    mm::UserPresets::setDirectory( dir );

                    inst->loadPreset( 4 ); // Wobble Bass: macro 1 = LFO rate
                    const auto macroName = inst->getParamName( mt::ModularEngine::kMacro1 );
                    const auto saved     = inst->savePreset( "My Wobble" ) && !inst->savePreset( "Init" );
                    const auto names     = inst->getPresetNames();
                    inst->loadPreset( 0 );
                    inst->loadPreset( names.indexOf( "My Wobble" ) );
                    const auto back = inst->getPatchName();
                    R.check( macroName == "Rate" && saved && names.size() == inst->getNumFactoryPresets() + 1 && names[names.size() - 1] == "My Wobble" && back == "My Wobble",
                             "a program saves a user preset, lists it after the factory ones and loads it", macroName + ", " + back );
                    const auto wobble = play( 48 );
                    R.check( wobble > 0.02f, "the user preset plays", "peak " + f3( wobble ) );

                    mm::UserPresets::setDirectory( {} );
                    dir.deleteRecursively();
                }

                {
                    namespace mm = mt::modular;
                    inst->loadPreset( 0 );
                    auto patch = inst->getOrCreatePatch();
                    mm::PatchEditor ed( patch, nullptr );
                    const auto      vca = (int)ed.moduleAt( 0, 2 ).getProperty( mm::PatchIDs::id );
                    ed.removeMod( ed.module( vca ).getChild( 0 ) ); // no envelope: only pressure opens it
                    ed.addMod( vca, "gain", "pressure", 1.f );

                    const auto pt = ai.createTrack( seq );
                    ai.setProgramToTrack( mp, seq, pt );
                    ai.setSelectedSequenceAndTrack( seq, pt );
                    ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 60, (juce::uint8)110 ) );
                    const auto before = measureOutput( meter, 300 );
                    ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::aftertouchChange( 1, 60, 127 ) );
                    const auto pressed = measureOutput( meter, 300 );
                    ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 60 ) );
                    measureOutput( meter, 300 );
                    R.check( before < 0.005f && pressed > 0.02f, "pad aftertouch reaches a Modular voice through the track", f3( before ) + " -> " + f3( pressed ) );
                }

                {
                    measureOutput( meter, 400 ); // the program's tempo timer ticks, the engine applies it
                    auto* modular = dynamic_cast<mt::ModularEngine*>( inst->getAudioObject()->getEngine() );
                    R.check( modular && std::abs( modular->getTempo() - ai.getMasterBPM() ) < 0.01, "the project tempo reaches the Modular engine",
                             juce::String( modular ? modular->getTempo() : 0.0 ) + " vs " + juce::String( ai.getMasterBPM() ) );
                }

                inst->setEngine( "Basic Synth" );
                R.check( !inst->hasPatch() && !inst->getState().getChildWithName( mt::modular::PatchIDs::MODULAR_PATCH ).isValid(), "switching to another engine drops the patch" );
            }
        }

        //--------------------------------------------------------------------------
        R.section( "Instrument hardening: notes never hang" );
        {
            // This section meters the device, so isolate the held synth from
            // the earlier drum pattern that transport playback also starts.
            ai.setTrackMute( seq, trk, true );
            const auto ip  = ai.createProgram( mt::MTProgramType::Plugin, "HardenSynth" );
            auto*      ins = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( ip ) );
            const auto ht  = ai.createTrack( seq );
            ai.setProgramToTrack( ip, seq, ht );
            ai.setSelectedSequenceAndTrack( seq, ht );

            auto held = [&]() {
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 57, (juce::uint8)110 ) );
                return measureOutput( meter, 300 );
            };
            auto settled = [&]() {
                measureOutput( meter, 900 ); // default release 0.3 s
                return measureOutput( meter, 300 );
            };

            // 1. Transport stop sends all-notes-off.
            const auto playing = held();
            ai.setSequencePlay( seq, true, true );
            measureOutput( meter, 300 );
            ai.setSequenceStop();
            const auto afterStop = settled();
            R.check( playing > 0.02f && afterStop < 0.005f, "transport stop releases a held note", "held " + f3( playing ) + " after " + f3( afterStop ) );

            // 2. Changing the track's program releases the old program's notes.
            held();
            ai.setProgramToTrack( prog, seq, ht ); // drum kit
            const auto afterProgChange = settled();
            R.check( afterProgChange < 0.005f, "program change releases a held note", "after " + f3( afterProgChange ) );
            ai.setProgramToTrack( ip, seq, ht );

            // 3. Swapping the engine while a note is held.
            held();
            if( ins )
                ins->setEngine( "Basic Synth" );
            ins = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( ip ) );
            const auto afterSwap = settled();
            R.check( afterSwap < 0.005f, "engine swap with a held note stays clean", "after " + f3( afterSwap ) );

            // 4. Transpose (semi) changed while a note is held: the note-off must still match.
            held();
            if( ins )
                ins->getState().setProperty( mt::IDs::semi, 5, nullptr );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 57 ) );
            const auto afterSemi = settled();
            R.check( afterSemi < 0.005f, "semi change while held still releases", "after " + f3( afterSemi ) );
            if( ins )
                ins->getState().setProperty( mt::IDs::semi, 0, nullptr );

            // 5. Muting the track while a note is held: the release must get through.
            held();
            ai.setTrackMute( seq, ht, true );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 57 ) );
            ai.setTrackMute( seq, ht, false );
            const auto afterMute = settled();
            R.check( afterMute < 0.005f, "release while muted is not lost", "after " + f3( afterMute ) );

            // 6. A note shorter than one block still sounds.
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 64, (juce::uint8)120 ) );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 64 ) );
            const auto blip = measureOutput( meter, 300 );
            R.check( blip > 0.005f, "very short note is audible", "peak " + f3( blip ) );
            settled();

            // Audit A9/A13: a retrigger survives an earlier short release, and CC64
            // holds released keys until the pedal rises, including while muted.
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOn(1, 64, (juce::uint8)110));
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 64));
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOn(1, 64, (juce::uint8)110));
            measureOutput(meter, 500); // Exclude the initial blip and an obsolete release tail.
            const auto retriggered = measureOutput(meter, 200);
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 64));
            R.check(retriggered > 0.02f && settled() < 0.005f, "A9: an earlier deferred off does not stop the retrigger");
            held();
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::controllerEvent(1, 64, 127));
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 57));
            measureOutput(meter, 700); // An unsupported pedal would have released by now.
            const auto sustained = measureOutput(meter, 200);
            ai.setTrackMute(seq, ht, true);
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::controllerEvent(1, 64, 0));
            ai.setTrackMute(seq, ht, false);
            R.check(sustained > 0.02f && settled() < 0.005f, "A13: sustain holds a released key and pedal-up releases it through mute");

            held();
            juce::Array<juce::MidiMessage> pedalBatch;
            pedalBatch.add(juce::MidiMessage::noteOff(1, 57));
            pedalBatch.add(juce::MidiMessage::controllerEvent(1, 64, 127));
            if(ins) ins->noteProcess(pedalBatch);
            const auto releasedBeforePedal = settled();
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::controllerEvent(1, 64, 0));
            ai.addLiveMidiMsg(ai.getSelected(), juce::MidiMessage::noteOff(1, 57));
            R.check(releasedBeforePedal < 0.005f, "A13: a pedal pressed after note-off in a batch does not recapture the key");

            // 7. Tree changes reach the engine (this is what undo/redo do).
            if( ins )
            {
                auto paramsVT = ins->getState().getOrCreateChildWithName( mt::IDs::INSTRUMENT_PARAMS, nullptr );
                paramsVT.setProperty( "level", 0.f, nullptr );
                measureOutput( meter, 100 ); // level is smoothed over 20 ms
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 60, (juce::uint8)120 ) );
                const auto silent = measureOutput( meter, 300 );
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 60 ) );
                settled();
                paramsVT.setProperty( "level", 0.7f, nullptr );
                measureOutput( meter, 100 );
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOn( 1, 60, (juce::uint8)120 ) );
                const auto loud = measureOutput( meter, 300 );
                ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 60 ) );
                settled();
                R.check( silent < 0.005f && loud > 0.02f, "parameter changes in the tree reach the engine (undo path)", "level 0 -> " + f3( silent ) + ", 0.7 -> " + f3( loud ) );
            }

            // 8. Pitch bend and mod wheel are accepted (no crash, still plays).
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::pitchWheel( 1, 12000 ) );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::controllerEvent( 1, 1, 100 ) );
            const auto bent = held();
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::noteOff( 1, 57 ) );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::pitchWheel( 1, 8192 ) );
            ai.addLiveMidiMsg( ai.getSelected(), juce::MidiMessage::controllerEvent( 1, 1, 0 ) );
            R.check( bent > 0.02f && settled() < 0.005f, "pitch bend and mod wheel", "peak " + f3( bent ) );

            // 9. Unknown engine names fall back instead of leaving the program silent.
            if( ins )
            {
                ins->getState().setProperty( mt::IDs::instrumentEngine, "No Such Engine", nullptr );
                R.check( ins->getEngineName() == "Basic Synth", "unknown engine name falls back to the default", ins->getEngineName() );
            }
        }

        //--------------------------------------------------------------------------
        R.section( "Instrument save and reload" );
        {
            auto exeDir    = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
            auto template1 = exeDir.getChildFile( "templates/Template 01/Template 01.kickPrj" );
            auto tempDir   = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_engine_test_project" );
            tempDir.deleteRecursively();
            tempDir.createDirectory();
            auto projFile = tempDir.getChildFile( "Roundtrip.kickPrj" );

            if( R.check( template1.copyFileTo( projFile ) && ai.loadProject( projFile ), "load a copy of Template 01", projFile.getFullPathName() ) )
            {
                pump( 500 );
                if( !ai.getNumSequences() )
                    ai.createSequence();
                const auto s2   = ai.getSelectedSequence();
                const auto t2   = ai.createTrack( s2, "Synth" );
                const auto p2   = ai.createProgram( mt::MTProgramType::Plugin, "Saved Synth" );
                auto*      inst = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( p2 ) );
                ai.setProgramToTrack( p2, s2, t2 );
                if( inst )
                {
                    inst->setParam( mt::BasicSynthEngine::kCutoff, 777.f );
                    inst->setParam( mt::BasicSynthEngine::kRelease, 1.5f );
                    mt::PadLayout l;
                    l.mode       = mt::PadLayout::Mode::Thirds;
                    l.root       = 5;
                    l.scaleIndex = 3;
                    l.toTree( inst->getState().getOrCreateChildWithName( mt::IDs::PAD_LAYOUT, nullptr ) );
                }
                if( auto* mod = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( ai.createProgram( mt::MTProgramType::Plugin, "Saved Modular" ) ) ) )
                {
                    mod->setEngine( "Modular" );
                    mod->loadPreset( 4 );
                    mod->getPatch().getChildWithProperty( mt::modular::PatchIDs::type, "filter" ).setProperty( "cutoff", 500.f, nullptr );
                }

                const auto saved = ai.saveProject();
                ai.closeProject();
                pump( 300 );
                const auto reloaded = ai.loadProject( projFile );
                pump( 800 );
                R.check( saved && reloaded, "save, close and reload", juce::String( "saved " ) + ( saved ? "yes" : "no" ) + ", reloaded " + ( reloaded ? "yes" : "no" ) );

                const auto pi = ai.getProgramIndexByName( "Saved Synth" );
                auto*      re = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( pi ) );
                if( R.check( re != nullptr, "instrument program comes back as an instrument", "index " + juce::String( pi ) ) )
                {
                    R.check( re->getEngineName() == "Basic Synth", "engine restored", re->getEngineName() );
                    R.check( std::abs( re->getParam( mt::BasicSynthEngine::kCutoff ) - 777.f ) < 0.01f && std::abs( re->getParam( mt::BasicSynthEngine::kRelease ) - 1.5f ) < 0.001f,
                             "parameters restored", f3( re->getParam( mt::BasicSynthEngine::kCutoff ) ) + " Hz, release " + f3( re->getParam( mt::BasicSynthEngine::kRelease ) ) );
                    const auto l = mt::PadLayout::fromTree( re->getState().getChildWithName( mt::IDs::PAD_LAYOUT ) );
                    R.check( l.mode == mt::PadLayout::Mode::Thirds && l.root == 5 && l.scaleIndex == 3, "pad layout restored" );

                    ai.addLiveMidiNote( pi, 60, 110 );
                    const auto sound = measureOutput( meter, 400 );
                    ai.stopLiveMidiNote( pi, 60 );
                    R.check( sound > 0.02f, "reloaded instrument plays", "peak " + f3( sound ) );
                    measureOutput( meter, 1800 );

                    // Undo through the project's undo manager reverts the parameter, and the
                    // engine follows (valueTreePropertyChanged).
                    ai.getUndoManager().beginNewTransaction();
                    re->setParam( mt::BasicSynthEngine::kLevel, 0.f );
                    ai.getUndoManager().beginNewTransaction();
                    ai.performUndo();
                    measureOutput( meter, 100 );
                    ai.addLiveMidiNote( pi, 60, 110 );
                    const auto afterUndo = measureOutput( meter, 400 );
                    ai.stopLiveMidiNote( pi, 60 );
                    R.check( re->getParam( mt::BasicSynthEngine::kLevel ) > 0.5f && afterUndo > 0.02f, "undo restores the parameter and the sound", "level " + f3( re->getParam( mt::BasicSynthEngine::kLevel ) ) + ", peak " + f3( afterUndo ) );
                    measureOutput( meter, 1800 );
                }

                const auto mi = ai.getProgramIndexByName( "Saved Modular" );
                if( auto* rm = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( mi ) ) )
                {
                    const float cutoff = rm->getPatch().getChildWithProperty( mt::modular::PatchIDs::type, "filter" ).getProperty( "cutoff" );
                    R.check( rm->getEngineName() == "Modular" && rm->getPatchName() == "Wobble Bass" && std::abs( cutoff - 500.f ) < 0.01f, "Modular patch restored with its edits",
                             rm->getEngineName() + " / " + rm->getPatchName() + ", cutoff " + f3( cutoff ) );
                    ai.addLiveMidiNote( mi, 48, 110 );
                    const auto sound = measureOutput( meter, 400 );
                    ai.stopLiveMidiNote( mi, 48 );
                    R.check( sound > 0.02f, "reloaded Modular patch plays", "peak " + f3( sound ) );
                    measureOutput( meter, 600 );

                    // A loaded project has an undo manager: a preset load undoes to the edited patch.
                    auto& um = ai.getUndoManager();
                    um.beginNewTransaction();
                    rm->loadPreset( 3 );
                    const auto loaded = rm->getPatchName();
                    um.beginNewTransaction();
                    ai.performUndo();
                    const float back = rm->getPatch().getChildWithProperty( mt::modular::PatchIDs::type, "filter" ).getProperty( "cutoff" );
                    R.check( loaded == "FM Bell" && rm->getPatchName() == "Wobble Bass" && std::abs( back - 500.f ) < 0.01f, "undoing a preset load restores the edited patch",
                             loaded + " -> " + rm->getPatchName() + ", cutoff " + f3( back ) );
                    ai.addLiveMidiNote( mi, 48, 110 );
                    const auto undone = measureOutput( meter, 400 );
                    ai.stopLiveMidiNote( mi, 48 );
                    R.check( undone > 0.02f, "and the engine plays it", "peak " + f3( undone ) );
                    measureOutput( meter, 600 );
                }
                else
                    R.check( false, "Modular program comes back", "index " + juce::String( mi ) );
                ai.closeProject();
                pump( 300 );
            }
            tempDir.deleteRecursively();
        }

        //--------------------------------------------------------------------------
        R.section( "Sampler through the engine (records the device input)" );
        bool started = false, stopped = false;
        double length = -1;
        mt::MTRecorderListener listener;
        listener.onRecorderStarted       = [&]() { started = true; };
        listener.onRecorderStopped       = [&]() { stopped = true; };
        listener.onRecorderLengthChanged = [&]( double t ) { length = t; };
        ai.setRecorderListener( listener );

        ai.enableRecorder( true );
        ai.setRecorderThreshold( 0.f );
        ai.setupRecorder( 1.0, 1.f, 0.f );
        ai.startRecorder( (int)mt::Recorder::LR_STEREO, true, true );
        pump( 1400 );

        auto& recBuf  = ai.getRecorderAudioBuffer();
        const auto sr = dev->getCurrentSampleRate();
        R.check( started && stopped, "start and stop callbacks fired", juce::String( "started " ) + ( started ? "yes" : "no" ) + ", stopped " + ( stopped ? "yes" : "no" ) );
        R.check( std::abs( recBuf.getNumSamples() - sr ) < 2 * testBlock, "recorded 1 s at the device rate", juce::String( recBuf.getNumSamples() ) + " samples @ " + juce::String( sr ) );
        R.check( std::abs( length - 1.0 ) < 0.02, "reported length is 1 s", f3( length ) );
        R.note( "input level while recording (room/mic): rms " + f3( recBuf.getNumSamples() ? rms( recBuf.getReadPointer( 0 ), recBuf.getNumSamples() ) : 0 ) );

        auto saved = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_engine_sampler_test.wav" );
        ai.saveRecording( saved.getFullPathName().toStdString() );
        R.check( saved.existsAsFile() && saved.getSize() > 1000, "saveRecording wrote a file", juce::String( saved.getSize() ) + " bytes" );
        saved.deleteFile();

        //--------------------------------------------------------------------------
        R.section( "Looper through the engine" );
        ai.setLooperEnable( true );
        ai.setLooperThreshold( 0.f );
        ai.setLooperLength( 1.0 );
        ai.setLooperRecord( true );
        ai.looperStart( true, true );
        pump( 300 );
        R.check( ai.isLooperRunning() && ai.isLooperRecording(), "record starts the loop" );
        pump( 1000 );
        R.check( !ai.isLooperEmpty(), "loop has content after one pass" );
        R.check( ai.getLooperProgress() >= 0.0 && ai.getLooperProgress() < 1.0, "progress stays within the loop", f3( ai.getLooperProgress() ) );
        ai.setLooperPlay();
        pump( 200 );
        R.check( ai.isLooperPlay() && !ai.isLooperRecording(), "switches to playback" );
        ai.looperStop();
        ai.looperClear();
        R.check( !ai.isLooperRunning() && ai.isLooperEmpty(), "stop and clear" );
    }

//==============================================================================
    /// KICK_TEST_RELOAD_CYCLES=N: close and reload one project N times, playing an instrument
    /// after each load. Hunts the intermittent "silent after reload" bug.
    void testReloadStress( Results& R, int Cycles )
    {
        R.section( "Reload stress (" + juce::String( Cycles ) + " cycles)" );

        auto& ai = mt::getAudioInterface();
        pump( 1500 );
        auto& dm = ai.getAudioDeviceManager();
        if( !R.check( dm.getCurrentAudioDevice() != nullptr, "audio device opened" ) )
            return;
        auto meter = dm.getOutputLevelGetter();

        auto dir = juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( "kick_reload_stress" ).getChildFile( "Stress" );
        dir.getParentDirectory().deleteRecursively();
        dir.createDirectory();

        if( !R.check( ai.loadProject( dir ), "create the project" ) )
            return;
        pump( 500 );
        if( !ai.getNumSequences() )
            ai.createSequence();
        const auto seq  = ai.getSelectedSequence();
        const auto trk  = ai.createTrack( seq, "Synth" );
        const auto prog = ai.createProgram( mt::MTProgramType::Plugin, "Stress Synth" );
        ai.setProgramToTrack( prog, seq, trk );
        ai.addLiveMidiNote( prog, 60, 110 );
        const auto first = measureOutput( meter, 400 );
        ai.stopLiveMidiNote( prog, 60 );
        measureOutput( meter, 600 );
        R.check( first > 0.02f, "plays before any reload", "peak " + f3( first ) );
        R.check( ai.saveProject(), "save" );

        int silent = 0;
        for( int i = 1; i <= Cycles; ++i )
        {
            ai.closeProject();
            pump( 300 );
            const bool loaded = ai.loadProject( dir );
            pump( 800 );
            const auto pi = ai.getProgramIndexByName( "Stress Synth" );
            ai.addLiveMidiNote( pi, 60, 110 );
            // Device output and the program's own output meter (dB) over the note.
            float peak = 0, progDb = -100.f;
            for( auto end = juce::Time::getMillisecondCounter() + 400; juce::Time::getMillisecondCounter() < end; )
            {
                pump( 5 );
                peak = juce::jmax( peak, (float)meter->getCurrentLevel() );
                if( auto* p = ai.getProgram( pi ) )
                    for( auto& m : p->readOutMeter() )
                        progDb = juce::jmax( progDb, m.peak );
            }
            ai.stopLiveMidiNote( pi, 60 );
            const auto cpu = dm.getCpuUsage();
            measureOutput( meter, 600 );
            if( !loaded || peak < 0.02f )
            {
                ++silent;
                auto* dev = dm.getCurrentAudioDevice();
                R.note( "cycle " + juce::String( i ) + ": loaded " + juce::String( loaded ? "yes" : "no" ) + ", program index " + juce::String( pi ) + ", peak " + f3( peak )
                        + ", program meter " + f3( progDb ) + " dB"
                        + [&] {
                              auto* ins = dynamic_cast<mt::MTInstrumentProgram*>( ai.getProgram( pi ) );
                              auto* obj = ins ? ins->getAudioObject() : nullptr;
                              if( !obj )
                                  return juce::String( ", no audio object" );
                              return ", output '" + ins->getOutput() + "', object " + juce::String( obj->getId() ) + " -> " + juce::String( obj->getConnectedTo( 0 ) ) + " / "
                                     + juce::String( obj->getConnectedTo( 1 ) ) + ", default out " + juce::String( AudIO::DawRouting::getDefaultStereoOutID() );
                          }()
                        + ", device playing " + juce::String( dev && dev->isPlaying() ? "yes" : "no" ) + ", callback load " + f3( cpu ) );
            }
        }
        R.check( silent == 0, "every reload is audible", juce::String( silent ) + " silent of " + juce::String( Cycles ) );
        ai.closeProject();
        pump( 300 );
        dir.getParentDirectory().deleteRecursively();
    }

} // namespace

#include "AuditRegressionTests.h"
#include "OptimizationRegressionTests.h"
#include "DesktopPlatformRegressionTests.h"
#include "TimingRecordingTests.h"
#include "ProgramMixerTests.h"
#include "InstrumentVoiceTests.h"
#include "AudioWorkflowTests.h"
#include "ProjectWorkflowTests.h"

class KickEngineTestApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "KickEngineTest"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise( const juce::String& CommandLine ) override
    {
#if JUCE_WINDOWS
        SetUnhandledExceptionFilter( writeCrashTrace );
    #ifdef _DEBUG
        // Report CRT asserts to the debugger output instead of blocking on a dialog.
        for( int type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT } )
            _CrtSetReportMode( type, _CRTDBG_MODE_DEBUG );
    #endif
#endif
        // The suite pumps events itself. Starting it inside a message callback
        // re-enters JUCE 7's Linux FD callback vector and can strand the quit message.
        run( CommandLine.trim().unquoted() );
    }

    void shutdown() override { mt::getAudioInterface().shutdown(); }

private:
    void run( const juce::String& KitArg )
    {
        auto exeDir = juce::File::getSpecialLocation( juce::File::currentExecutableFile ).getParentDirectory();
        auto kitDir = KitArg.isNotEmpty() ? juce::File( KitArg ) : exeDir.getChildFile( "samples" ).getChildFile( "Test Kit" );

        Results R;

        // The engine owns the AudIO memory pool, so create it before any buffers.
        mt::getAudioInterface();

        if( const auto cycles = juce::SystemStats::getEnvironmentVariable( "KICK_TEST_RELOAD_CYCLES", {} ).getIntValue(); cycles > 0 )
        {
            testReloadStress( R, cycles );
            R.lines.add( "\n" + juce::String( R.passed ) + " passed, " + juce::String( R.failed ) + " failed" );
            exeDir.getChildFile( "engine_test_results.txt" ).replaceWithText( R.lines.joinIntoString( "\n" ) );
            setApplicationReturnValue( R.failed );
            quit();
            return;
        }

        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_PROJECT_WORKFLOW_ONLY", {}).getIntValue() != 0)
        {
            testProjectWorkflow(R);
            R.lines.add("\n" + juce::String(R.passed) + " passed, " + juce::String(R.failed) + " failed");
            exeDir.getChildFile("engine_test_results.txt").replaceWithText(R.lines.joinIntoString("\n"));
            setApplicationReturnValue(R.failed); quit(); return;
        }
        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_AUDIO_WORKFLOW_ONLY", {}).getIntValue() != 0)
        {
            testAudioWorkflow(R);
            R.lines.add("\n" + juce::String(R.passed) + " passed, " + juce::String(R.failed) + " failed");
            exeDir.getChildFile("engine_test_results.txt").replaceWithText(R.lines.joinIntoString("\n"));
            setApplicationReturnValue(R.failed); quit(); return;
        }
        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_INSTRUMENT_ONLY", {}).getIntValue() != 0)
        {
            testInstrumentVoices(R);
            R.lines.add("\n" + juce::String(R.passed) + " passed, " + juce::String(R.failed) + " failed");
            exeDir.getChildFile("engine_test_results.txt").replaceWithText(R.lines.joinIntoString("\n"));
            setApplicationReturnValue(R.failed); quit(); return;
        }
        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_PROGRAM_MIXER_ONLY", {}).getIntValue() != 0)
        {
            testProgramMixer(R);
            R.lines.add("\n" + juce::String(R.passed) + " passed, " + juce::String(R.failed) + " failed");
            exeDir.getChildFile("engine_test_results.txt").replaceWithText(R.lines.joinIntoString("\n"));
            setApplicationReturnValue(R.failed); quit(); return;
        }
        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_TIMING_ONLY", {}).getIntValue() != 0)
        {
            testTimingRecording(R);
            R.lines.add("\n" + juce::String(R.passed) + " passed, " + juce::String(R.failed) + " failed");
            exeDir.getChildFile("engine_test_results.txt").replaceWithText(R.lines.joinIntoString("\n"));
            setApplicationReturnValue(R.failed); quit(); return;
        }
        const bool optimizationOnly = juce::SystemStats::getEnvironmentVariable("KICK_TEST_OPTIMIZE_ONLY", {}).getIntValue() != 0;
        if(!optimizationOnly && juce::SystemStats::getEnvironmentVariable("KICK_TEST_AUDIT_ONLY", {}).getIntValue() == 0)
        {
            testObjectIds( R );
            testSampleRecorder( R );
            testLooper( R );
            testFilter( R );
            testADSR( R );
            testBasicSynth( R );
            testModularEngine( R );
            testModularPatchEditing( R );
            testModularCablesAndEffects( R );
            testModularPlayModes( R );
            testModularTempoArpAndModules( R );
            testPadLayout( R );
            testEngine( R, kitDir );
        }
        if(!optimizationOnly) testAuditRegressions(R);
        testOptimizationWave(R);
        testDesktopPlatform(R);
        if(!optimizationOnly) testTimingRecording(R);
        if(!optimizationOnly) testProgramMixer(R);
        if(!optimizationOnly) testInstrumentVoices(R);
        if(!optimizationOnly) testAudioWorkflow(R);
        if(!optimizationOnly) testProjectWorkflow(R);

        R.lines.add( "\n" + juce::String( R.passed ) + " passed, " + juce::String( R.failed ) + " failed" );
        exeDir.getChildFile( "engine_test_results.txt" ).replaceWithText( R.lines.joinIntoString( "\n" ) );

        setApplicationReturnValue( R.failed );
        quit();
    }
};

START_JUCE_APPLICATION( KickEngineTestApp )
