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

    class ASound : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<ASound>;

        explicit ASound( const juce::String& File, juce::AudioFormatManager& FmtMgr, float UpSampleRate, double MaxLengthInSeconds = -1.0 );
        ASound(juce::AudioFormatManager& FmtMgr, double SampleRate); // generated fallback click

        void setSound( const juce::String& File, double MaxLengthInSeconds );

        const juce::AudioBuffer<float>& getData();
        void                            purge();

        ///
        /// Inline functions.
        ///
        inline bool                isValid() const noexcept { return lengthInSamples != 0; }
        inline juce::File          getFile() const { return file; };
        inline juce::String        getName() const { return file.getFileNameWithoutExtension(); };
        inline juce::String        getFileName() const { return file.getFileName(); };
        inline const juce::String& getFilePath() const { return file.getFullPathName(); };
        inline int                 getNumChannels() const { return 2; };
        inline int                 getNumSamplesPerChannel() const { return lengthInSamples; };
        inline int                 getLength() const { return lengthInSamples; };
        inline double              getLengthInSeconds() const { return lengthInSeconds; };
        inline double              getSampleRate() const { return sampleRate; };

    private:
        void reload();
        void resample( double Factor );

    private:
        juce::AudioFormatManager&  aFmtMgr;
        juce::File                 file;
        juce::LagrangeInterpolator interpolator;
        juce::AudioBuffer<float>   data;

        bool        loaded{ false };
        juce::int64 lengthInSamples{ 0 };
        double      lengthInSeconds{ -1.0 };
        double      upSampleRate;
        double      sampleRate{ 0.0 };
    };

    class ASoundSlice
    {
    public:
        ASoundSlice( const juce::ValueTree& Tree = juce::ValueTree{ IDs::SLICE }, juce::UndoManager* Undo = nullptr );
        ~ASoundSlice() = default;

        Atomic<bool>  isMuted;
        Atomic<bool>  isSolo;
        Atomic<bool>  isReversed;
        Atomic<bool>  isLooping;
        Atomic<bool>  isCrossFade;
        Atomic<bool>  isWarp;
        Atomic<int>   loopStart;
        Atomic<int>   loopEnd;
        Atomic<int>   semi;
        Atomic<int>   fine;
        Atomic<float> gain;
        Atomic<float> pan;
        AtomicString  sourceFile;

        std::atomic<int>    cents{ 0 }; // Don't EDIT directly
        std::atomic<double> gL{ 0 }; // Don't EDIT directly
        std::atomic<double> gR{ 0 }; // Don't EDIT directly
        ASound::Ptr         source; // Don't EDIT directly

        // Getters.
        juce::String           getId();
        inline juce::ValueTree getState() noexcept { return vTree; }

        const juce::AudioBuffer<float>& getSourceBuffer();
        inline double                   getReSampleRate() const noexcept { return resamplingRate; };
        inline double                   getSourceSampleRate() const noexcept { return source ? source->getSampleRate() : AUDIO_SAMPLE_RATE; };
        inline double                   getSourceLength() const noexcept { return source ? source->getNumSamplesPerChannel() : 0; };
        inline double                   getSliceLength() const noexcept { return source ? ( ( loopEnd() - loopStart() ) / source->getSampleRate() ) : 0; };

        inline bool isForward() const noexcept { return !isReversed(); }

        inline juce::Range<int> getLoopPoints() const noexcept { return juce::Range<int>( loopStart.load(), loopEnd.load() ); }
        inline double           getEffectiveResamplerRate() const noexcept { return effRate.load(); }
        inline double           getLeft() const noexcept { return gL.load(); }
        inline double           getRight() const noexcept { return gR.load(); }

        inline int getStart() const noexcept { return loopStart(); }
        inline int getEnd() const noexcept { return loopEnd(); };

        // Setters.
        inline void setMute( bool Value ) noexcept { isMuted = Value; }
        inline void setSolo( bool Value ) noexcept { isSolo = Value; }
        inline void setDirection( bool IsForward ) noexcept { isReversed = !IsForward; }
        inline void setForward( bool Value ) noexcept { isReversed = !Value; }
        inline void setReverse( bool Value ) noexcept { isReversed = Value; }
        inline void setLooping( bool Value ) noexcept { isLooping = Value; }
        inline void setWarp( bool Value ) noexcept { isWarp = Value; }
        inline void setFade( bool Value ) noexcept { isCrossFade = Value; }
        inline void setSemi( int Value ) noexcept { semi = juce::jlimit( SEMI::MIN, SEMI::MAX, Value ); }
        inline void setFine( int Value ) noexcept { fine = juce::jlimit( FINE::MIN, FINE::MAX, Value ); }
        inline void setGain( float Value ) noexcept { gain = juce::jlimit( 0.f, 1.f, Value ); }
        inline void setPan( float Value ) noexcept { pan = juce::jlimit( -1.f, 1.f, Value ); }

        void setLoopPoints( int Start, int End ) noexcept;
        void setStart( int Start ) noexcept;
        void setEnd( int End ) noexcept;
        void resetLoopPoints() noexcept { setLoopPoints( 0, INT_MAX ); }
        void setTuneFactor( int Semi, int Fine ) noexcept
        {
            setSemi( Semi );
            setFine( Fine );
        }

        void setSource( ASound::Ptr Source, bool ResetLoopPoints = false );
        ASound::Ptr getPlaybackSource( uint64_t& Revision ) const noexcept
        {
            const juce::SpinLock::ScopedLockType guard( sourceLock );
            Revision = sourceRevision.load( std::memory_order_relaxed );
            return source;
        }
        uint64_t getSourceRevision() const noexcept { return sourceRevision.load( std::memory_order_acquire ); }
        void setReSampleRate( double SampleRate ) noexcept;

        void refresh() noexcept;

    private:
        void retune();
        void reloop();

        void attachTo( const juce::ValueTree& Tree, juce::UndoManager* Undo );
        void removeAttachment();

    private:
        juce::ValueTree    vTree;
        juce::UndoManager* undo{ nullptr };

        std::atomic<double> effRate{ 1.0 };
        mutable juce::SpinLock sourceLock;
        std::atomic<uint64_t> sourceRevision{ 0 };
        std::atomic<double> resamplingFactor{ 1.0 };
        double              resamplingRate{ AUDIO_SAMPLE_RATE };
    };

} // namespace mt
