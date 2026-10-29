
namespace ui
{
    WaveRenderer::WaveRenderer()
        : thumbnailCache( 1 ) // [4]
        , thumbnail( 512, formatManager, thumbnailCache ) // [5]
        , backgroundColour( ui::Theme::backgroundColour( 11 ) )
    {
        formatManager.registerBasicFormats();
    }

    void WaveRenderer::setSource( const juce::AudioSampleBuffer& buffer )
    {
        clearCache();
        thumbnail.reset( buffer.getNumChannels(), sampleRate, buffer.getNumSamples() );
        reRender( buffer, buffer.getNumSamples() );
        setRange( { 0, thumbnail.getTotalLength() } );
        setZoomFactor( hZoom );
    }

    void WaveRenderer::setSource( const juce::AudioSampleBuffer& buffer, double SampleRate )
    {
        sampleBuffer = buffer;
        sampleRate   = SampleRate;
        juce::AudioSampleBuffer tempBuffer( buffer );
        changeAmplitude( tempBuffer, level );
        // Setup thumbnail...
        setSource( tempBuffer );
    }

    void WaveRenderer::setSource( float* const* dataReferer, int numChannels, int numSamples, int SampleRate )
    {
        setSource( juce::AudioSampleBuffer( dataReferer, numChannels, numSamples ), SampleRate );
    }

    void WaveRenderer::setZoomFactor( double amount )
    {
        hZoom = juce::jlimit( 0.0, 0.99, amount );
        if( thumbnail.getTotalLength() > 0 )
        {
            auto newScale     = juce::jmax( 0.001, thumbnail.getTotalLength() * ( 1.0 - hZoom ) );
            auto timeAtCentre = xToTime( (float)getWidth() / 2.0f );

            setRange( { timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 } );
        }
    }

    void WaveRenderer::setRange( juce::Range<double> newRange )
    {
        visibleRange = newRange;
        repaint();
    }

    void WaveRenderer::clearCache()
    {
        thumbnail.clear();
        thumbnailCache.clear();
        repaint();
    }

    void WaveRenderer::reRender( const juce::AudioSampleBuffer& Data, int NumSamples, juce::int64 SampleNumberInSource, int StartOffsetInBuffer )
    {
        thumbnail.addBlock( SampleNumberInSource, Data, StartOffsetInBuffer, NumSamples );
    }
    void WaveRenderer::changeAmplitude( juce::AudioSampleBuffer& buffer, double Level )
    {
        double scalingFactor = juce::Decibels::decibelsToGain(juce::jlimit(-60.0, 12.0, Level));
        for( int channel = 0; channel < buffer.getNumChannels(); ++channel )
        {
            buffer.applyGain( channel, 0, buffer.getNumSamples(), scalingFactor );
        }
    }

    void WaveRenderer::setLevelOrReverseSample( double Level, bool IsReverse )
    {
        level = Level;
        juce::AudioSampleBuffer tempBuffer{ sampleBuffer };

        if( IsReverse )
        {
            for( int channel = 0; channel < tempBuffer.getNumChannels(); ++channel )
            {
                auto* channelData = tempBuffer.getWritePointer( channel );
                std::reverse( channelData, channelData + tempBuffer.getNumSamples() );
            }
        }
        setSource( tempBuffer, sampleRate );
    }

}
