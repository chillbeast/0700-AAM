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

AppTrack::AppTrack()
    : kick_ui::IRowView( "Track View" )
{
    setName( "AppTrack" );
    setupProvider();
    setMargin( 0, 6, 2, 0 );

    mt::signals::midiInputsChanged.connect( [&]() {
        auto d = ae.getNumMIDIDevices( true );
        if( _numMidiPorts != d )
        {
            // Device list has changed; refresh MIDI device list...
            _numMidiPorts = d;
            // Refresh.
            populate();
        }
    } );
}

AppTrack::~AppTrack()
{
    ae.removeListener( this );
    mt::signals::midiInputsChanged.disconnect( this );
}

void AppTrack::setupProvider()
{
    provider.numRows = [&]() -> int {
        return ae.getNumTracks( ae.getSelectedSequence() );
    };

    provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
        auto seqNo     = ae.getSelectedSequence();
        auto numTracks = ae.getNumTracks( seqNo );
        if( juce::isPositiveAndBelow( Row, numTracks ) )
        {
            auto tData = ae.getTrackData( seqNo, Row );
            auto comp  = dynamic_cast<AppTrackComponent*>( Existing );
            if( comp == nullptr )
            {
                comp = new AppTrackComponent( seqNo, Row, tData ? tData->name.get() : "" );

                comp->onTrackEdit = [&]() {
                    if( onTrackEdit )
                        onTrackEdit();
                };
            }
            comp->onSoloMuteChange = [&]() {
                refreshSoloMute();
            };
            comp->setTrack( seqNo, Row );
            comp->setMidiPorts( &_midiInPorts );
            mt::utils::addChildComponents( this, comp );
            return comp;
        }
        return nullptr;
    };

    provider.onRowSelected = [&]( int Row ) {
        ae.setSelectedTrack( Row );
    };

    _initialized = true;
}

void AppTrack::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
{
    DBG( "APPTRACK handleCommand" );
    if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
    {
        auto knobID    = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
        auto knobValue = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
        if( knobID != contracts::DeviceControlContracts::EN_KNOB_CENTER )
            knobCommand( knobID, knobValue );
        else if( contracts::DeviceControlContracts::EN_KNOB_CENTER == knobID )
            IRowView::knobRotated( knobValue );
    }
}

void AppTrack::knobCommand( int knob, int value )
{
    auto seqNo     = ae.getSelectedSequence();
    auto numTracks = ae.getNumTracks( seqNo );
    if( 0 < numTracks )
    {
        auto&                           rowComp  = getRowComponents();
        auto                            trackNum = getRowNumbers( ( numTracks >= 4 ) ? 4 : numTracks, ( ( knob - 1 ) / 4 ) + 1, 0, 0, 50 );
        std::vector<AppTrackComponent*> trackComp;
        for( auto i = 0; i < trackNum.size(); i++ )
        {
            auto comp = dynamic_cast<AppTrackComponent*>( rowComp[trackNum[i]] );
            trackComp.push_back( comp );
        }

        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
            case contracts::DeviceControlContracts::EN_KNOB_2:
            case contracts::DeviceControlContracts::EN_KNOB_3:
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( trackComp.size() >= 1 && trackComp[0] ) trackComp[0]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_1 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_5:
            case contracts::DeviceControlContracts::EN_KNOB_6:
            case contracts::DeviceControlContracts::EN_KNOB_7:
            case contracts::DeviceControlContracts::EN_KNOB_8:
                if( trackComp.size() >= 2 && trackComp[1] ) trackComp[1]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_5 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_9:
            case contracts::DeviceControlContracts::EN_KNOB_10:
            case contracts::DeviceControlContracts::EN_KNOB_11:
            case contracts::DeviceControlContracts::EN_KNOB_12:
                if( trackComp.size() >= 3 && trackComp[2] ) trackComp[2]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_9 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_13:
            case contracts::DeviceControlContracts::EN_KNOB_14:
            case contracts::DeviceControlContracts::EN_KNOB_15:
            case contracts::DeviceControlContracts::EN_KNOB_16:
                if( trackComp.size() >= 4 && trackComp[3] ) trackComp[3]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_13 ), value );
                break;
        }
    }
}

void AppTrack::populate( void )
{
    if( !isVisible() || !_initialized.load() )
        return;

    if( ae.isProjectLoaded() )
        refresh();
    return;
}

//==============================================================================

void AppTrack::onProjectLoaded( juce::File )
{
    populate();
}

void AppTrack::onProjectClose()
{
    forEachRowComponent([](juce::Component* row) {
        if(auto* component = dynamic_cast<AppTrackComponent*>(row)) component->setTrack(-1, -1);
    });
}

void AppTrack::onTrackSelectionChange( const mt::MTSelected& Selected )
{
    if( ae.isProjectLoaded() )
        juce::MessageManager::callAsync( [&]() {
            selectRow( aeSel.track );
        } );
}

void AppTrack::onTrackListChanged( int /*Sequence*/ )
{
    populate();
    if( isVisible() )
        refreshSoloMute();
}

void AppTrack::onTrackMetersUpdated()
{
    if(!ae.isProjectLoaded()) return;
    kick_ui::IRowView::forEachRowComponent( [&]( juce::Component* Comp ) {
        if( auto trkDisplay = dynamic_cast<AppTrackComponent*>( Comp ) )
            trkDisplay->updateMeter();
    } );
}

void AppTrack::refreshSoloMute()
{
    auto& rowComps = getRowComponents();

    for( auto rowComp : rowComps )
    {
        auto comp = dynamic_cast<AppTrackComponent*>( rowComp );
        if(comp) comp->refreshSoloMute();
    }
}

void AppTrack::timerCallback()
{
    auto d = ae.getNumMIDIDevices( true );
    if( _numMidiPorts != d )
    {
        // Device list has changed; refresh MIDI device list...
        _numMidiPorts = d;
        // Refresh.
        populate();
    }
}

void AppTrack::visibilityChanged()
{
    auto visible = isVisible();
    IRowView::setVisible( visible );

    if( isVisible() )
    {
        ae.addListener( this );
        populate();
        refreshSoloMute();
    }
    else
    {
        ae.removeListener( this );
        contracts::DclSignals::sendCommandToDcl(
            contracts::DclContractsHelper::sendSliderValueInPercentMessage( 0 ) );
    }
}
