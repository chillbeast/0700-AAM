namespace kick_ui
{
    //////////////////////////////////////////////////////////////////////
    // Grid Settings
    /////////////////////////////////////////////////////////////////////

    GridSettings::GridSettings()
    {
        mt::utils::addAndMakeVisible( this, snapMode, hittingPadsMode, autoScroll, autoAdvance );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), snapMode, hittingPadsMode, autoScroll, autoAdvance );
        mt::utils::setValueFontHeight( 20, snapMode, hittingPadsMode, autoScroll, autoAdvance );
        mt::utils::setTitleFontHeight( 16, snapMode, hittingPadsMode, autoScroll, autoAdvance );
        mt::utils::setTitleProportion( .4f, snapMode, hittingPadsMode, autoScroll, autoAdvance );
        mt::utils::makeHidden( autoAdvance );

        snapMode.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            if( onSnapModeChanged )
                onSnapModeChanged( Index, Value );
        };

        hittingPadsMode.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            if( onHittingPadModeChanged )
                onHittingPadModeChanged( Index == 1 );
        };

        autoScroll.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            if( onAutoScrollChanged )
                onAutoScrollChanged( Index, Value );
        };

        autoAdvance.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        };
    }

    bool GridSettings::getHittingPadMode()
    {
        return hittingPadsMode.getSelected() == 1;
    }

    bool GridSettings::getAutoAdvance()
    {
        return autoAdvance.getSelected() == 1;
    }

    int GridSettings::getSnapMode()
    {
        return snapMode.getSelected();
    }

    AutoScrollMode GridSettings::getAutoScrollType()
    {
        auto selected = autoScroll.getSelected();
        if( selected == 0 )
            return AutoScrollMode::Off;
        else if( selected == 1 )
            return AutoScrollMode::Follow;
        else if( selected == 2 )
            return AutoScrollMode::Page;
        return AutoScrollMode::Off;
    }

    void GridSettings::setScreen( ScreenType Screen )
    {
        screenType = Screen;
        resized();
    }

    void GridSettings::resized()
    {
        mt::utils::makeHidden( snapMode, hittingPadsMode, autoAdvance, autoScroll );
        if( screenType == Grid )
        {
            mt::utils::makeVisible( snapMode, hittingPadsMode, autoScroll );
            snapMode.setBoundsRelative( .1076f, .1458f, .784f, .2083f );
            hittingPadsMode.setBoundsRelative( .1076f, .396f, .784f, .2083f );
        }
        else if( screenType == ListEdit )
        {
            mt::utils::makeVisible( hittingPadsMode, autoAdvance, autoScroll );
            hittingPadsMode.setBoundsRelative( .1076f, .1458f, .784f, .2083f );
            autoAdvance.setBoundsRelative( .1076f, .396f, .784f, .2083f );
        }
        autoScroll.setBoundsRelative( .1076f, .645f, .784f, .2083f );
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // GridData
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const float GridData::gridWidth{ 864.f };
    const float GridData::gridHeight{ 416.f };

    GridData::GridData()
    { //Add listener to audio engine
        ae.addListener( this );
    }

    GridData::~GridData()
    { //Remove listener to audio engine
        ae.removeListener( this );
    }

    void GridData::refresh( bool Notify )
    { // Obtain tempo manager...
        juce::MessageManager::callAsync( [&, Notify]() {
            tempoMgr = ae.getTempoMgr( aeSel.sequence );
            // Obtain bar length
            numBars = ae.getTrackLength( aeSel.sequence, aeSel.track );
            if( numBars < 1 )
                numBars = ae.getSequenceLength( aeSel.sequence );
            setZoomFactor( 0.f, 0.f );
            // Notify...
            recalculate( Notify );
        } );
    }

    void GridData::onTrackSelectionChange( const mt::MTSelected& Selected )
    {
        refresh( true );
    };

    void GridData::onTrackProgramChanged( int, int )
    {
        refresh( true );
    }

    void GridData::onPlayHeadChanged( [[maybe_unused]] mt::MTBarNBeats BnB )
    {
        gridPlayHeadChanged();
    }

    void GridData::recalculate( bool Notify )
    {
        if( Notify )
            gridChanged();
    }

    void GridData::setZoomFactor( float horizontalFactor, float verticalFactor )
    {
        setHorizantalZoomFactor( horizontalFactor, false );
        setVerticalZoomFactor( verticalFactor, false );

        recalculate( true );
    }

    void GridData::setHorizantalZoomFactor( float horozontalFactor, bool Notify )
    {
        float horTemp = zoom.width + horozontalFactor;
        if( gridWidth > ( getNumBars() * horTemp ) )
            horTemp = gridWidth / getNumBars();

        zoom.width = horTemp;
        recalculate( Notify );
    }

    void GridData::setVerticalZoomFactor( float verticalFactor, bool Notify )
    {
        float verTemp = zoom.height + verticalFactor;

        auto maxNoteHeight = gridHeight / 8.f;
        auto minNoteHeight = gridHeight / 64.f;

        zoom.height = juce::jlimit( minNoteHeight, maxNoteHeight, verTemp );
        recalculate( Notify );
    }

    void GridData::setDefaultZoom()
    {
        zoom.width  = 432.f;
        zoom.height = 26.f;
        setZoomFactor( 0.f, 0.f );
    }

    // X, Y conversion API's
    float GridData::getYFromNote( int Note ) const
    {
        return ( ( mt::ScaleAndNotes::maxNotes - Note - 1 ) * getNoteHeight() );
    }

    int GridData::getNoteFromY( float Y ) const
    {
        return ( mt::ScaleAndNotes::maxNotes - ( Y / getNoteHeight() ) );
    }

    int GridData::getXFromTime( int Time )
    {
        auto bar      = tempoMgr ? tempoMgr->getBarAtTick( Time ) : 0;
        auto remTicks = Time - ( tempoMgr ? tempoMgr->getTicksUpto( bar ) : 0 );

        auto x = bar * getBarWidth();
        x += ( remTicks * getXPerTick( bar ) );
        return x;
    }

    int GridData::getTimeFromX( int X, bool Quantize )
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

    unsigned int GridData::getTicksUptoBar( unsigned int Bar )
    {
        return tempoMgr ? tempoMgr->getTicksUpto( Bar ) : 0;
    }

    unsigned int GridData::getTicksPerQuantizationAt( unsigned int Ticks )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerQuantization() : 0;
    }

    mt::MTTimeBase GridData::getTimeBase( const juce::String& Timebase )
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

    int GridData::getTimeBase( mt::MTTimeBase Timebase )
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

    int GridData::getTimeBaseIndex( int TimeDiv )
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
            case 48: index = 8; break;
            case 64: index = 9; break;
            case 96: index = 10; break;
            default: index = 5; break;
        }
        return index;
    }

    unsigned int GridData::getTicksPerTimebaseAt( unsigned int Ticks, const juce::String& Timebase )
    {
        auto timebase = getTimeBase( Timebase );
        return getTicksPerTimebaseAt( Ticks, timebase );
    }

    unsigned int GridData::getTicksPerTimebaseAt( unsigned int Ticks, mt::MTTimeBase Timebase )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerTimebase( Timebase ) : 0;
    }

    unsigned int GridData::getTicksPerBarAt( unsigned int Ticks )
    {
        return tempoMgr ? tempoMgr->getTempoSettingsAtTick( Ticks ).getTicksPerBar() : 0;
    }

    int GridData::getBarFromX( int X ) const
    {
        auto bar = (int)( X / getBarWidth() );
        return bar;
    }

    int GridData::getXFromBar( int Bar ) const
    {
        auto x = Bar * getBarWidth();
        return x;
    }

    int GridData::getQuantizedXFromX( int X ) const
    {
        auto bar  = getBarFromX( X );
        auto x    = ( bar * getBarWidth() );
        auto remX = X - x;

        auto numQl  = getNumQlPerBar( bar );
        auto xPerQl = getBarWidth() / numQl;

        x += (int)( remX / xPerQl ) * xPerQl;
        return x;
    }
}
