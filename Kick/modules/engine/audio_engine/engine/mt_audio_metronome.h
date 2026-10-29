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
    class MTMetronome
    {
    public:
        explicit MTMetronome( AudIO::DawEngine& engine, const juce::ValueTree& VT )
            : _engine( engine ), _vt( VT )
        {
            if( auto pMetronome = std::make_unique<Metronome>( engine, _vt ) )
            {
                _metronome = pMetronome.get();
                _engine.createObject( std::move( pMetronome ) );
                _metronome->initAudioRouting();
            }
            assert( _metronome != nullptr );

            countInConnection = signals::onCountIn.connect([this](bool accent) { trigger(accent, true); });
            clickConnection = signals::onMetronome.connect([this](bool accent) { trigger(accent, false); });
            _met.source.onValueChanged = [this]() {
                auto src = _met.source();
                if(_metronome) _metronome->setSource(src);
            };
            _met.source.onValueChanged();
        }

        ~MTMetronome()
        {
            countInConnection.disconnect();
            clickConnection.disconnect();
            _met.source.onValueChanged = nullptr;
            _metronome->noteOffAll();
            _engine.removeObject(_metronome->getId());
            _metronome = nullptr;
        }

        std::function<bool()> shouldPlay;
        void trigger(bool accent, bool force)
        {
            if(_metronome && (force || (_met.enabled() && (!shouldPlay || shouldPlay()))))
                _metronome->noteOn(accent ? 0 : 1, accent ? 127 : 80);
        }

    private:
        juce::ValueTree   _vt{ IDs::METRONOME };
        AudIO::DawEngine& _engine;
        KickMetronome&    _met{ getSettings().metronome };

        Metronome* _metronome{ nullptr };
        sigslot::scoped_connection countInConnection, clickConnection;
    };

} // namespace MT
