#include "kick_ui/recent_project/kick_RecentProject_Utils.h"
namespace ui
{
    /////////////////////////////////////////////////////////////////
    ////////   LoadProjectAndAnimation   ////////////////////////////
    /////////////////////////////////////////////////////////////////

    LoadProjectAndAnimation::LoadProjectAndAnimation()
        : juce::Thread( "LoadProject" )
    {
        ae.addListener( this );
    }

    LoadProjectAndAnimation::~LoadProjectAndAnimation()
    {
        stopThread(-1);
        ae.removeListener( this );
    }

    void LoadProjectAndAnimation::startLoading( juce::File project )
    {
        if(isThreadRunning()) return;
        createNew = false;
        ui::RecentProject::isProjectLoading = true;
        proj                                = project;
        startThread();
        startAnimation();
    }

    void LoadProjectAndAnimation::startCreating(juce::File project)
    {
        if(isThreadRunning()) return;
        createNew = true; proj = project; ui::RecentProject::isProjectLoading = true;
        startThread(); startAnimation();
    }

    void LoadProjectAndAnimation::onProjectLoaded( juce::File file )
    {
        juce::MessageManager::callAsync([this] {
            if(isAnimationRunning()) stopAnimation();
            kick_ui::signals::sigScreenOpen(kick_ui::Screen::HomeListView);
            ui::RecentProject::isProjectLoading = false;
        });
    }

    void LoadProjectAndAnimation::run()
    {
        const bool loaded = createNew ? ae.createProject(proj) : ae.loadProject(proj);
        if(!loaded)
        {
            const auto error = createNew ? ae.getLastProjectError() : juce::String("Unable to load this project; check its location and contents");
            juce::MessageManager::callAsync([this, error] {
                if(isAnimationRunning()) stopAnimation(); ui::RecentProject::isProjectLoading = false;
                mt::utils::showNotification(error);
            });
        }
    }
}
