#pragma once

#include "Chain.h"

#include <algorithm>

#if defined( DEBUG_PRINTS ) && ( DEBUG_PRINTS > 0 )
    #include <iostream>
#endif

namespace AudIO
{
    Chain::Chain( bool stereo, uint32_t capacity )
        : Object( stereo ? 2 : 1, stereo ? 2 : 1, 0 )
    {
        _objects.reserve( capacity );
    }

    Chain::~Chain()
    {
        _inserts.clear();
        _objects.clear();
    }

    void Chain::initializeChain( objectMeta* meta )
    {
        _chainmeta = meta;
    }

    bool Chain::changeCapacity( uint32_t newcapacity )
    {
        newcapacity;
        return false;
    }

    uint32_t Chain::createInsert( const char* type )
    {
        auto obj = CREATE_OBJECT( type );
        return createInsert( std::move( obj ) );
    }

    uint32_t Chain::createInsert( std::unique_ptr<Object> obj )
    {
        auto id = 0;
        if( obj && !existsInObjects( obj->getId() ) )
        {
            auto& pObj = *obj;
            _objects.emplace_hint( _objects.end(), pObj.getId(), std::move( obj ) );
            pObj.prepareToPlay( sampleRate(), blockSize() );
            id = pObj.getId();

            // Debug print.
#if defined( DEBUG_PRINTS ) && ( DEBUG_PRINTS > 0 )
            std::cout << pObj.getType() << " (" << pObj.getId() << ")" << std::endl;
#endif
        }
        return id;
    }

    Object* Chain::getObject( uint32_t id )
    {
        if( const auto it = _objects.find( id ); it != _objects.end() )
        {
            return it->second.get();
        }
        return nullptr;
    }

    bool Chain::insertObject( uint32_t id, uint32_t pos )
    {
        if( existsInObjects( id ) && !existsInInserts( id ) )
        {
            auto&                           obj = *_objects.at( id ).get();
            std::list<insertMeta>::iterator it;
            if( _inserts.empty() )
            {
                _inserts.emplace_front( obj );
                it = _inserts.begin();
            }
            else
            {
                it = _inserts.begin();
                if( pos > _inserts.size() )
                    it = std::prev( _inserts.end() );
                else
                    it = std::next( _inserts.begin(), pos );

                it = _inserts.emplace( it, obj );
            }
            if( it == _inserts.begin() )
            {
                it++;
                if( it != _inserts.end() )
                {
                    _inserts.front().patch( ( *it ).getBuffer() );
                }
                else
                {
                    patchLastOutputToChainOutput();
                }
                patchChainInputToFirstInput();
            }
            else if( it == _inserts.end()-- )
            {
                it--;
                ( *it ).patch( _inserts.back().getBuffer() );
                patchLastOutputToChainOutput();
            }
            else
            {
                auto& prev = *std::prev( it );
                auto& cur  = *it;
                auto& next = *std::next( it );
                prev.patch( cur.getBuffer() );
                cur.patch( next.getBuffer() );
            }
            return true;
        }
        return false;
    }

    bool Chain::removeInsertById( uint32_t id )
    {
        auto found = [id]( insertMeta& meta ) { return meta.getObject().getId() == id; };
        auto it    = std::find_if( _inserts.begin(), _inserts.end(), found );
        return removeInsertByIterator( it );
    }

    bool Chain::removeInsertByPos( uint32_t pos )
    {
        auto it = _inserts.begin();
        if( pos > _inserts.size() )
            it = std::prev( _inserts.end() );
        else
            it = std::next( _inserts.begin(), pos );
        return removeInsertByIterator( it );
    }

    bool Chain::deleteInsert( uint32_t id )
    {
        if( existsInObjects( id ) )
        {
            removeInsertById( id );
            _objects.erase( id );
            return true;
        }
        return false;
    }

    bool Chain::removeInsertByIterator( std::list<insertMeta>::iterator it )
    {
        if( it != _inserts.end() )
        {
            if( it == _inserts.begin() )
            {
                _inserts.erase( it );
                patchChainInputToFirstInput();
            }
            else if( it == _inserts.end()-- )
            {
                _inserts.erase( it );
                patchLastOutputToChainOutput();
            }
            else
            {
                auto next = _inserts.erase( it );
                auto prev = next--;
                ( *prev ).patch( ( *next ).getBuffer() );
            }
            return true;
        }
        return false;
    }

    void Chain::setParameter( int index, float value )
    {
        switch( index )
        {
            case BusOrChain::kParamBypass:
                m_bypass = ( value > 0.5f ) ? true : false;
                m_bypassGain.store( ( m_bypass ? 0.0f : 1.0f ) );
                break;
            case BusOrChain::kParamMute:
                m_mute = ( value > 0.5f ) ? true : false;
                m_muteGain.store( ( m_mute ? 0.0f : 1.0f ) );
                break;
            case BusOrChain::kParamGain:
                m_gain = value;
                break;
            case BusOrChain::kParamPan:
                m_pan = value;
                break;
            case BusOrChain::kParamStereo:
                m_downmixStereoToMono = ( value > 0.5f ) ? false : true;
                break;
            case BusOrChain::kParamLimiter:
                m_limiterEnabled.store(value > 0.5f);
                break;
            case BusOrChain::kParamDownMixToLeft:
                m_downmixStereoToMono = ( value > 0.5f ) ? true : false;
                m_downmixToLeft       = ( value > 0.5f ) ? true : false;
                break;
            case BusOrChain::kParamDownMixToRight:
                m_downmixStereoToMono = ( value > 0.5f ) ? true : false;
                m_downmixToLeft       = ( value > 0.5f ) ? false : true;
                break;
        }
    }

