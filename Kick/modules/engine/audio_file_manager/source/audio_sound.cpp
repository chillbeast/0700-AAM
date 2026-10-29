/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

namespace mt
{

    ///
    /// ASound
    ///
    ASound::ASound( const juce::String& File, juce::AudioFormatManager& FmtMgr, float UpSampleRate, double MaxLengthInSeconds )
        : aFmtMgr( FmtMgr )
        , upSampleRate( UpSampleRate )
    {
        setSound( File, MaxLengthInSeconds );
    }

    ASound::ASound(juce::AudioFormatManager& fmt, double rate)
        : aFmtMgr(fmt), loaded(true), lengthInSamples((int)(rate * 0.035)), lengthInSeconds(0.035), upSampleRate(1.0), sampleRate(rate)
    {
        data.setSize(2, (int)lengthInSamples + 4);
        data.clear();
        for(int i = 0; i < lengthInSamples; ++i)
        {
            const double time = i / rate;
            const float sample = (float)(0.6 * std::sin(juce::MathConstants<double>::twoPi * 1600.0 * time) * std::exp(-time * 160.0) * juce::jmin(1.0, time * 4000.0));
            data.setSample(0, i, sample);
            data.setSample(1, i, sample);
        }
    }

    void ASound::setSound( const juce::String& File, double MaxLengthInSeconds )
    {
        auto inFile = juce::File( File );
        if( ( file != inFile ) || ( lengthInSeconds != MaxLengthInSeconds ) )
        {
            file            = inFile;
            lengthInSeconds = MaxLengthInSeconds;
            reload();
        }
    }

    const juce::AudioBuffer<float>& ASound::getData()
    {
        if( !loaded ) reload();
        return data;
    }

    void ASound::purge()
    {
        if( 0 == getReferenceCount() )
        {
            data.setSize( 2, 1 );
            loaded = false;
        }
    }

    void ASound::reload()
    {
        if( file.existsAsFile() )
            resample( upSampleRate );
    }

    void ASound::resample( double Factor )
    {
        lengthInSamples = 0;
        sampleRate      = 0;
        loaded          = false;

        // Create audio file reader
        std::unique_ptr<juce::AudioFormatReader> reader{ aFmtMgr.createReaderFor( file ) };
        if( !reader.get() )
            return; // Return if we are not able to create reader

        constexpr int defaultNumChannels = 2;
        constexpr int paddingSamples     = 4;

        // Determine number of channels and sample rate.
        const auto channels = std::min<int>( defaultNumChannels, static_cast<unsigned int>( reader->numChannels ) );
        sampleRate          = reader->sampleRate;

        // Calculate the number of samples.
        const auto   numSamplesInFile    = reader->lengthInSamples;
        const double fileLengthInSeconds = static_cast<double>( numSamplesInFile ) / sampleRate;

        if( ( lengthInSeconds < 0 ) || ( lengthInSeconds > fileLengthInSeconds ) )
            lengthInSeconds = fileLengthInSeconds;

        const auto numInputSamples  = std::min( static_cast<juce::int64>( reader->lengthInSamples ), static_cast<juce::int64>( lengthInSeconds * sampleRate ) );
        const auto numOutputSamples = static_cast<juce::int64>( Factor * numInputSamples );

        // Allocate space to hold file samples.
        data.setSize( defaultNumChannels, numOutputSamples + paddingSamples, false, true, true );

        // Update length and sample rate to reflect resampler changes.
        lengthInSamples = numOutputSamples;
        sampleRate *= Factor;

        if( juce::approximatelyEqual( 1.0, Factor ) )
        { // Read the file contents.
            reader->read( &data, 0, numInputSamples, 0, true, channels > 1 );
        }
        else
        { // Read the file contents.
            juce::AudioBuffer<float> tmpBuffer( channels, ( numInputSamples + paddingSamples ) );
            reader->read( &tmpBuffer, 0, numInputSamples, 0, true, channels > 1 );

            const auto interpolatorFactor = ( 1.0 / Factor );
            for( int outChan = 0; outChan < defaultNumChannels; ++outChan )
            {
                int inChan = ( channels > 1 ) ? outChan : 0;

                interpolator.reset();
                interpolator.process(
                    interpolatorFactor,
                    tmpBuffer.getReadPointer( inChan ),
                    data.getWritePointer( outChan ),
                    numOutputSamples,
                    numInputSamples,
                    0 );
            }
        }

        loaded = true;
    }

    ///
    /// ASoundSlice
    ///

    ASoundSlice::ASoundSlice( const juce::ValueTree& Tree, juce::UndoManager* Undo )
    {
        auto centsCalculator = [&]() {
            cents.store( 100 * semi.get() + fine.get() );
            retune();
        };

        auto gainCalculator = [&]() {
            AudIO::PannerFloat panner( pan.get(), gain.get() );
            gL.store( panner.getLeft() );
            gR.store( panner.getRight() );
        };

        auto audioSource = [&]() {
            auto srcFile = sourceFile.load();

            setSource( getAudioSoundCache().getOrLoad( srcFile ), source != nullptr );
        };

        semi.onValueChanged       = centsCalculator;
        fine.onValueChanged       = centsCalculator;
        gain.onValueChanged       = gainCalculator;
        pan.onValueChanged        = gainCalculator;
        sourceFile.onValueChanged = audioSource;

        attachTo( Tree, Undo );

        // Manually trigger to update.
        semi.onValueChanged();
        fine.onValueChanged();
        sourceFile.onValueChanged();
    }

