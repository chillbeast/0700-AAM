namespace kick_ui
{
    AudioGridCutTool::AudioGridCutTool( AudioGridComponent& GridComp, AudioGridData& GridData, juce::Viewport& ViewPort )
        : IAudioTools( GridComp, GridData, ViewPort, AudioGridToolType::GridCutTool )
    {
    }

    void AudioGridCutTool::paint( juce::Graphics& G )
    {
        if( x1 != x2 && x1 >= 0 && x2 > 0 )
        {
            juce::Rectangle<float> selRect( x1, 0, x2 - x1, gridComp.getHeight() );
            juce::Colour           transparentRed = juce::Colour::fromRGBA( 128, 128, 128, 64 );
            G.setColour( transparentRed );
            G.fillRect( selRect );
            G.setColour( juce::Colours::lightgrey );
            G.drawRect( selRect, 1.5f );
        }
    }

    void AudioGridCutTool::mouseUp(const juce::MouseEvent& event)
    {
        if(event.mouseWasClicked())
            if(auto* program = dynamic_cast<mt::MTAudioProgram*>(ae.getProgram(ae.getSelectedProgram())))
            {
                program->selectAllSamples(false); program->selectAudioSample(mDownTime);
                program->splitSelectedAudio(gridData.getTimeFromX(event.position.x), ae.getSequenceBPM(ae.getSelectedSequence()) * mt::defaultPPQN / 60.0);
                sampleNeedRepaint();
            }
        x1 = x2 = 0; gridComp.repaint();
    }

    void AudioGridCutTool::mouseDrag( const juce::MouseEvent& Event )
    {
        auto startTime = gridData.getTimeFromX( Event.position.x );
        if( auto pProg = dynamic_cast<mt::MTAudioProgram*>( ae.getProgram( ae.getSelectedProgram() ) ) )
        {
            pProg->selectAudioSample( startTime );
            sampleNeedRepaint();
            if( auto pSample = pProg->getAudioSample( mDownTime ) )
                return;
        }
        auto currX  = gridData.getXFromTime( startTime );
        auto mDownX = gridData.getXFromTime( gridData.getTimeFromX( Event.getMouseDownX() ) );
        if( currX == mDownX )
        {
            x1 = mDownX;
            x2 = currX + gridData.getQLWidth( 0 );
        }
        else
        {
            auto deltaX = currX - mDownX;
            if( deltaX < 0 )
            {
                x1 = currX < 0 ? 0 : currX;
                x2 = mDownX + gridData.getQLWidth( 0 );
            }
            else
            {
                x1 = mDownX;
                x2 = currX + gridData.getQLWidth( 0 );
            }
        }
        gridComp.repaint();
    }
}
