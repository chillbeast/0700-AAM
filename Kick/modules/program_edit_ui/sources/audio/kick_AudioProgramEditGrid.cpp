
namespace kick_ui
{

    const float AudioGridData::gridWidth{ 1002.f };

    AudioGridData::AudioGridData()
    { //Add listener to audio engine
        ae.addListener( this );
    }

    AudioGridData::~AudioGridData()
    { //Remove listener to audio engine
        ae.removeListener( this );
    }

    void AudioGridData::refresh( bool Notify )
    { // Obtain tempo manager...
        juce::MessageManager::callAsync( [&, Notify]() {
            tempoMgr = ae.getTempoMgr( aeSel.sequence );
            // Obtain bar length
            numBars = ae.getTrackLength( aeSel.sequence, aeSel.track );
            if( numBars < 1 )
                numBars = ae.getSequenceLength( aeSel.sequence );
            setZoomFactor( 0.f );
            // Notify...
            recalculate( Notify );
        } );
    }

    void AudioGridData::onTrackSelectionChange( const mt::MTSelected& Selected )
    {
        refresh( true );
    };

    void AudioGridData::onTrackProgramChanged( int, int )
    {
        refresh( true );
    }

    void AudioGridData::onPlayHeadChanged( [[maybe_unused]] mt::MTBarNBeats BnB )
    {
        audioGridPlayHeadChanged();
    }

    void AudioGridData::recalculate( bool Notify )
    {
        if( Notify )
            audioGridChanged();
    }

    void AudioGridData::setZoomFactor( float horizontalFactor )
    {
        setHorizantalZoomFactor( horizontalFactor );
    }

    void AudioGridData::setHorizantalZoomFactor( float horozontalFactor )
    {
        float horTemp = zoom.width + horozontalFactor;
        if( gridWidth > ( getNumBars() * horTemp ) )
            horTemp = gridWidth / getNumBars();

        zoom.width = horTemp;
        recalculate( true );
    }

    void AudioGridData::setDefaultZoom()
    {
        zoom.width = 501.f;
        setZoomFactor( 0.f );
    }

    int AudioGridData::getXFromTime( int Time )
    {
        auto bar      = tempoMgr ? tempoMgr->getBarAtTick( Time ) : 0;
        auto remTicks = Time - ( tempoMgr ? tempoMgr->getTicksUpto( bar ) : 0 );

        auto x = bar * getBarWidth();
        x += ( remTicks * getXPerTick( bar ) );
        return x;
    }

    int AudioGridData::getTimeFromX( int X, bool Quantize )
    {
        auto timeX = X;
        if( Quantize )
            timeX = getQuantizedXFromX( X );

        auto bar  = (int)( timeX / getBarWidth() );
        auto remX = timeX - ( bar * getBarWidth() );

        auto ticksUptoBar = bar * ( tempoMgr ? tempoMgr->getTicksPerBar( bar ) : 0 );
        auto remTicks     = getTicksForX( bar, remX );

        return ticksUptoBar + remTicks;
    }

    unsigned int AudioGridData::getTicksUptoBar( unsigned int Bar )
    {
        return tempoMgr ? tempoMgr->getTicksUpto( Bar ) : 0;
    }