    // Tuning attachments...
    void ASoundSlice::attachTo( const juce::ValueTree& Tree, juce::UndoManager* Undo )
    {
        vTree = Tree;
        undo  = Undo;

        if( vTree.getProperty( IDs::id ).toString().isEmpty() )
            vTree.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        isMuted.attachTo( vTree, IDs::mute, Undo, false );
        isSolo.attachTo( vTree, IDs::solo, Undo, false );
        isReversed.attachTo( vTree, IDs::reverse, Undo, false );
        isLooping.attachTo( vTree, IDs::loop, Undo, false );
        isCrossFade.attachTo( vTree, IDs::fade, Undo, false );
        isWarp.attachTo( vTree, IDs::warp, Undo, false );
        loopStart.attachTo( vTree, IDs::loopStart, Undo, 0 );
        loopEnd.attachTo( vTree, IDs::loopEnd, Undo, INT_MAX );
        semi.attachTo( vTree, IDs::semi, Undo, 0 );
        fine.attachTo( vTree, IDs::fine, Undo, 0 );
        gain.attachTo( vTree, IDs::gain, Undo, 1.f );
        pan.attachTo( vTree, IDs::pan, Undo, 0.f );
        sourceFile.attachTo( vTree, IDs::source, Undo );
    }

    void ASoundSlice::removeAttachment()
    {
        undo  = nullptr;
        vTree = EMPTY::VTState;
    }

    void ASoundSlice::setSource( ASound::Ptr Source, bool ResetLoopPoints )
    {
        ASound::Ptr retired;
        {
            const juce::SpinLock::ScopedLockType guard( sourceLock );
            if( source == Source ) return;
            retired = source; // Release the previous owner outside the snapshot lock.
            source = Source;
            sourceRevision.fetch_add( 1, std::memory_order_release );
        }
        // Binding a saved/copied slice preserves its trim. Only an explicit
        // source replacement resets the region.
        if( ResetLoopPoints )
        {
            loopStart = 0;
            loopEnd   = INT_MAX;
        }
        refresh();
    }

    void ASoundSlice::refresh() noexcept
    {
        if( source.get() )
        {
            setLoopPoints( loopStart(), loopEnd() );
        }

        const auto sourceSampleRate = source.get() ? source->getSampleRate() : AUDIO_SAMPLE_RATE;
        resamplingFactor            = sourceSampleRate / resamplingRate;
        retune();
    }

    const juce::AudioBuffer<float>& ASoundSlice::getSourceBuffer()
    {
        return source ? source->getData() : EMPTY::SampleBuffer;
    }

    juce::String ASoundSlice::getId()
    {
        return vTree.getProperty( IDs::id ).toString();
    }

    //int ASoundSlice::getEnd() const noexcept
    //{
    //     auto lEnd = loopEnd();
    //    if( source )
    //    {
    //        return lEnd > sourceLength ? sourceLength : lEnd;
    //    }
    //    else
    //    {
    //        return ( INT_MAX == lEnd ) ? ( loopStart() + 1 ) : lEnd;
    //    }
    //}

    void ASoundSlice::setLoopPoints( int Start, int End ) noexcept
    {
        const auto sourceLength = getSourceLength();
        const int end           = juce::jlimit( 0, (int)sourceLength, End );
        loopStart               = juce::jlimit( 0, end, Start );
        loopEnd                 = end;
    }

    void ASoundSlice::setStart( int Start ) noexcept
    {
        auto end   = loopEnd();
        auto start = juce::jlimit( 0, juce::jmax( 0, end ), Start );
        loopStart  = start;
    }

    void ASoundSlice::setEnd( int End ) noexcept
    {
        const auto sourceLength = getSourceLength();
        auto       start        = loopStart();
        auto       end          = juce::jmin<int>( source ? sourceLength : INT_MAX, End );
        loopEnd                 = juce::jmax( start, end );
    }

    void ASoundSlice::retune()
    {
        // Calculate rates...
        auto tf = AudIO::utils::calculatePitchFactor( semi(), fine() );
        // Recompute effective tune rate.
        effRate = resamplingFactor.load() * tf;
    }

    void ASoundSlice::reloop()
    {
        const auto sourceLength = getSourceLength();
        auto       srcSamples   = source ? (int)sourceLength : INT_MAX;
        auto       start        = loopStart();
        auto       end          = juce::jmin( srcSamples, loopEnd() );
        loopStart.store( juce::jmax( 0, juce::jmin( start, ( end - 1 ) ) ) );
        loopEnd.store( juce::jmax( (int)loopStart(), end ) );
    }

    void ASoundSlice::setReSampleRate( double SampleRate ) noexcept
    {
        resamplingRate   = SampleRate;
        resamplingFactor = source ? ( ( source->getSampleRate() / resamplingRate ) ) : 1.0;
        retune();
    }

} // namespace mt
