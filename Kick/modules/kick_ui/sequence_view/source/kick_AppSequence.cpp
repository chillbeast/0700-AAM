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

AppSequence::AppSequence()
    : kick_ui::IRowView( "Sequence View" )
{
    setupProvider();
    setMargin( 0, 6, 2, 0 );

    getSelectedSeqBPM = [&]( int selSeq ) -> int {
        auto& rowComp = getRowComponents();
        if( !rowComp.size() )
        {
            // Handle the case when there are no row components
            return -1;
        }
        for( auto& it : rowComp )
        {
            auto* comp = dynamic_cast<AppSequenceComponent*>( it );
            if( selSeq == comp->getSeqNumber() )
                return comp->getTempoValue();
        }
        return -1;
    };
}

AppSequence::~AppSequence()
{
    ae.removeListener( this );
}

void AppSequence::setupProvider()
{
    provider.numRows = [&]() -> int {
        return ae.getNumSequences();
    };

    provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
        auto numSeqs = ae.getNumSequences();
        if( juce::isPositiveAndBelow( Row, numSeqs ) )
        {
            auto comp = dynamic_cast<AppSequenceComponent*>( Existing );
            if( comp == nullptr )
            {
                auto tData = ae.getSequenceData( Row );
                comp       = new AppSequenceComponent( Row, tData->name.get() );
            }
            //comp->onSeqMasterChanged = [&]() { refresh(); };
            comp->onSeqEdit = [&]() {
                if( onSeqEdit )
                    onSeqEdit();
            };

            comp->setSequence( Row );
            return comp;
        }
        return nullptr;
    };

    provider.onRowSelected = [&]( int Row ) {
        ae.setSelectedSequence( Row );
    };

    _initialized = true;
}

void AppSequence::onSequenceSelectionChange( const mt::MTSelected& Selected )
{
    if( ae.isProjectLoaded() )
    {
        juce::MessageManager::callAsync( [&]() {
            selectRow( aeSel.sequence );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setTempoMessage( getSelectedSeqBPM( ae.getSelectedSequence() ) ) );
        } );
    }
}

void AppSequence::populate( void )
{
    if( !isVisible() || !_initialized.load() )
        return;
    selectRow( aeSel.sequence );
    refresh();
}

void AppSequence::onTransControlChange( juce::String timeSig, juce::String TC )
{
    juce::StringArray timeSigComp;
    juce::String      timeSigNum, timeSigDen;

    timeSigComp.addTokens( timeSig, "/", "" );
    if(timeSigComp.size() != 2) return;
    timeSigNum = timeSigComp[0];
    timeSigDen = timeSigComp[1];

    if(ae.setTimeSignature(ae.getSelectedSequence(), timeSigNum.getIntValue(), timeSigDen.getIntValue()) != 0)
        mt::utils::showNotification("Stop playback before changing the time signature");
}

void AppSequence::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
{
    DBG( "APPSEQUENCE handleCommand" );
    if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
    {
        auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
        auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
        if( knob != contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
            knobCommand( knob, value );
        else if( contracts::DeviceControlContracts::EN_KNOB_CENTER == knob )
            IRowView::knobRotated( value );
    }
}

void AppSequence::knobCommand( int knob, int value )
{
    auto seqNo  = ae.getSelectedSequence();
    auto numSeq = ae.getNumSequences();
    if( 0 < numSeq )
    {
        auto&                              rowComp = getRowComponents();
        auto                               rowNum  = getRowNumbers( ( numSeq >= 4 ) ? 4 : numSeq, ( ( knob - 1 ) / 4 ) + 1, 0, 0, 50 );
        std::vector<AppSequenceComponent*> seqComp;
        for( auto i = 0; i < rowNum.size(); i++ )
        {
            auto comp = dynamic_cast<AppSequenceComponent*>( rowComp[rowNum[i]] );
            seqComp.push_back( comp );
        }

        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
            case contracts::DeviceControlContracts::EN_KNOB_2:
            case contracts::DeviceControlContracts::EN_KNOB_3:
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( seqComp.size() >= 1 && seqComp[0] ) seqComp[0]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_1 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_5:
            case contracts::DeviceControlContracts::EN_KNOB_6:
            case contracts::DeviceControlContracts::EN_KNOB_7:
            case contracts::DeviceControlContracts::EN_KNOB_8:
                if( seqComp.size() >= 2 && seqComp[1] ) seqComp[1]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_5 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_9:
            case contracts::DeviceControlContracts::EN_KNOB_10:
            case contracts::DeviceControlContracts::EN_KNOB_11:
            case contracts::DeviceControlContracts::EN_KNOB_12:
                if( seqComp.size() >= 3 && seqComp[2] ) seqComp[2]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_9 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_13:
            case contracts::DeviceControlContracts::EN_KNOB_14:
            case contracts::DeviceControlContracts::EN_KNOB_15:
            case contracts::DeviceControlContracts::EN_KNOB_16:
                if( seqComp.size() >= 4 && seqComp[3] ) seqComp[3]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_13 ), value );
                break;
        }
    }
}

//==============================================================================

void AppSequence::onSequenceListChanged()
{
    if( ae.isProjectLoaded() )
        populate();
}

void AppSequence::visibilityChanged()
{
    IRowView::visibilityChanged();

    if( isVisible() )
        ae.addListener( this );
    else
        ae.removeListener( this );
}