    unsigned int AudioGridData::getTicksPerQuantizationAt( unsigned int Ticks )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerQuantization() : 0;
    }

    mt::MTTimeBase AudioGridData::getTimeBase( const juce::String& Timebase )
    {
        mt::MTTimeBase timebase{ mt::MTTimeBase::_4 };

        if( Timebase.equalsIgnoreCase( "1" ) )
            timebase = mt::MTTimeBase::_1;
        else if( Timebase.equalsIgnoreCase( "2" ) )
            timebase = mt::MTTimeBase::_2;
        else if( Timebase.equalsIgnoreCase( "2T" ) )
            timebase = mt::MTTimeBase::_2T;
        else if( Timebase.equalsIgnoreCase( "4" ) )
            timebase = mt::MTTimeBase::_4;
        else if( Timebase.equalsIgnoreCase( "4T" ) )
            timebase = mt::MTTimeBase::_4T;
        else if( Timebase.equalsIgnoreCase( "8" ) )
            timebase = mt::MTTimeBase::_8;
        else if( Timebase.equalsIgnoreCase( "8T" ) )
            timebase = mt::MTTimeBase::_8T;
        else if( Timebase.equalsIgnoreCase( "16" ) )
            timebase = mt::MTTimeBase::_16;
        else if( Timebase.equalsIgnoreCase( "16T" ) )
            timebase = mt::MTTimeBase::_16T;
        else if( Timebase.equalsIgnoreCase( "32" ) )
            timebase = mt::MTTimeBase::_32;
        else if( Timebase.equalsIgnoreCase( "32T" ) )
            timebase = mt::MTTimeBase::_32T;
        else if( Timebase.equalsIgnoreCase( "64" ) )
            timebase = mt::MTTimeBase::_64;
        else if( Timebase.equalsIgnoreCase( "64T" ) )
            timebase = mt::MTTimeBase::_64T;

        return timebase;
    }

    int AudioGridData::getTimeBase( mt::MTTimeBase Timebase )
    {
        switch( Timebase )
        {
            case mt::MTTimeBase::_1: return 1;
            case mt::MTTimeBase::_2: return 2;
            case mt::MTTimeBase::_2T: return 3;
            case mt::MTTimeBase::_4: return 4;
            case mt::MTTimeBase::_4T: return 6;
            case mt::MTTimeBase::_8: return 8;
            case mt::MTTimeBase::_8T: return 12;
            case mt::MTTimeBase::_16: return 16;
            case mt::MTTimeBase::_16T: return 24;
            case mt::MTTimeBase::_32: return 32;
            case mt::MTTimeBase::_32T: return 78;
            case mt::MTTimeBase::_64: return 64;
            case mt::MTTimeBase::_64T: return 96;
            default: return 4;
        }
    }

    int AudioGridData::getTimeBaseIndex( int TimeDiv )
    {
        auto index = 5;
        switch( TimeDiv )
        {
            case 1: index = 0; break;
            case 4: index = 1; break;
            case 6: index = 2; break;
            case 8: index = 3; break;
            case 12: index = 4; break;
            case 16: index = 5; break;
            case 24: index = 6; break;
            case 32: index = 7; break;
            case 78: index = 8; break;
            case 64: index = 9; break;
            case 96: index = 10; break;
            default: index = 5; break;
        }
        return index;
    }

    unsigned int AudioGridData::getTicksPerTimebaseAt( unsigned int Ticks, const juce::String& Timebase )
    {
        auto timebase = getTimeBase( Timebase );
        return getTicksPerTimebaseAt( Ticks, timebase );
    }

    unsigned int AudioGridData::getTicksPerTimebaseAt( unsigned int Ticks, mt::MTTimeBase Timebase )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerTimebase( Timebase ) : 0;
    }

    unsigned int AudioGridData::getTicksPerBarAt( unsigned int Ticks )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerBar() : 0;
    }

    int AudioGridData::getBarFromX( int X ) const
    {
        auto bar = (int)( X / getBarWidth() );
        return bar;
    }

    int AudioGridData::getXFromBar( int Bar ) const
    {
        auto x = Bar * getBarWidth();
        return x;
    }

    int AudioGridData::getQuantizedXFromX( int X ) const
    {
        auto bar  = getBarFromX( X );
        auto x    = ( bar * getBarWidth() );
        auto remX = X - x;

        auto numQl  = getNumQlPerBar( bar );
        auto xPerQl = getBarWidth() / numQl;

        x += (int)( remX / xPerQl ) * xPerQl;
        return x;
    }

    AudioPlayhead::AudioPlayhead()
    {
        setInterceptsMouseClicks( false, false );
    }

    void AudioPlayhead::drawAt( float NormalizedX, [[maybe_unused]] int MaxWidth )
    {
        auto prevX = normX;
        normX      = juce::jlimit( 0.f, (float)getWidth(), NormalizedX );

        if( prevX != normX )
            triggerAsyncUpdate();
    }

    void AudioPlayhead::handleAsyncUpdate()
    {
        repaint();
    }

    void AudioPlayhead::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();

        if( b.isEmpty() )
            return;

        // Set colour...
        g.setColour( ui::Theme::selectionColour( 4 ) );

        // Draw Vertical line.
        auto centerX = (int)( normX );
        g.drawVerticalLine( centerX, 0, b.getHeight() );

        // Draw triangle playhead
        juce::Path triangle;
        triangle.addTriangle( ( centerX - playHeadWidth_2 ), 0, ( centerX + playHeadWidth_2 ), 0, centerX, playHeadHeight );
        g.fillPath( triangle );
    }

    BrowseSamplePopup::BrowseSamplePopup()
    {
        mt::utils::addAndMakeVisible( this, availableSamples, barNumber, beatNumber, tickNumber );
        availableSamples.setJustificationType( juce::Justification::centredLeft );
        availableSamples.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );
        auto mgr     = ae.getTempoMgr( ae.getSelectedSequence() );
        auto numBars = ae.getSequenceLength( ae.getSelectedSequence() );

        barNumber.setRange( 1, numBars, 1 );
        beatNumber.setRange( 1, mgr->getBeatsPerBar( numBars ), 1 );
        tickNumber.setRange( 1, ( mgr->getTicksPerBar( numBars ) ) / beatNumber.getMax(), 1 );

        barNumber.onValueChanged = [&]( int value ) {
            bbtValue.bar = value;
        };

        beatNumber.onValueChanged = [&]( int Value ) {
            bbtValue.beat = Value;
        };

        tickNumber.onValueChanged = [&]( int Value ) {
            bbtValue.tick = Value;
        };

        availableSamples.onSelectionChanged = [&]( int index, const juce::String& SampleName ) {
            selectedSample = SampleName;
        };
        refresh();
    }
    void BrowseSamplePopup::refresh()
    {
        barNumber.setValue( bbtValue.bar, juce::dontSendNotification );
        beatNumber.setValue( bbtValue.beat, juce::dontSendNotification );
        tickNumber.setValue( bbtValue.tick, juce::dontSendNotification );
    }
    void BrowseSamplePopup::resized()
    {
        availableSamples.setBounds( 155, 120, 305, 60 );
        barNumber.setBounds( 155, 200, 95.6, 64 );
        beatNumber.setBounds( 260, 200, 95.6, 64 );
        tickNumber.setBounds( 365, 200, 95.6, 64 );
    }
    void BrowseSamplePopup::onPlayHeadChanged( mt::MTBarNBeats BnB )
    {
        /*DBG( "BEAT" << BnB.beat );
        bar.setValue( BnB.bar, juce::dontSendNotification );
        beat.setValue( BnB.beat, juce::dontSendNotification );
        tick.setValue( BnB.tick, juce::dontSendNotification );*/
    }
    void BrowseSamplePopup::visibilityChanged()
    {
        samples = mt::getAudioInterface().getAssetNames( mt::AssetTypes::Samples );
        if( samples.size() == 0 )
            samples.push_back( "None" );
        availableSamples.setListOptions( samples );
        availableSamples.setSelected( 0 );
        juce ::String s = availableSamples.getSelected();
        DBG( s );
        // onPlayHeadChanged( BnBValue );
        refresh();
    }

    void AudioScaleComponent::mouseUp( const juce::MouseEvent& Event )
    {
        if( Event.mouseWasClicked() )
        {
            auto normX = juce::jlimit( 0.f, (float)getWidth(), (float)Event.getMouseDownX() );
            if( onMouseClick )
                onMouseClick( normX );
        }
    }

    void AudioScaleComponent::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
        auto offset = 3;
        auto b      = getLocalBounds();
        b.removeFromTop( (int)( 0.35 * b.getHeight() ) );

        const auto beatColour = ui::Theme::gridColour( 2 );
        const auto barColour  = ui::Theme::gridColour( 3 );

        juce::String text;
        g.setFont( juce::jmin( 26.0f, ( 0.9f * b.getHeight() ) ) );

        int   bar  = 1;
        int   beat = 0;
        float xPos = 0.f;
        float x    = 0.f;

        juce::Rectangle<float> tArea = b.toFloat();

        auto numBars  = gridData.getNumBars();
        auto barWidth = gridData.getBarWidth();
        auto tempMgr  = ae.getTempoMgr( ae.getSelectedSequence() );
        if( tempMgr )
        {
            if( mt::getAudioInterface().getNumSequences() <= 0 )
            {
                barWidth = getWidth();
                numBars  = 1;
            }

            for( int i = 1; i <= numBars; ++i )
            {
                auto beatsPerBar = gridData.getBeatsPerBar( ( i - 1 ) );
                auto beatWidth   = barWidth / beatsPerBar;

                // Draw Bar Marker...
                x = xPos;
                g.setColour( barColour );
                g.drawVerticalLine( xPos, b.getY(), b.getY() + b.getHeight() );

                text = text.formatted( "%d", bar );
                tArea.setHorizontalRange( { ( xPos + offset ), ( xPos + beatWidth ) } );
                g.drawFittedText( text, tArea.toNearestInt(), juce::Justification::centredLeft, 1 );

                // Draw Beat Marker...
                beat = 1;
                g.setColour( beatColour );
                for( int q = 0; q < beatsPerBar - 1; ++q )
                {
                    x += beatWidth;
                    g.drawVerticalLine( x, b.getY(), b.getY() + b.getHeight() );

                    text = text.formatted( "%d.%d", bar, ++beat );
                    tArea.setHorizontalRange( { ( x + offset ), ( x + beatWidth ) } );
                    g.drawFittedText( text, tArea.toNearestInt(), juce::Justification::centredLeft, 1 );
                }
                xPos += barWidth;
                ++bar;
            }
            g.setColour( barColour );
            g.drawVerticalLine( xPos, b.getY(), b.getY() + b.getHeight() );
        }
    }

    AudioGridComponent::AudioGridComponent( juce::Viewport& ViewPort )
        : gridViewPort( ViewPort )
    {
        {
            auto s = new AudioGridSelectTool( *this, gridData, gridViewPort );
            auto e = new AudioGridEraseTool( *this, gridData, gridViewPort );
            auto d = new AudioGridDragTool( *this, gridData, gridViewPort );
            auto c = new AudioGridCutTool( *this, gridData, gridViewPort );
            auto p = new AudioGridPencilTool( *this, gridData, gridViewPort );
            auto m = new AudioGridMagnifyTool( *this, gridData, gridViewPort );

            tools.insert( std::make_pair( s->getType(), s ) );
            tools.insert( std::make_pair( e->getType(), e ) );
            tools.insert( std::make_pair( d->getType(), d ) );
            tools.insert( std::make_pair( c->getType(), c ) );
            tools.insert( std::make_pair( p->getType(), p ) );
            tools.insert( std::make_pair( m->getType(), m ) );
        }

        activeTool = tools.begin()->second;

        sampleNeedRepaint.connect( [&]() {
            triggerAsyncUpdate();
        }, signalScope);
    }
    AudioGridComponent::~AudioGridComponent()
    {
        signalScope.reset(); cancelPendingUpdate();
        for( auto tool : tools )
        {
            delete tool.second;
        }
    }

    void AudioGridComponent::mouseUp( const juce::MouseEvent& event )
    {
        if( activeTool ) activeTool->mouseUp( event );
    }
    void AudioGridComponent::mouseMove( const juce::MouseEvent& event )
    {
        if( activeTool ) activeTool->mouseMove( event );
    }
    void AudioGridComponent::mouseDrag( const juce::MouseEvent& event )
    {
        if( activeTool ) activeTool->mouseDrag( event );
    }
    void AudioGridComponent::mouseDown( const juce::MouseEvent& event )
    {
        if( activeTool ) activeTool->mouseDown( event );
    }
    void AudioGridComponent::toolTypeChanged( AudioGridToolType toolType )
    {
        auto it    = tools.find( toolType );
        activeTool = ( it != tools.end() ) ? it->second : nullptr;
        setMouseCursor( juce::MouseCursor::NormalCursor );
    }

    void AudioGridComponent::resized()
    {
    }

    void AudioGridComponent::paint( juce::Graphics& g )
    {
        auto canvas     = getLocalBounds().toFloat();
        auto qColour    = ui::Theme::gridColour( 1 ).withMultipliedBrightness( 1.2 );
        auto beatColour = ui::Theme::gridColour( 2 );
        auto barColour  = ui::Theme::gridColour( 3 );

        auto numBars = ae.getSequenceLength();
        auto tempMgr = ae.getTempoMgr( ae.getSelectedSequence() );

        if( mt::getAudioInterface().getNumSequences() > 0 && tempMgr )
        {
            ////Draw bar beat
            auto barWidth  = gridData.getBarWidth();
            auto drawWidth = barWidth * numBars;

            float x  = 0.f;
            float qx = 0.f;

            juce::Rectangle<float> barArea{ x, canvas.getY(), drawWidth, canvas.getHeight() };

            //Draw Bars and Beats
            for( int bar = 0; bar < numBars; ++bar )
            {
                auto qlWidth   = gridData.getQLWidth( bar );
                auto beatWidth = gridData.getBarWidth() / gridData.getBeatsPerBar( bar );

                //Draw Beat Background
                auto thisBarArea = barArea.removeFromLeft( barWidth );
                g.fillCheckerBoard( thisBarArea, beatWidth, thisBarArea.getHeight(), ui::Theme::backgroundColour( 1 ).withAlpha( 0.9f ), ui::Theme::backgroundColour( 3 ).withAlpha( 0.9f ) );

                //QuantizationLevels
                g.setColour( qColour );
                auto drawQl = gridData.getNumQlPerBar( bar );
                for( int q = 0; q < drawQl; ++q )
                {
                    qx += qlWidth;
                    g.drawLine( qx, 0.f, qx + 1.f, canvas.getHeight() );
                }

                //Draw Bar line
                g.setColour( barColour );
                g.drawLine( x, 0.f, x + 1.f, canvas.getHeight() );
                x += barWidth;
            }
            if( activeTool ) activeTool->paint( g );
        }
        else
        {
            float x             = 0.f;
            auto  gridCompWidth = getWidth();

            // Draw the beats and ticks within the single bar
            juce::Rectangle<float> barArea{ x, (float)canvas.getY(), (float)gridCompWidth, (float)canvas.getHeight() };
            g.fillCheckerBoard( barArea, (float)gridCompWidth / 4, canvas.getHeight(), ui::Theme::backgroundColour( 1 ).withAlpha( 0.9f ), ui::Theme::backgroundColour( 3 ).withAlpha( 0.9f ) );

            qColour = ui::Theme::gridColour( 1 );
            for( int beat = 0; beat < 4; ++beat )
            {
                float beatX = beat * gridCompWidth / 4;
                for( int tick = 1; tick < 4; ++tick )
                {
                    float tickX = beatX + ( tick * gridCompWidth / 16 );
                    g.drawLine( tickX, 0, tickX, canvas.getHeight(), 0.5f );
                }
            }
        }
    }

    void AudioGridComponent::visibilityChanged()
    {
        if( isVisible() )
        {
            refresh();
        }
    }

    void AudioGridComponent::refresh()
    {
        pAudio = dynamic_cast<mt::MTAudioProgram*>( ae.getProgram( ae.getSelectedProgram() ) );

        if( pAudio )
        {
            juce::Array<std::unique_ptr<mt::MTIAudio>> audioClipsData = pAudio->getAudioClipsData();
            auto                                       samplesList    = pAudio->getAudioSampleList();
            auto                                       selectedList   = pAudio->getSelectedAudioSampleList();
            int                                        index          = 0;
            int                                        min{ 0 };
            int                                        max{ 1 };
            wavComps.clear();

            if( selectedList.isEmpty() || selectedList.size() > 1 )
                stateNeedChage( false );
            else
                stateNeedChage( true );

            for( auto vt : samplesList )
            {
                mt::MTIAudio audioSample( vt, &ae.getUndoManager() );

                auto& comp = wavComps.emplace_back( std::make_unique<ui::WaveRenderer>() );
                addAndMakeVisible( comp.get() );

                auto& b = wavComps[index];
                mt::utils::addAndMakeVisible( this, b.get() );

                auto tempMgr = ae.getTempoMgr( ae.getSelectedSequence() );
                if( tempMgr )
                {
                    auto pixelsPerTick = gridData.getXPerTick( 0 );
                    auto startX        = audioSample.startTime.get() * pixelsPerTick;

                    if( auto pSample = mt::getAudioSoundCache().exists( audioSample.source.get() ) )
                    {
                        max                   = pSample->getNumSamplesPerChannel();
                        auto   buffer         = pSample->getData();
                        auto   tempo          = tempMgr->getBPM();
                        double ticksPerSecond = tempMgr->getTicksPerBeat() / ( BPM::TOSEC / tempo );

                        // Calculate sample length
                        auto sampleLengthInPixels = audioSample.length.get() * pixelsPerTick;

                        b->setBounds( startX, 0, sampleLengthInPixels, getHeight() );
                        b->setSelected( selectedList.contains( vt ) );
                        b->setInterceptsMouseClicks( false, false );
                    }
                }

                ++index;
            }

            index = 0;

            for( auto& audioSample : audioClipsData )
            {
                auto& b = wavComps[index];

                if( auto pSample = mt::getAudioSoundCache().exists( audioSample->source.get() ) )
                {
                    int sampleRate = pSample->getSampleRate();

                    b->setSource( pSample->getData(), sampleRate );
                    b->setLevelOrReverseSample( audioSample->level.get(), audioSample->reverse.get() );
                    b->setMute( audioSample->mute.get() );
                    b->setRange( { audioSample->start.get() / static_cast<double>( pSample->getSampleRate() ), ( audioSample->end.get() == 0 ? max / static_cast<double>( pSample->getSampleRate() ) : audioSample->end.get() / static_cast<double>( pSample->getSampleRate() ) ) } );
                }
                b->setBackgroundColour( ui::Theme::backgroundColour( 9 ) );
                ++index;
            }
        }
    }
    void AudioGridComponent::handleAsyncUpdate()
    {
        repaint();
        refresh();
    }

    AudioGridParameterComponent::AudioGridParameterComponent( mt::MTAudioProgram*& AudioPgm )
        : pAudio( AudioPgm )

    {
    }

    void AudioGridParameterComponent::setSelectedParam( const juce::String& ParamName )
    {
        paramName = ParamName;
        repaint();
    }

    void AudioGridParameterComponent::clearDataForSelectedParam()
    {
        if(pAudio) { pAudio->clearAutomation(paramName); repaint(); }
    }

    void AudioGridParameterComponent::setSelectedTool( AudioGridToolType ToolType )
    {
        toolType = ToolType;
    }

    void AudioGridParameterComponent::paint( juce::Graphics& g )
    {
        auto b          = getLocalBounds();
        int  viewHeight = b.getHeight();
        auto tempMgr    = gridData.getTempoMgr();
        auto barWidth   = gridData.getBarWidth();

        // Draw background...
        auto color = ui::Theme::backgroundColour( 1 );
        g.fillAll( color );

        g.setColour( ui::Theme::outlineColour( 1 ) );
        g.fillRect( 0.f, 0.f, (float)getWidth(), 1.f );

        // Draw vertical grid lines
        g.setColour( ui::Theme::gridColour( 1 ) );
        auto xPos = 0.f;

        for( auto bar = 0; bar < gridData.getNumBars(); ++bar )
        {
            auto qlWidth = gridData.getQLWidth( bar );
            auto numQl   = gridData.getNumQlPerBar( bar );
            auto x       = xPos;
            for( ; x < b.getWidth(); x += qlWidth )
            {
                g.drawVerticalLine( x, 0, b.getHeight() );
            }
            xPos = x;
        }

        if(pAudio && ae.isProjectLoaded() && paramName != "None")
        {
            g.fillAll( ui::Theme::backgroundColour( 9 ) );
            g.setColour( juce::Colours::blue );
            auto vt     = pAudio->getParamTree( paramName );
            int  height = 0;
            int  ys     = b.getHeight();

            if( vt.isValid() )
            {
                for( int i = 0; i < vt.getNumProperties(); ++i )
                {
                    auto propertyName = vt.getPropertyName( i ).toString();
                    if( propertyName.startsWith( "s" ) )
                    {
                        auto numberPart   = propertyName.removeCharacters( "s" ).getIntValue();
                        numberPart        = gridData.getXFromTime( numberPart );
                        int PropertyValue = vt.getProperty( propertyName );
                        int w             = barWidth / gridData.getNumQlPerBar( 0 );

                        height     = (int)( ys * ( PropertyValue / 100.f ) );
                        auto yAxis = ys - ( height - 1 );
                        if( paramName == "Mute" )
                        {
                            g.setColour( ui::Theme::backgroundColour( 12 ) );

                            g.fillRect( numberPart, PropertyValue > 0 ? b.getY() : b.getBottom(), ( w > 0 ? w : 1 ), PropertyValue > 0 ? b.getHeight() : 1 );

                            g.setColour( juce::Colours::yellow );
                            g.fillRect( numberPart, PropertyValue > 0 ? b.getY() : b.getBottom(), ( w > 0 ? w : 1 ), 1 );
                        }
                        else if( paramName == "PAN" )
                        {
                            int middleY = b.getY() + b.getHeight() / 2;
                            PropertyValue = b.getY() + b.getHeight() * PropertyValue / 100;
                            g.setColour( juce::Colours::white );
                            g.drawLine( b.getX(), middleY, b.getRight(), middleY, .5f );
                            g.setColour( ui::Theme::backgroundColour( 12 ) );

                            if( PropertyValue > middleY )
                            {
                                float heightFromMiddle = PropertyValue - middleY;
                                heightFromMiddle       = std::min( heightFromMiddle, (float)( b.getHeight() - middleY ) );

                                g.fillRect( (float)numberPart, middleY - heightFromMiddle, (float)w, heightFromMiddle );

                                g.setColour( juce::Colours::yellow );
                                g.fillRect( (float)numberPart, middleY - heightFromMiddle, (float)w, 1.0f );
                            }
                            else if( PropertyValue < middleY )
                            {
                                float heightFromMiddle = middleY - PropertyValue;
                                heightFromMiddle       = std::min( heightFromMiddle, (float)middleY );

                                g.fillRect( (float)numberPart, (float)middleY, (float)w, heightFromMiddle );

                                g.setColour( juce::Colours::yellow );
                                g.fillRect( (float)numberPart, middleY + heightFromMiddle - 1.0f, (float)w, 1.0f );
                            }
                        }
                        else
                        {
                            g.setColour( ui::Theme::backgroundColour( 12 ) );
                            g.fillRect( numberPart, std::min( yAxis, b.getHeight() - 1 ), w, ( height > 0 ? height : 1 ) );

                            g.setColour( juce::Colours::yellow );
                            g.fillRect( numberPart, std::min( yAxis, b.getHeight() - 1 ), w, 1 );
                        }
                    }
                }
            }
        }
    }

    void AudioGridParameterComponent::mouseUp( const juce::MouseEvent& Event )
    {
        if( Event.mouseWasClicked() )
            handleMouseEvent( Event );
    }

    void AudioGridParameterComponent::mouseDrag( const juce::MouseEvent& Event )
    {
        handleMouseEvent( Event );
    }

    void AudioGridParameterComponent::handleMouseEvent( const juce::MouseEvent& Event )
    {
        if(!pAudio || getHeight() < 1 || paramName == "None") return;
        if(toolType == AudioGridToolType::GridEraseTool)
        {
            pAudio->eraseAutomationPoint(paramName, gridData.getTimeFromX(Event.x)); repaint(); return;
        }
        float currentX = Event.x;
        float currentY = Event.y;
        if( toolType == AudioGridToolType::GridPencilTool && currentY <= getHeight() && currentY >= 0 )
        {
            if( currentY >= 0 && paramName != "None" )
            {
                if( pAudio )
                {
                    auto start = gridData.getTimeFromX( currentX );
                    auto value = static_cast<int>( 100.f * ( 1.f - ( static_cast<float>( currentY ) / getHeight() ) ) );
                    if(paramName == "Mute") value = value >= 50 ? 100 : 0;
                    pAudio->setValueForParam( paramName, start, value );
                }
            }

            repaint();
        }
    }

    AudioProgramGrid::AudioProgramGrid( mt::MTAudioProgram*& Audio )
        : pAudio( Audio ), gridComponent( audiogridViewport )
    {
        mt::utils::addAndMakeVisible( this, audiogridViewport, scaleViewport, paramViewport, phViewport, playhead, /*gridComponent, scaleComp, paramComp,*/ warp, reverse, mute, sampleList, fadeIn, fadeOut, bpm, level, semi, fine, tc, dontSnap, nudge, editStart, editEnd, split, duplicate, clearAuto, automation, ecBtn, plusButton, minusButton, zoomLabel /*, sampleComp */ );

        mt::utils::setRadioGroupID( (int)RadioGroupId::ParamViewControl, nudge, editStart, editEnd );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), warp, reverse, mute, sampleList, tc, dontSnap, nudge, editStart, editEnd, split, duplicate, clearAuto );
        zoomLabel.setValueJustification( juce::Justification::centred );
        zoomLabel.setColours( ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 3 ) );
        headerTools.selectSample.setToggleState( true, juce::dontSendNotification );
        zoomLabel.setValueFontHeight( 16 );
        automation.setListOptions( { "Volume", "PAN", "Mute", "Send1", "Send2", "Send3", "Send4" } );
        plusButton.toFront( true );
        minusButton.toFront( true );

        mt::utils::setTitleProportion( .535f, fadeIn, fadeOut, semi, fine, level, bpm );
        mt::utils::setTitleJustification( juce::Justification::centred, fadeIn, fadeOut, semi, fine, level, bpm );

        // Add components to viewports...
        audiogridViewport.setViewedComponent( &gridComponent, false );
        scaleViewport.setViewedComponent( &scaleComp, false );
        paramViewport.setViewedComponent( &paramComp, false );
        phViewport.setViewedComponent( &playhead, false );
        // Scroll bars...
        audiogridViewport.setScrollBarsShown( false, false, true, true );
        scaleViewport.setScrollBarsShown( false, false, false, false );
        paramViewport.setScrollBarsShown( false, false, false, true );
        phViewport.setScrollBarsShown( false, false, true, false );
        // Viewport Listners..
        audiogridViewport.addListener( this );
        scaleViewport.addListener( this );
        paramViewport.addListener( this );
        phViewport.addListener( this );

        sampleNeedRepaint.connect( [&]() {
            refresh();
        }, signalScope);

        gridData.audioGridChanged.connect( [&] {
            gridComponent.setSize( gridData.getWidth(), audiogridViewport.getHeight() );
            scaleComp.setSize( gridData.getWidth(), scaleViewport.getHeight() );
            playhead.setSize( gridData.getWidth(), audiogridViewport.getHeight() + scaleViewport.getHeight() );
            paramComp.setSize( gridData.getWidth(), paramViewport.getHeight() );
        }, signalScope);

        stateNeedChage.connect( [&]( bool State ) {
            mt::utils::setEnabled( State, reverse, warp, mute, fadeIn, fadeOut, fine, semi, bpm, level, nudge, editEnd, editStart, duplicate, split );
        }, signalScope);

        audiogridViewport.setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::all );
        scaleViewport.setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::all );

        scaleComp.onMouseClick = [&]( double Value ) {
            //Seek the playhead position to play
            auto tickPos = gridData.getTimeFromX( Value );
            editCursor = tickPos; ae.seek(tickPos);
            updateAudioPlayhead();
        };

        headerTools.bars.onValueChanged = [&]( int Bars ) {
            ae.setTrackLength( aeSel.sequence, aeSel.track, Bars );
            gridData.refresh();
        };

        headerTools.selectAll.onClick = [&]() {
            if( headerTools.selectAll.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridSelectTool );
        };

        headerTools.erase.onClick = [&]() {
            if( headerTools.erase.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridEraseTool );
        };

        headerTools.pencil.onClick = [&]() {
            if( headerTools.pencil.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridPencilTool );
        };

        headerTools.selectSample.onClick = [&]() {
            if( headerTools.selectSample.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridDragTool );
        };

        headerTools.cut.onClick = [&]() {
            if( headerTools.cut.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridCutTool );
        };

        headerTools.magnify.onClick = [&]() {
            if( headerTools.magnify.getToggleState() ) headerTools.toolSelectionChanged( AudioGridToolType::GridMagnifyTool );
        };

        headerTools.settings.onClick = [&]() {
            gridSettings.setScreen( ui::GridSettings::GridAudio );
            mt::utils::showDialog( "SETTINGS", &gridSettings, [this]( int R ) {}, { 0, 0, 650, 520 } );
        };
        headerTools.copy.setComponentID("audio-copy"); headerTools.paste.setComponentID("audio-paste");
        headerTools.paste.setVisible(true);
        headerTools.copy.onClick = [this] {
            if(!pAudio) return;
            audioClipboard = pAudio->copySelectedAudio(); clipboardProject = ae.getProjectDir();
            if(audioClipboard.getNumChildren() == 0) mt::utils::showNotification("Select audio regions to copy");
        };
        headerTools.paste.onClick = [this] {
            auto* tempo = ae.getTempoMgr(ae.getSelectedSequence());
            const int at = editCursor >= 0 ? editCursor : gridData.getTimeFromX((int)(gridData.playHead.normized * gridComponent.getWidth()), false);
            if(!pAudio || !tempo || clipboardProject != ae.getProjectDir() || !pAudio->pasteAudio(audioClipboard, at, tempo->getTicksUpto(ae.getSequenceLength())))
                mt::utils::showNotification("Copy regions first and choose a paste position with enough space");
            else gridComponent.refresh();
        };

        components = {
            &sampleList, &fadeIn, &fadeOut, &level, &semi, &fine, &bpm, &warp, &reverse,
            &mute, &duplicate
        };
        level.setRange(-60, 12, 1);
        scaleComp.setComponentID("audio-scale"); semi.setComponentID("audio-semi"); fine.setComponentID("audio-fine");
        fadeIn.setComponentID("audio-fade-in"); fadeOut.setComponentID("audio-fade-out");
        split.setComponentID("audio-split"); clearAuto.setComponentID("audio-clear-auto"); automation.setComponentID("audio-automation");
        semi.onValueChanged = [this](int value) { if(pAudio) pAudio->setSelectedAudioProperty(mt::IDs::semi, value); };
        fine.onValueChanged = [this](int value) { if(pAudio) pAudio->setSelectedAudioProperty(mt::IDs::fine, value); };
        fadeIn.onValueChanged = [this](int value) { if(pAudio) pAudio->setSelectedAudioProperty("fadeIn", value); };
        fadeOut.onValueChanged = [this](int value) { if(pAudio) pAudio->setSelectedAudioProperty("fadeOut", value); };

        headerTools.toolSelectionChanged.connect( [&]( AudioGridToolType Type ) {
            gridComponent.toolTypeChanged( Type );
            paramComp.setSelectedTool( Type );
        } );

        mute.onClick = [&]() {
            bool isMuted = mute.getToggleState();
            pAudio->setSelectedAudioSampleMute( isMuted );
            gridComponent.refresh();
        };

        warp.onClick = [&]() {
            bool isWarp = warp.getToggleState();
            pAudio->setSelectedAudioSampleWarp( isWarp );

            mt::utils::setEnabled(true, semi, fine); mt::utils::setEnabled(isWarp, bpm);
        };

        bpm.onValueChanged = [&]( int value ) {
            pAudio->setSelectedAudioSampleBPM( value );
        };

        reverse.onClick = [&]() {
            bool isReverse = reverse.getToggleState();
            pAudio->setSelectedAudioSampleReverse( isReverse );
            gridComponent.refresh();
        };

        split.onClick = [&]() {
            if( pAudio )
            {
                auto& selSample       = pAudio->getSelectedAudioSampleList();
                auto  playHeadPos     = gridData.playHead.normized * gridComponent.getWidth();
                auto  playHeadPosTick = gridData.getTimeFromX( playHeadPos, false );
                if(pAudio->splitSelectedAudio(editCursor >= 0 ? editCursor : playHeadPosTick, ae.getSequenceBPM(ae.getSelectedSequence()) * mt::defaultPPQN / 60.0)) gridComponent.refresh();
            }
        };

        duplicate.onClick = [&]() {
            auto& sel = pAudio->getSelectedAudioSampleList();

            for( auto& sample : sel )
            {
                mt::MTIAudio audioSample( sample, &ae.getUndoManager() );
                int          sampleStartTick     = audioSample.startTime.get();
                auto         sampleLengthInTicks = getSampleLengthInTicks( audioSample.source.get(), audioSample.start.get(), audioSample.end.get() );
                auto         startX              = sampleStartTick + sampleLengthInTicks;

                pAudio->addSample( sample.getProperty( mt::IDs::source ), startX, sampleLengthInTicks, 0 );

                auto vt = pAudio->getClipTree( startX );
                for( int i = 0; i < sample.getNumProperties(); ++i )
                    if( auto propName = sample.getPropertyName( i ); propName != mt::IDs::source && propName != mt::IDs::startTime )
                        vt.setProperty( propName, sample.getProperty( propName ), &ae.getUndoManager() );

                pAudio->selectAllSamples( false );
                pAudio->setSelectAudioSample( startX, true );
            }
            gridComponent.refresh();
        };

        level.onValueChanged = [&]( int value ) {
            pAudio->setSelectedAudioSampleLevel( value );
            gridComponent.refresh();
        };

        ecBtn.onClick = [&]() {
            resized();
            gridComponent.refresh();
        };

        plusButton.onClick = [this]() {
            setZoomInZoomOut( 20, true );
            gridComponent.refresh();
            plusButton.setToggleState( false, juce::dontSendNotification );
        };

        minusButton.onClick = [this]() {
            setZoomInZoomOut( -20, false );
            gridComponent.refresh();
            minusButton.setToggleState( false, juce::dontSendNotification );
        };

        clearAuto.onClick = [&]() { paramComp.clearDataForSelectedParam(); };

        automation.onSelectionChanged = [&]( int, const juce::String& Param ) { paramComp.setSelectedParam( Param ); };

        automation.setSelected( 0, juce::sendNotificationAsync );

        sampleList.onClick = [&]() {
            auto p = new BrowseSamplePopup();
            mt::utils::showOptions( "ADD SAMPLE AT PLAYHEAD POSITION", p, [&, p]( int R ) {
                if( R == UI::APPLY )
                {
                    auto selectedSample = p->getSelectedSample();
                    auto bbtValues      = p->getBBTValue();
                    auto sample         = mt::getAudioInterface().getAsset( mt::AssetTypes::Samples, p->availableSamples.getSelectedIndex() );

                    auto startX              = getStartTick( ( ( bbtValues.bar ) - 1 ), ( ( bbtValues.beat ) - 1 ), bbtValues.tick - 1 );
                    auto slice               = ae.getAssetSlice( sample );
                    auto sampleLengthInTicks = 0;
                    if( slice )
                        sampleLengthInTicks = getSampleLengthInTicks( sample, 0, slice->source->getNumSamplesPerChannel() );
                    pAudio->addSample( sample, startX, sampleLengthInTicks, 0 );
                    gridComponent.refresh();
                }
                if( p ) delete p;
            } );
        };
        refresh();
    }

    double AudioProgramGrid::getSampleLengthInTicks( const juce::String& Source, int LoopStart, int LoopEnd )
    {
        auto seqData = ae.getSequenceData( aeSel.sequence );
        if( auto pSample = mt::getAudioSoundCache().exists( Source ) )
        {
            auto tempo      = seqData ? seqData->tempo.get() : 120;
            auto sampleRate = pSample->getSampleRate();

            int    sampleLengthInSamples = ( LoopEnd == 0 ? pSample->getNumSamplesPerChannel() : LoopEnd ) - LoopStart;
            double sampleLengthInSeconds = static_cast<double>( sampleLengthInSamples ) / pSample->getSampleRate();
            auto   sampleLengthInTicks   = sampleLengthInSeconds * ( gridData.getTicksPerBeat( 0 ) / ( BPM::TOSEC / tempo ) );

            return sampleLengthInTicks;
        }
        return 0;
    }

    void AudioProgramGrid::updateOnViewPositionChange( ui::KickViewport* pViewport )
    {
        if( pViewport == &audiogridViewport )
        {
            scaleViewport.setViewPosition( audiogridViewport.getViewPositionX(), scaleViewport.getViewPositionY() );
            paramViewport.setViewPosition( audiogridViewport.getViewPositionX(), paramViewport.getViewPositionY() );
            phViewport.setViewPosition( audiogridViewport.getViewPositionX(), phViewport.getViewPositionY() );
        }
        else
        {
            auto x = pViewport->getViewPositionX();
            audiogridViewport.setViewPosition( x, audiogridViewport.getViewPositionY() );
        }
    }

    void AudioProgramGrid::setZoomInZoomOut( int xValue, bool isZoomIn )
    {
        auto gridwidth = gridData.getWidth();
        auto p         = audiogridViewport.getViewArea().getBottomLeft();
        auto viewPos   = audiogridViewport.getViewPosition();
        auto bar       = gridData.getBarFromX( viewPos.x );
        auto numQl     = viewPos.x / gridData.getQLWidth( bar );
        gridData.setZoomFactor( xValue );
        if( gridwidth != gridData.getWidth() )
        {
            auto xPos = numQl * gridData.getQLWidth( bar ) + xValue;
            audiogridViewport.setViewPosition( xPos, viewPos.y );
        }
    }

    void AudioProgramGrid::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        auto tempMgr = ae.getTempoMgr( ae.getSelectedSequence() );

        if( tempMgr != nullptr && contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto  value   = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            auto& sel     = pAudio->getSelectedAudioSampleList();
            auto  qlWidth = tempMgr->getTicksPerQuantization( 0 );

            if( nudge.getToggleState() )
                nudgeSelectedSample( sel, value, qlWidth );
            else if( editStart.getToggleState() || editEnd.getToggleState() )
            {
                if( pAudio )
                {
                    auto   pSeqData       = ae.getSequenceData( ae.getSelectedSequence() );
                    auto   tempo          = pSeqData ? pSeqData->tempo.get() : BPM::DEFAULT;
                    double ticksPerSecond = gridData.getTicksPerBeat( 0 ) / ( BPM::TOSEC / tempo );
                    for( auto& sample : sel )
                    {
                        mt::MTIAudio audioSample{ sample, &ae.getUndoManager() };
                        double       samplesPerTick = audioSample.src->getSourceSampleRate() / ticksPerSecond;
                        auto         needRefresh{ -1 };
                        if( editStart.getToggleState() )
                            needRefresh = pAudio->editSelectedSampleStart( value < 0 ? -qlWidth : qlWidth, samplesPerTick, value < 0 ? false : true );
                        else if( editEnd.getToggleState() )
                            needRefresh = pAudio->editSelectedSampleEnd( value < 0 ? -qlWidth : qlWidth, samplesPerTick, value < 0 ? false : true );
                        if( needRefresh == 0 ) gridComponent.refresh();
                    }
                }
            }
        }
    }

    void AudioProgramGrid::nudgeSelectedSample( juce::Array<juce::ValueTree>& SelSample, int Value, double qlWidth )
    {
        auto tempMgr = ae.getTempoMgr( ae.getSelectedSequence() );
        if( tempMgr != nullptr && !SelSample.isEmpty() )
        {
            auto nudgeWidth    = ( Value > 0 ) ? qlWidth : -qlWidth;
            auto totalTicks    = tempMgr->getTicksUpto( ae.getSequenceLength() );
            auto selectedStart = 0;
            for( auto& sample : SelSample )
            {
                mt::MTIAudio audiosample( sample, nullptr );

                audiosample.startTime = audiosample.startTime.get() + static_cast<int>( nudgeWidth );
                if( audiosample.startTime < 0 )
                    audiosample.startTime = 0;
                selectedStart = audiosample.startTime.get();
            }
            pAudio->selectAllSamples( false );
            pAudio->setSelectAudioSample( selectedStart, true );
            gridComponent.refresh();
        }
    }

    int AudioProgramGrid::getStartTick( int bar, int beat, int tick )
    {
        auto startTime = gridData.getTicksUptoBar( bar ) + ( gridData.getTicksPerBeat( bar ) * beat ) + tick;
        return startTime;
    }

    void AudioProgramGrid::resized()
    {
        auto b = getLocalBounds().reduced( 10 );
        scaleViewport.setBounds( b.removeFromTop( 40 ) );
        if( ecBtn.getToggleState() )
        {
            audiogridViewport.setBounds( b.removeFromTop( 141 ) );
            phViewport.setBounds( audiogridViewport.getX(), scaleViewport.getY(), audiogridViewport.getWidth(), audiogridViewport.getHeight() + scaleViewport.getHeight() );
            paramViewport.setBounds( b.removeFromTop( 135 ) );
        }
        else
        {
            audiogridViewport.setBounds( b.removeFromTop( 276 ) );
            phViewport.setBounds( audiogridViewport.getX(), scaleViewport.getY(), audiogridViewport.getWidth(), audiogridViewport.getHeight() + scaleViewport.getHeight() );
            paramViewport.setBounds( { 0, 0, 0, 0 } );
        }

        //Bottom Controls

        tc.setBoundsRelative( .0097f, .9039f, .042f, .0742f );
        dontSnap.setBoundsRelative( .0557f, .9039f, .1076f, .0742f );
        nudge.setBoundsRelative( .1673f, .9039f, .1076f, .0742f );
        editStart.setBoundsRelative( .2788f, .9039f, .1076f, .0742f );
        editEnd.setBoundsRelative( .3904f, .9039f, .1076f, .0742f );
        split.setBoundsRelative( .5019f, .9039f, .1076f, .0742f );
        zoomLabel.setBoundsRelative( .6135f, .9025f, .1076f, .078f );
        minusButton.setBoundsRelative( .6135f, .9039f, .0332f, .0742f );
        plusButton.setBoundsRelative( .6878f, .9039f, .0332f, .0742f );
        clearAuto.setBoundsRelative( .7250f, .9039f, .1076f, .0742f );
        automation.setBoundsRelative( .8366f, .9039f, .1076f, .0742f );
        ecBtn.setBoundsRelative( .9481f, .9039f, .042f, .0742f );

        //Middle Controls

        reverse.setBoundsRelative( .0097f, .8078f, .0939f, .0742f );
        mute.setBoundsRelative( .1076f, .8078f, .0939f, .0742f );
        warp.setBoundsRelative( .2054f, .8078f, .0939f, .0742f );
        duplicate.setBoundsRelative( .3033f, .8078f, .0939f, .0742f );

        sampleList.setBoundsRelative( .0097f, .7227f, .3874f, .0742f );
        fadeIn.setBoundsRelative( .4012f, .7227f, .0949f, .1593f );
        fadeOut.setBoundsRelative( .5f, .7227f, .0949f, .1593f );
        level.setBoundsRelative( .5988f, .7227f, .0949f, .1593f );
        semi.setBoundsRelative( .6976f, .7227f, .0949f, .1593f );
        fine.setBoundsRelative( .7964f, .7227f, .0949f, .1593f );
        bpm.setBoundsRelative( .8953f, .7227f, .094f, .1593f );

        gridData.audioGridChanged();
    }

    void AudioProgramGrid::refresh()
    {
        pAudio = ae.isProjectLoaded() ? dynamic_cast<mt::MTAudioProgram*>(ae.getProgram(ae.getSelectedProgram())) : nullptr;
        auto length = ae.getTrackLength( aeSel.sequence, aeSel.track );
        headerTools.bars.setValue( length, juce::dontSendNotification );
        if( pAudio )
        {
            auto& sel = pAudio->getSelectedAudioSampleList();
            for( auto& samples : sel )
            {
                mt::MTIAudio audioSample( samples, &ae.getUndoManager() );
                level.setValue( audioSample.level.get(), juce::dontSendNotification );
                mute.setToggleState( audioSample.mute.get(), juce::dontSendNotification );
                reverse.setToggleState( audioSample.reverse.get(), juce::dontSendNotification );
                warp.setToggleState( audioSample.warp.get(), juce::dontSendNotification );
                semi.setValue((int)samples.getProperty(mt::IDs::semi, 0), juce::dontSendNotification);
                fine.setValue((int)samples.getProperty(mt::IDs::fine, 0), juce::dontSendNotification);
                fadeIn.setValue((int)samples.getProperty("fadeIn", 0), juce::dontSendNotification);
                fadeOut.setValue((int)samples.getProperty("fadeOut", 0), juce::dontSendNotification);
                bpm.setValue( audioSample.bpm.get(), juce::dontSendNotification );
            }
        }
    }

    void AudioProgramGrid::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 8, 8, 1006, 401 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 8, sampleList.getY() - 5, 1006, 83 }, ui::Theme::backgroundColour( 11 ) );
    }

    void AudioProgramGrid::timerCallback()
    {
        updateAudioPlayhead();
    }

    void AudioProgramGrid::updateAudioPlayhead()
    {
        if( isVisible() && ae.isSequencePlaying() )
        {
            const auto& audioPlay = ae.getTrackPlayhead();
            auto        gridWidth = gridComponent.getWidth();
            auto        gridX     = (int)( audioPlay.normized * gridWidth );
            auto        viewWidth = audiogridViewport.getWidth();
            auto        yPos      = audiogridViewport.getViewPositionY();

            playhead.drawAt( gridX, gridWidth );

            auto scrollType = gridSettings.getAutoScrollType();
            if( scrollType == ui::AutoScrollMode::Follow )
            {
                auto halfWidth = viewWidth / 2;
                if( gridX < halfWidth )
                {
                    audiogridViewport.setViewPosition( 0, yPos );
                }

                else if( ( gridWidth - gridX ) >= halfWidth && gridX > halfWidth )
                {
                    audiogridViewport.setViewPosition( gridX - halfWidth, yPos );
                }
            }
            else if( scrollType == ui::AutoScrollMode::Page )
            {
                int tempPage = gridX / viewWidth;
                if( gridPage != tempPage )
                {
                    gridPage = tempPage;
                    audiogridViewport.setViewPosition( gridX, yPos );
                }
            }
        }
    }

    void AudioProgramGrid::visibilityChanged()
    {
        for( auto comp : getChildren() )
            comp->setVisible( isVisible() );

        if( isVisible() )
        {
            refresh();
            gridData.refresh( true );
            gridComponent.refresh();
            startTimer( 25 );
        }
    }
}
