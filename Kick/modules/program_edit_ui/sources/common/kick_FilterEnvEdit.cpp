#include "kick_FilterEnvEdit.h"

namespace kick_ui
{
    FilterEnvEdit::FilterEnvEdit( std::vector<int>& Pads )
        : pads( Pads ), KickGrid( 4, 1 )
    {
        addComponent( filterLabel, 0, 0, 215 );
        addComponent( msLabel, 1, 0, 215 );
        addComponent( feOptions, 2, 0, 215 );
        addComponent( ampEnvOptions, 3, 0, 215 );

        addComponent( filterType, 0, 0, 130 );
        addComponent( filterCutoff, 0, 0, 130 );
        addComponent( filterReso, 0, 0, 130 );
        addComponent( filterEnv, 0, 0, 130 );

        addComponent( msVelStat, 1, 0, 130 );
        addComponent( msVelAtk, 1, 0, 130 );
        addComponent( msVelEnv, 1, 0, 130 );
        addComponent( msVelFlt, 1, 0, 130 );

        addComponent( feAttack, 2, 0, 130 );
        addComponent( feHold, 2, 0, 130 );
        addComponent( feDecay, 2, 0, 130 );
        addComponent( feSustain, 2, 0, 130 );
        addComponent( feAhdsrBtn, 2, 0, 130 );

        addComponent( ampAttack, 3, 0, 130 );
        addComponent( ampHold, 3, 0, 130 );
        addComponent( ampDecay, 3, 0, 130 );
        addComponent( ampSustain, 3, 0, 130 );
        addComponent( ampAhdsrBtn, 3, 0, 130 );

        ampAhdsrBtn.addListener( this );
        feAhdsrBtn.addListener( this );

        ui::connectTheme( *this, [&]() {
            feAhdsrBtn.setImages( "effect3" );
            ampAhdsrBtn.setImages( "effect3" );
        } );
    }

    FilterEnvEdit::~FilterEnvEdit()
    {
    }

    void FilterEnvEdit::buttonClicked( juce::Button* button )
    {
        KickEnvelopViewBase* dialog = nullptr;
        std::vector<int>     values;
        if( button == &feAhdsrBtn )
        {
            values.resize( 4 );
            values[0] = feAttack.getValue();
            values[1] = feHold.getValue();
            values[2] = feDecay.getValue();
            values[3] = feSustain.getValue();

            KickAHDSView* p = new KickAHDSView( values );
            dialog          = p;
        }
        else if( button == &ampAhdsrBtn )
        {
            values.resize( 4 );
            values[0] = ampAttack.getValue();
            values[1] = ampHold.getValue();
            values[2] = ampDecay.getValue();
            values[3] = ampSustain.getValue();

            KickAHDSView* p = new KickAHDSView( values );
            dialog          = p;
        }

        mt::utils::showDialog(
            "FILTER ENVELOPE", dialog, [this, dialog, button]( int /*r*/ ) {
                if( 1 /*r != UI::BUTTONS::CANCEL*/ )
                {
                    const std::vector<int>& initValues = dialog->getAHDSValues();
                    if( button == &feAhdsrBtn )
                    {
                        feAttack.setValue( initValues[0] );
                        feHold.setValue( initValues[1] );
                        feDecay.setValue( initValues[2] );
                        feSustain.setValue( initValues[3] );
                    }
                    else if( button == &ampAhdsrBtn )
                    {
                        ampAttack.setValue( initValues[0] );
                        ampHold.setValue( initValues[1] );
                        ampDecay.setValue( initValues[2] );
                        ampSustain.setValue( initValues[3] );
                    }
                }
                delete dialog;
            },
            dialog->getLocalBounds() );
    }

    void FilterEnvEdit::refresh()
    {
    }

    KickFilterEnvelope::KickFilterEnvelope(std::vector<int>& keys, mt::MTKeygroupProgram*& program)
        : SampleVoiceEditor(keys, [&program] { return program; }) {}
}
