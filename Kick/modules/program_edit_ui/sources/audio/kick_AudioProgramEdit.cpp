namespace kick_ui
{
    AudioProgramEdit::AudioProgramEdit()
        : ProgEditInterface( mt::MTProgramType::Audio )
    {
        mt::utils::addAndMakeVisible( this, audioPgmContent );
    }

    void AudioProgramEdit::resized()
    {
        audioPgmContent.setBounds( getLocalBounds() );
    }

    void AudioProgramEdit::visibilityChanged()
    {
        if(isVisible()) pAudio = dynamic_cast<mt::MTAudioProgram*>(ae.getProgram(ae.getSelectedProgram()));
        for( auto comp : getChildren() )
            comp->setVisible( isVisible() );
        if( isVisible() )
            audioPgmContent.refresh();
    }

}
