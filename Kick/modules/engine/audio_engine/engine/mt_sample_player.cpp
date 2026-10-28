#include "mt_sample_player.h"

#define broadcastMessageLengthInMs 3
#define cuePreviewLengthInMs       30

namespace mt
{
    MTSamplePlayer::MTSamplePlayer()
    {
        formatManager.registerBasicFormats();
        player.setSource( &transportSource );
        transportSource.setSource( this );
        transportSource.addChangeListener( this );
    }

    MTSamplePlayer::~MTSamplePlayer()
    {
        transportSource.stop();
        player.setSource( nullptr );
        devmgr.removeAudioCallback( &player );
    }

    void MTSamplePlayer::setSource( juce::File source )
    {
        transportSource.stop();
        devmgr.removeAudioCallback( &player );
        auto replacement = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(source));
        {
            const juce::ScopedLock guard(sourceLock);
            pos = length = regionStart = regionEnd = 0;
            sourceFile = source;
            reader = std::move(replacement);
            looping = false;
        }
        transportSource.setSource(nullptr);
        if( reader )
        {
            transportSource.setSource(this, 0, nullptr, reader->sampleRate);
            devmgr.addAudioCallback(&player);
        }
    }

    void MTSamplePlayer::start()
    {
        region = MTSampleRegion();
        start( region, MTSamplePlayMode::samplePlayFromStart );
    }

    void MTSamplePlayer::stop()
    {
        transportSource.stop();
    }

    bool MTSamplePlayer::isPlaying()
    {
        return transportSource.isPlaying();
    }

    void MTSamplePlayer::start( const MTSampleRegion& r, MTSamplePlayMode pm )
    {
        transportSource.stop();
        {
        const juce::ScopedLock guard(sourceLock);
        if( !reader || reader->lengthInSamples <= 0 ) return;
        playingBackwords = false;

        region      = r;
        playMode    = pm;
        regionStart = region.start;
        regionEnd   = region.end;

        if( region.isValid() )
        {
            if( playMode == MTSamplePlayMode::samplePlayFromStart || playMode == MTSamplePlayMode::samplePlayModeLoopForward
                || playMode == MTSamplePlayMode::samplePlayModeLoopBidirectional )
            {
                pos    = region.start;
                length = region.end;
            }
            else if( playMode == MTSamplePlayMode::samplePlayToStart )
            {
                pos    = 0;
                length = region.start;
            }
            else if( playMode == MTSamplePlayMode::samplePlayToEnd )
            {
                pos    = 0;
                length = region.end;
            }
            else if( playMode == MTSamplePlayMode::samplePlayFromEnd )
            {
                pos    = region.end;
                length = reader->lengthInSamples;
            }
            else if( playMode == MTSamplePlayMode::samplePlayModeLoopBackward )
            {
                playingBackwords = true;
                length           = region.end;
                pos              = length;
            }
            else if( playMode == MTSamplePlayMode::samplePlayModeCue )
            {
                int previewOffSet = static_cast<int>( ( reader->sampleRate * cuePreviewLengthInMs ) / 1000 );
                pos               = region.getStart();
                length            = pos + previewOffSet;
                pos               = pos - previewOffSet;
            }
            if( reader )
                length = juce::jmin( length, reader->lengthInSamples );
            else
                length = 0;
        }
        else
        {
            pos    = 0;
            length = reader->lengthInSamples;
        }

        //length = juce::jmin( length, reader->lengthInSamples );

        looping = ( playMode == MTSamplePlayMode::samplePlayModeLoopForward || playMode == MTSamplePlayMode::samplePlayModeLoopBackward || playMode == MTSamplePlayMode::samplePlayModeLoopBidirectional );

        pos = juce::jlimit<juce::int64>(0, reader->lengthInSamples, pos);
        length = juce::jlimit<juce::int64>(pos, reader->lengthInSamples, length);
        regionStart = juce::jlimit<juce::int64>(0, length, regionStart);
        regionEnd = juce::jlimit<juce::int64>(regionStart, length, regionEnd);
        if( pos == length && !playingBackwords ) return;
        }
        transportSource.start();
    }

    juce::int64 MTSamplePlayer::getNextReadPosition() const
    {
        const juce::ScopedLock guard(sourceLock);
        if( pos == length )
            return length + 2;
        return pos;
    }

    juce::int64 MTSamplePlayer::getTotalLength() const
    {
        const juce::ScopedLock guard(sourceLock);
        return length;
    }

    bool MTSamplePlayer::isLooping() const
    {
        const juce::ScopedLock guard(sourceLock);
        return looping;
    }

    void MTSamplePlayer::prepareToPlay( int samplesPerBlockExpected, double /*sr*/ )
    {
        const juce::ScopedLock guard(sourceLock);
        samplesPerBlock = samplesPerBlockExpected;
        if( reader ) broadcastElapsedSamples = juce::jmax(1, static_cast<int>(reader->sampleRate * broadcastMessageLengthInMs / 1000));
    }

    void MTSamplePlayer::releaseResources() {}

    void MTSamplePlayer::getNextAudioBlock( const juce::AudioSourceChannelInfo& bufferToFill )
    {
        const juce::ScopedLock guard(sourceLock);
        bufferToFill.clearActiveBufferRegion();
        if( !reader ) return;
        int sampleCopySize = 0;
        if( playingBackwords )
        {
            sampleCopySize = ( pos - regionStart < bufferToFill.numSamples ) ? static_cast<int>( std::max<juce::int64>(0, pos - regionStart) ) : bufferToFill.numSamples;
            reader->read( bufferToFill.buffer, bufferToFill.startSample, sampleCopySize, pos - sampleCopySize, true, true );
            bufferToFill.buffer->reverse( bufferToFill.startSample, sampleCopySize );
            pos -= sampleCopySize;
        }
        else
        {
            sampleCopySize = ( length - pos < bufferToFill.numSamples ) ? static_cast<int>( std::max<juce::int64>(0, length - pos) ) : bufferToFill.numSamples;
            reader->read( bufferToFill.buffer, bufferToFill.startSample, sampleCopySize, pos, true, true );
            pos += sampleCopySize;
        }

        // send playheade position message
        numSamplesElapsed += sampleCopySize;
        if( pos == length || numSamplesElapsed >= broadcastElapsedSamples )
        {
            numSamplesElapsed = 0;
            sendChangeMessage();
        }

        // reset pos if playmode is loop
        if( looping && ( pos >= regionEnd || pos <= regionStart ) )
        {
            if( playMode == MTSamplePlayMode::samplePlayModeLoopForward )
            {
                pos = regionStart;
            }
            else if( playMode == MTSamplePlayMode::samplePlayModeLoopBackward )
            {
                pos = regionEnd;
            }
            else if( playMode == MTSamplePlayMode::samplePlayModeLoopBidirectional )
            {
                if( pos <= regionStart )
                {
                    playingBackwords = false;
                }
                else
                {
                    pos              = length;
                    playingBackwords = true;
                }
            }
        }
    }

    double MTSamplePlayer::getSampleRate()
    {
        const juce::ScopedLock guard(sourceLock);
        if( reader )
            return reader->sampleRate;

        return 0.0;
    }

    void MTSamplePlayer::changeListenerCallback( ChangeBroadcaster* source )
    {
        // send playback stopped message
        if( source == &transportSource )
            sendChangeMessage();
    }
}
