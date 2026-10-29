
namespace kick_ui
{
    AudioGridPencilTool::AudioGridPencilTool( AudioGridComponent& GridComp, AudioGridData& GridData, juce::Viewport& ViewPort )
        : IAudioTools( GridComp, GridData, ViewPort, AudioGridToolType::GridPencilTool )
    {
    }

    void AudioGridPencilTool::paint( juce::Graphics& G )
    {
        if( x1 != x2 && x1 >= 0 && x2 > 0 )
        {
            juce::Rectangle<float> selRect( x1, 0, x2 - x1, gridComp.getHeight() );

            juce::Colour transparentRed = juce::Colour::fromRGBA( 128, 128, 128, 64 );
            G.setColour( transparentRed );
            G.fillRect( selRect );
            G.setColour( juce::Colours::lightgrey );
            G.drawRect( selRect, 1.5f );
        }
    }

    void AudioGridPencilTool::mouseUp( const juce::MouseEvent& Event )
    {
        if(auto* program = dynamic_cast<mt::MTAudioProgram*>(ae.getProgram(ae.getSelectedProgram())))
        {
            if(!program->getAudioSample(mDownTime))
            {
                auto sourceName = program->getDrawingSource();
                auto source = sourceName.isEmpty() ? mt::ASound::Ptr{} : ae.getAssetManager().getAssetSound(sourceName);
                auto* tempo = ae.getTempoMgr(ae.getSelectedSequence());
                if(source && source->isValid() && tempo)
                {
                    const double rate = ae.getSequenceBPM(ae.getSelectedSequence()) * mt::defaultPPQN / 60.0;
                    const int limit = tempo->getTicksUpto(ae.getSequenceLength());
                    const int start = juce::jlimit(0, juce::jmax(0, limit - 1), gridData.getTimeFromX(Event.mouseWasClicked() ? Event.position.x : juce::jmin(x1, x2)));
                    const int end = Event.mouseWasClicked() ? start + juce::jmax(1, (int)std::ceil(source->getLengthInSeconds() * rate)) : gridData.getTimeFromX(juce::jmax(x1, x2));
                    ae.getUndoManager().beginNewTransaction("Draw audio region");
                    program->addSample(sourceName, start, juce::jmax(1, juce::jmin(limit, end) - start), 0);
                    program->selectAllSamples(false); program->setSelectAudioSample(start, true); sampleNeedRepaint();
                }
                else mt::utils::showNotification("Add or select a sample before drawing an audio region");
            }
        }
        x1 = x2 = 0;
        gridComp.repaint();
    }

    void AudioGridPencilTool::mouseDrag( const juce::MouseEvent& Event )
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