    float Chain::getParameter( int index )
    {
        switch( index )
        {
            case BusOrChain::kParamBypass:
                return m_bypass ? 1.0f : 0.0f;
            case BusOrChain::kParamMute:
                return m_mute ? 1.0f : 0.0f;
            case BusOrChain::kParamGain:
                return m_gain.value();
            case BusOrChain::kParamPan:
                return m_pan.value();
            case BusOrChain::kParamStereo:
                return m_downmixStereoToMono ? 0.f : 1.f;
            case BusOrChain::kParamLimiter:
                return m_limiterEnabled.load() ? 1.f : 0.f;
            case BusOrChain::kParamDownMixToLeft:
                return m_downmixStereoToMono ? ( m_downmixToLeft ? 1.f : 0.f ) : 0.f;
            case BusOrChain::kParamDownMixToRight:
                return m_downmixStereoToMono ? ( m_downmixToLeft ? 0.f : 1.f ) : 0.f;
        }
        return 0.0f;
    }

    std::pair<float, float> Chain::getLevelAndPan() const
    {
        return std::make_pair<float, float>( m_gain.value(), m_pan.value() );
    }

    void Chain::setLevelAndPan( float Gain, float Pan )
    {
        m_gain = Gain;
        m_pan  = Pan;
    }

    void Chain::prepareToPlay( uint32_t samplerate, uint32_t blocksize )
    {
        Object::prepareToPlay( samplerate, blocksize );
        m_limiterGain = 1.0f;
        m_limiterRelease = 1.0f - std::exp(-1.0f / (0.1f * std::max(1u, samplerate)));
        // Do it for all objects
        for( auto& obj : _objects )
            obj.second->prepareToPlay( samplerate, blocksize );
    }

    void Chain::process( Buffer<float>& Buffer )
    {
        float bypassGain = m_bypassGain.getNext( Object::blockSize() );

        if( inMeterEnabled.load() )
            metering.processIn( Buffer );

        if( utils::isEqual<float>( 0.f, bypassGain ) )
        {
            Buffer.copyInputsToOutputs();
        }
        else
        {
            float muteGain = m_muteGain.getNext( Object::blockSize() );
            float mGain    = m_gain.getNext( Object::blockSize() );
            float mPan     = m_pan.getNext( Object::blockSize() );

            auto chainGain = muteGain * mGain * bypassGain;
            if( utils::isEqual<float>( 0.f, chainGain ) )
            {
                Buffer.clearOutputs();
            }
            else // !Muted
            {
                if( _inserts.size() )
                {
                    for( auto& insert : _inserts )
                        insert.process();
                }
                else
                {
                    Buffer.copyInputsToOutputs();
                }

                if( m_downmixStereoToMono )
                {
                    Buffer.stereoToMono( chainGain, m_downmixToLeft );
                }
                else
                {
                    Buffer.applyGainAndPan( chainGain, mPan );
                }
            } // !Muted
        } // !Bypass

        // Stereo-linked, zero-latency sample-peak limiter, after inserts/gain.
        // Instant attack, 100 ms release, fixed 0.98 ceiling; no makeup gain.
        if(m_limiterEnabled.load())
        {
            for(uint32_t sample = 0; sample < Buffer.getBlockSize(); ++sample)
            {
                float peak = 0.0f;
                for(uint32_t channel = 0; channel < Buffer.getNumOutputChannels(); ++channel)
                    peak = std::max(peak, std::abs(Buffer.getOutputBuffer(channel)[sample]));
                const float target = peak > 0.98f ? 0.98f / peak : 1.0f;
                m_limiterGain = std::min(target, m_limiterGain + (1.0f - m_limiterGain) * m_limiterRelease);
                for(uint32_t channel = 0; channel < Buffer.getNumOutputChannels(); ++channel)
                    Buffer.getOutputBuffer(channel)[sample] *= m_limiterGain;
            }
        }
        else m_limiterGain = 1.0f;

        if( outMeterEnabled.load() )
            metering.process( Buffer );
    }

    MeterData& Chain::readInMeter()
    {
        return metering.getMeterIn( inMeter );
    }

    MeterData& Chain::readOutMeter()
    {
        return metering.getMeter( outMeter );
    }

    void Chain::patchChainInputToFirstInput()
    {
        if( _chainmeta && !_inserts.empty() )
        {
            auto& object       = _inserts.front();
            auto& chainBuffer  = _chainmeta->getBuffer();
            auto& objectBuffer = object.getBuffer();
            for( uint32_t ch = 0; ch < ( m_stereo ? 2U : 1U ); ch++ )
            {
                PatchQueue<float>::get().patchInput( &objectBuffer, chainBuffer.getInputBuffer( ch ), ch );
            }
        }
    }

    void Chain::patchLastOutputToChainOutput()
    {
        if( _chainmeta && !_inserts.empty() )
        {
            auto& object       = _inserts.back();
            auto& chainBuffer  = _chainmeta->getBuffer();
            auto& objectBuffer = object.getBuffer();
            for( uint32_t ch = 0; ch < ( m_stereo ? 2U : 1U ); ch++ )
            {
                PatchQueue<float>::get().patchOutput( &objectBuffer, chainBuffer.getOutputBuffer( ch ), ch );
            }
        }
    }
    bool Chain::existsInObjects( uint32_t id )
    {
        return _objects.find( id ) != _objects.end();
    }

    bool Chain::existsInInserts( uint32_t id )
    {
        auto found = [id]( insertMeta& meta ) { return meta.getObject().getId() == id; };
        return std::find_if( _inserts.begin(), _inserts.end(), found ) != _inserts.end();
    }
} // Namespace AudIO
