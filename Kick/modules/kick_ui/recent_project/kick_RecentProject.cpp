namespace ui
{
    namespace
    {
        bool validateNewProjectName(const juce::String& name)
        {
            const auto root = getAppDirs(MTProjects);
            const bool valid = name.isNotEmpty() && name == name.trim()
                && name != "." && name != ".." && !name.containsAnyOf("/\\:")
                && juce::File::createLegalFileName(name) == name && !root.getChildFile(name).exists();
            if(!valid)
            {
                auto* popup = new ui::FilePopup("Choose a valid project name that does not already exist.", true, false, false);
                mt::utils::showDialog("NEW PROJECT", popup, [popup](int) { delete popup; }, ui::layout::warningPopup);
            }
            return valid;
        }
    }
    bool RecentProject::enableRecentProjVTreeEntry{ true };
    bool RecentProject::isProjectLoading{ false };

    RecentProject::RecentProject()
        : kick_ui::IScreen( kick_ui::Screen::RecentProject ), longPressComp( std::make_unique<LongPressComp>() )
    {
        ae.addListener( this );

        setHasFocusOutline( false );

        mt::utils::addAndMakeVisible( this, header, fileBrowser, newProj, demoList, templateList, projectList, longPressComp.get() );
        longPressComp->setVisible( false );
        addDemos();
        addTemplates();

        header.onBackButtonPressed = [this]( bool ) { closeScreen( kick_ui::Screen::HomeListView ); };
        header.showOptions( false, false );
        header.showAutoLoad( true );
        header.setAutoLoadToggleState( settings.projects.isAutoLoad() );

        longPressComp->onLoadTriggered = [&]( int Index ) {
            if(juce::isPositiveAndBelow(Index, recentItems.size()) && recentItems[Index]->onMouseDown) recentItems[Index]->onMouseDown( Index, juce::File() );
        };

        longPressComp->onRemoveTriggered = [&]( int Index ) {
            auto idxOffset = demoList.getSize() + templateList.getSize();
            auto p         = new FilePopup{ "Do you want to remove from this list", true, true };
            mt::utils::showDialog(
                "WARNING!", p, [&, p, Index, idxOffset]( int R ) {

                            if( R == UI::APPLY )
                            {
                                juce::String currProj    = settings.projects.currentProject();
                                auto         reversedRow = getReversedRow( Index - idxOffset );

                                if( !ae.isProjectLoaded() || settings.projects.getProject( reversedRow ) != currProj )
                                {
                                    removeFileFromVTree( Index - idxOffset );
                                    setNumOptions( getNumOfChild() );
                                    longPressComp->setVisible( false );
                                }
                                else
                                {
                                    auto pp = new FilePopup{ "Current Project is not allowed to Remove", false, false };
                                    mt::utils::showDialog(
                                        "WARNING!", pp, [&, pp]( int R ) { delete pp; }, { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                                }
                            }
                            delete p; }, { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        header.onAutoLoadClick = [&]() {
            auto p = new FilePopup{ "Automatically load last opened project on startup?", false, false };
            mt::utils::showOptions(
                "AUTO LOAD", p, [this, p]( int R ) {
					if( R == UI::APPLY )
					{
						header.setAutoLoadToggleState( true );
						settings.projects.isAutoLoad = ( true );
                        settings.projects.onLaunch = "Last Used";

						header.setAutoLoadToggleState( settings.projects.isAutoLoad() );
					}
                    else
                    {
                        header.setAutoLoadToggleState( false );
                        settings.projects.isAutoLoad = ( false );
                        settings.projects.onLaunch = "Selector";

                        header.setAutoLoadToggleState( settings.projects.isAutoLoad() );
                    }
					delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        fileBrowser.onClick = [&]() {
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuFileBrowser );
        };

        maxNumOfRecentProject = settings.projects.maxRecentProjects();

        ui::connectTheme( *this, [&]() {
            fileBrowser.setImages( "Browser_Recent" );
            header.setIcon( "RecentProject" );
        } );

        onKeyBoardEnd = [&]( juce::String text, juce::File project ) {
            copyProjIntoProjectDir( text, project );
        };

        newProj.onClick = [&]() {
            juce::String str = settings.projects.namePrefix();
            if(settings.projects.namingStyle().equalsIgnoreCase("TIME STAMP")) str = juce::Time::getCurrentTime().formatted("%d%b%y_%H%M%S_").toUpperCase();

            auto& keyboard = ui::KickVirtualKeyBoard::getWithText( str );
            keyboard.show(
                "NEW PROJECT", ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
                [&keyboard, this]( int R ) {
                    if( UI::BUTTONS::APPLY == R )
                    {
                        createProject( keyboard.getInputText() );
                    }
                },
                ui::layout::keyBoardPopup );
        };
    }

    void RecentProject::createProject( juce::String value )
    {
        if(!validateNewProjectName(value)) return;
        auto nChild = settings.projects.getNumChildren();
        for( auto i = 0; i < nChild; i++ )
        {
            juce::String childProperty = settings.projects.getProject( i );
            juce::File   childFile     = juce::File( childProperty );
            if( value == childFile.getFileName() )
            {
                auto p = new FilePopup( "A project with the same name already exists ", false, false );
                mt::utils::showDialog(
                    "WARNING!", p, [&, p]( int R ) {
                        delete p;
                    },
                    ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                return;
            }
        }

        auto       sepratorStr = juce::File::getSeparatorString();
        juce::File file        = getAppDirs( MTProjects ).getFullPathName() + sepratorStr + value;
        settings.projects.createProject( file );
        ui::LoadProjectAndAnimation::getInstance().startCreating(file);
    }

    void RecentProject::sliderMoved( float )
    {}

    void RecentProject::onProjectLoaded( juce::File file )
    {
        if(!juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            juce::Component::SafePointer<RecentProject> safe(this);
            juce::MessageManager::callAsync([safe, file] { if(safe) safe->onProjectLoaded(file); });
            return;
        }
        if( enableRecentProjVTreeEntry )
            settings.projects.rearrangeProject( file );
        else
            enableRecentProjVTreeEntry = !enableRecentProjVTreeEntry;
    }

    void RecentProject::openVirtualKeyBoard( juce::File project )
    {
        auto& keyboard = ui::KickVirtualKeyBoard::getWithText( project.getFileNameWithoutExtension() );
        keyboard.show(
            "", ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
            [&keyboard, this, project]( int R ) {
                if( UI::BUTTONS::APPLY == R )
                {
                    if( onKeyBoardEnd )
                        onKeyBoardEnd( keyboard.getInputText(), project );
                }
            },
            ui::layout::keyBoardPopup );
    }

    void RecentProject::copyProjIntoProjectDir( juce::String text, juce::File project )
    {
        if(!validateNewProjectName(text)) return;
        const auto root = getAppDirs(MTProjects);
        const auto projDir = root.getChildFile(text);
        juce::Array<juce::File> sourceFiles;
        project.findChildFiles(sourceFiles, juce::File::findFiles, false, "*.kickPrj");
        if(sourceFiles.isEmpty() || projDir.createDirectory().failed()) return;
        const auto destination = projDir.getChildFile(text + mt::__projectFileSuffix);
        if(!sourceFiles[0].copyFileTo(destination))
        {
            projDir.deleteFile(); // Only the just-created empty directory can be removed.
            return;
        }
        auto destinationFile = projDir;
        loadProject(destinationFile);
    }

    void RecentProject::loadProject( juce::File& Project )
    {
        ui::LoadProjectAndAnimation::getInstance().startLoading( Project );
    }

    int RecentProject::getNumOfChild()
    {
        return settings.projects.getNumChildren();
    }

    juce::File RecentProject::getRecentProject( int row )
    {
        reversedRow = getNumOfChild() - row - 1;
        return juce::File( settings.projects.getProject( reversedRow ) );
    }

    int RecentProject::getReversedRow( int row )
    {
        return getNumOfChild() - row - 1;
    }

    void RecentProject::removeFileFromVTree( int row )
    {
        reversedRow = getReversedRow( row );
        settings.projects.removeProject( reversedRow );
    }

    void RecentProject::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        fileBrowser.setBoundsRelative( .8492f, .0267f, .0429f, .0733f );

        auto listHeight = b.getHeight() / 3;
        demoList.setBounds( b.removeFromTop( listHeight ).reduced( 10, 10 ) );
        templateList.setBounds( b.removeFromTop( listHeight ).reduced( 10, 10 ) );

        auto rem = b.reduced( 10, 5 );
        newProj.setBounds( rem.removeFromRight( 150 ).reduced( 0, 5 ) );
        projectList.setBounds( rem );
    }

    void RecentProject::setNumOptions( int NumOptions )
    {
        auto idxOffset = demoList.getSize() + templateList.getSize();
        recentItems.erase( recentItems.begin() + idxOffset, recentItems.end() );

        projectList.clear();

        for( int index = 0; index < NumOptions; ++index )
            addProjectList( index );

        selectedIndex = recentItems.empty() ? -1 : juce::jlimit(0, (int)recentItems.size() - 1, selectedIndex);
        resized();
    }

    void RecentProject::addDemos()
    {
        juce::File demoPath = getAppDirs( MTDemos );
        demoPath.findChildFiles( demoDirList, juce::File::findDirectories, false );
        // Sort the list numerically based on the number in the file name
        std::sort( demoDirList.begin(), demoDirList.end(),
                   []( const juce::File& a, const juce::File& b ) {
                       // Extract the numeric part from the file name
                       int numA = a.getFileName().retainCharacters( "0123456789" ).getIntValue();
                       int numB = b.getFileName().retainCharacters( "0123456789" ).getIntValue();

                       return numA < numB;
                   } );
        for( auto i = 0; i < demoDirList.size(); i++ )
        {
            auto recentItemsPtr = std::make_unique<RecentItems>();
            recentItemsPtr->setProperties( i, demoDirList[i].getFullPathName() );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };
            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                enableRecentProjVTreeEntry = false;
                loadProject( project );
            };

            demoList.addComp( recentItemsPtr.get() );
            recentItems.emplace_back( std::move( recentItemsPtr ) );
        }
        resized();
    }

    void RecentProject::addTemplates()
    {
        juce::File templatePath = getAppDirs( MTTemp );
        templatePath.findChildFiles( templateDirList, juce::File::findDirectories, false );
        // Sort the list numerically based on the number in the file name
        std::sort( templateDirList.begin(), templateDirList.end(),
                   []( const juce::File& a, const juce::File& b ) {
                       // Extract the numeric part from the file name
                       int numA = a.getFileName().retainCharacters( "0123456789" ).getIntValue();
                       int numB = b.getFileName().retainCharacters( "0123456789" ).getIntValue();

                       return numA < numB;
                   } );
        auto idxOffset = demoList.getSize();

        for( auto i = 0; i < templateDirList.size(); i++ )
        {
            auto recentItemsPtr = std::make_unique<RecentItems>();
            recentItemsPtr->setProperties( i + idxOffset, templateDirList[i].getFullPathName() );

            recentItemsPtr->clearAllShowBorderCallback = [&]() {
                clearAllShowBorder();
            };

            recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                selectedIndex = index;
            };

            recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                openVirtualKeyBoard( project );
            };

            templateList.addComp( recentItemsPtr.get() );
            recentItems.emplace_back( std::move( recentItemsPtr ) );
        }
    }

    void RecentProject::addProjectList( int index )
    {
        auto sz  = projectList.listOptions.size();
        auto idx = index + 1;

        if( sz < idx )
        {
            for( auto i = sz; i < idx; ++i )
            {
                auto file = getRecentProject( index );

                auto recentItemsPtr = std::make_unique<RecentItems>();
                auto idxOffset      = demoList.getSize() + templateList.getSize();

                recentItemsPtr->setProperties( recentItems.size(), file.getFullPathName() );

                recentItemsPtr->clearAllShowBorderCallback = [&]() {
                    clearAllShowBorder();
                };
                recentItemsPtr->onLongPressed = [&, idxOffset]( int Index ) {
                    auto b = projectList.listOptions[Index - idxOffset]->getBounds();

                    //TODO - Needs to be changed once finalist

                    b.setY( b.getY() + 432 );
                    b.setWidth( b.getWidth() - 105 );
                    b.setX( b.getX() + b.getWidth() + 8 );
                    b.setHeight( b.getHeight() - 26 );

                    longPressComp->setBounds( getLocalBounds() );
                    longPressComp->setCompBounds( b );
                    longPressComp->showComp( Index );
                };

                recentItemsPtr->updateSelectedIndex = [&]( int index ) {
                    selectedIndex = index;
                };

                recentItemsPtr->onMouseDown = [&]( int index, juce::File project ) {
                    juce::MessageManager::callAsync( [this, index, project] {
                        auto idxOffset = demoList.getSize() + templateList.getSize();
                        if( idxOffset <= index )
                        {
                            juce::File file = getRecentProject( index - idxOffset );
                            loadProject( file );
                        }
                    } );
                };

                projectList.addComp( recentItemsPtr.get() );
                recentItems.emplace_back( std::move( recentItemsPtr ) );
            }
        }
    }

    void RecentProject::clearAllShowBorder()
    {
        for( const auto& it : recentItems )
        {
            it->showBorder( false );
            it->repaint();
        }
    }

    void RecentProject::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        //Preventing Knob related acion while project is loading

        if( contracts::DclContractsHelper::isKnobMessage( hwMsg ) && contracts::DclContractsHelper::getKnobCode( hwMsg ) == contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
            knobRotated( contracts::DclContractsHelper::getKnobValue( hwMsg ) );

        if( contracts::DclContractsHelper::isButtonMessage( hwMsg ) && contracts::DclContractsHelper::getButtonCode( hwMsg ) == contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER && ( contracts::DclContractsHelper::getButtonState( hwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            knobClicked();
    }

    void RecentProject::knobClicked()
    {
        if( juce::isPositiveAndBelow(selectedIndex, recentItems.size()) )
            if( recentItems[selectedIndex]->onMouseDown )
                recentItems[selectedIndex]->onMouseDown( selectedIndex, juce::File() );
    }

    void RecentProject::knobRotated( double value )
    {
        if( longPressComp->isVisible() )
            longPressComp->setVisible( false );

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( false );

        selectedIndex = selectedIndex + value;

        if( selectedIndex < 0 )
            selectedIndex = 0;

        if( selectedIndex >= recentItems.size() )
            selectedIndex = recentItems.size() - 1;

        if( juce::isPositiveAndBelow( selectedIndex, recentItems.size() ) )
            recentItems[selectedIndex]->showBorder( true );

        if(!juce::isPositiveAndBelow(selectedIndex, recentItems.size())) return;
        auto projectsStartIndex = demoList.getSize() + templateList.getSize();
        if( selectedIndex >= projectsStartIndex )
        {
            auto visibleWidth    = projectList.getWidth();
            auto componentBounds = recentItems[selectedIndex]->getBounds();
            auto viewPosition    = projectList.getViewPositionX();

            // Calculate the right edge position of the component within the list
            auto componentRightEdge = componentBounds.getRight() - viewPosition;

            // If the right edge of the component is outside the visible area, adjust the view position
            if( componentRightEdge > visibleWidth )
            {
                projectList.setViewPosition( componentBounds.getX(), projectList.getViewPositionY() );
            }
            // If the left edge of the component is outside the visible area, adjust the view position
            else if( componentBounds.getX() < viewPosition )
            {
                projectList.setViewPosition( componentBounds.getX(), projectList.getViewPositionY() );
            }
        }
    }

    void RecentProject::visibilityChanged()
    {
        auto isProjectLoaded = ae.isProjectLoaded();
        header.showBackButton( isProjectLoaded );

        if( isVisible() )
        {
            settings.projects.refreshMaxNumOfRecentProj();
            setNumOptions( getNumOfChild() );
            auto projectStartIndex = demoList.getSize() + templateList.getSize();
            selectedIndex          = ( 0 < ( projectList.getSize() ) ) ? projectStartIndex : 0;

            clearAllShowBorder();
            if(juce::isPositiveAndBelow(selectedIndex, recentItems.size())) recentItems[selectedIndex]->showBorder( true );

            // Check if we have come here on first launch,
            if( bFirstLaunch )
            {
                bFirstLaunch = false;
                // Check if we have to auto load a project
                const auto policy = settings.projects.onLaunch();
                if(policy == "New Project")
                {
                    juce::String name = settings.projects.namePrefix().trim();
                    if(settings.projects.namingStyle().equalsIgnoreCase("TIME STAMP")) name = juce::Time::getCurrentTime().formatted("%d%b%y_%H%M%S").toUpperCase();
                    if(name.isEmpty()) name = "Untitled";
                    name = juce::File::createLegalFileName(name);
                    const auto target = getAppDirs(MTProjects).getNonexistentChildFile(name, {}, false);
                    if(validateNewProjectName(target.getFileName())) ui::LoadProjectAndAnimation::getInstance().startCreating(target);
                }
                else if(policy == "Last Used" || settings.projects.isAutoLoad())
                {
                    // Auto load project.
                    juce::String prevProj = settings.projects.currentProject();
                    if( prevProj.isNotEmpty() )
                    {
                        auto projFile = juce::File( prevProj );
                        if(projFile.exists()) loadProject(projFile);
                        else mt::utils::showNotification("The last project is unavailable; choose another project");
                    }
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            Recent List
    //////////////////////////////////////////////////////////////////////////////////////////////

    RecentList::RecentList()
    {
        setLookAndFeel( KickLooknFeel::getLnf() );
        setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::all );
        setScrollBarsShown( false, true, false, false );
        setScrollBarThickness( 7 );
        setWantsKeyboardFocus( false );
        listComp.setWantsKeyboardFocus( false );
        setViewedComponent( &listComp, false );
    }

    void RecentList::clear() noexcept
    {
        listComp.removeAllChildren();
        listOptions.clear();
        resized();
    }

    void RecentList::addComp( RecentItems* recentItem ) noexcept
    {
        listOptions.push_back( recentItem );
        mt::utils::addAndMakeVisible( &listComp, recentItem );
        resized();
    }

    void RecentList::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        auto optionHeight = bounds.getHeight() - 10;
        auto optionWidth  = 200;

        auto x = 0;
        for( auto index = 0; index < listOptions.size(); ++index )
        {
            listOptions[index]->setBounds( x, 0, optionWidth, optionHeight );
            x += optionWidth;
        }
        listComp.setSize( x, optionHeight );
    }

    void RecentList::visibilityChanged()
    {}

    juce::File RecentList::getRecentProject( int index )
    {
        juce::File file = juce::File( mt::getSettings().projects.getProject( numOptions - index - 1 ) );
        return file;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            New Project Btn
    //////////////////////////////////////////////////////////////////////////////////////////////

    NewProjectBtn::NewProjectBtn()
    {
        addAndMakeVisible( icon );
        icon.setInterceptsMouseClicks( false, false );
    }

    void NewProjectBtn::paint( juce::Graphics& g )
    {
        auto bounds     = getLocalBounds();
        auto background = bounds.removeFromRight( bounds.proportionOfWidth( 0.9 ) );

        mt::utils::fillRoundedRect( g, bounds.removeFromLeft( 3 ), ui::Theme::foregroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, background, ui::Theme::backgroundColour( 0 ) );
    }

    void NewProjectBtn::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        icon.setBoundsRelative( 0.1f, 0.2f, 0.9f, 0.6f );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            Recent Items
    //////////////////////////////////////////////////////////////////////////////////////////////

    RecentItems::RecentItems()
    {
        label.setFontHeight( 18 );

        label.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        timeStamp.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        date.setTextColour( ui::Theme::textColour( 1 ) );
        timeStamp.setTextColour( ui::Theme::textColour( 1 ) );

        mt::utils::addChildComponents( this, label, date, timeStamp, image, icon );
        mt::utils::makeVisible( image, label );

        date.setInterceptsMouseClicks( false, false );
        timeStamp.setInterceptsMouseClicks( false, false );
        icon.setInterceptsMouseClicks( false, false );
    }

    void RecentItems::setProperties( int Index, const juce::String& Path )
    {
        index = Index;
        path  = Path;

        if( path.isDirectory() )
        {
            juce::Array<juce::File> files;
            path.findChildFiles( files, juce::File::findFiles, false, "*.png" );

            if( files.isEmpty() )
            {
                label.setText( path.getFileNameWithoutExtension(), juce::dontSendNotification );
                timeStamp.setText( path.getLastModificationTime().toString( false, true, false, false ), juce::dontSendNotification );
                date.setText( path.getLastModificationTime().toString( true, false, false, false ), juce::dontSendNotification );

                // Component visibility
                mt::utils::makeVisible( date, timeStamp, icon );
            }
            else
            {
                image.setImage( "recent_project" + juce::File::getSeparatorString() + files[0].getFileName() );
                label.setText( files[0].getFileNameWithoutExtension(), juce::dontSendNotification );

                // Component visibility
                mt::utils::makeHidden( date, timeStamp, icon );
            }
        }
    }

    void RecentItems::showBorder( bool status )
    {
        if( bShowBorder != status )
        {
            bShowBorder = status;
            repaint();
        }
    }

    void RecentItems::paint( juce::Graphics& g )
    {
        if( icon.isVisible() )
            mt::utils::fillRoundedRect( g, image.getBounds(), ui::Theme::backgroundColour( 0 ) );

        if( bShowBorder )
            mt::utils::renderRoundedRect( g, getLocalBounds(), ui::Theme::outlineColour( 1 ), 2 );
    }

    void RecentItems::resized()
    {
        auto bounds = getLocalBounds();
        if( bounds.isEmpty() )
            return;

        image.setBoundsRelative( 0.05f, 0.02f, 0.9f, 0.83f );
        label.setBoundsRelative( 0.07f, 0.85f, 0.87f, 0.13f );
        icon.setBoundsRelative( 0.1f, 0.3f, 0.22f, 0.4f );
        timeStamp.setBoundsRelative( 0.35f, 0.35f, 0.55f, 0.15f );
        date.setBoundsRelative( 0.35f, 0.5f, 0.55f, 0.15f );
    }

    void RecentItems::mouseDown( const juce::MouseEvent& e )
    {
        if( clearAllShowBorderCallback )
            clearAllShowBorderCallback();

        if( updateSelectedIndex )
            updateSelectedIndex( index );

        showBorder( true );
        repaint();
        isLongPressTriggered = false;
        startTimer( longPressThreshold ); // Start the timer for long press detection
    }

    void RecentItems::mouseDrag( const juce::MouseEvent& e )
    {
        if( e.getDistanceFromDragStart() > dragThreshold )
        {
            DBG( "MouseDrag -> isDragging" );

            isDragging = true;
            stopTimer(); // Stop the timer if dragging is detected
        }
    }

    void RecentItems::mouseUp( const juce::MouseEvent& e )
    {
        if( isLongPressTriggered && !isDragging )
        {
            DBG( "LongPressed " );
            if( onLongPressed )
                onLongPressed( index );
        }
        else if( e.mouseWasClicked() && !isDragging ) //Ignore dragging
        {
            startTimer( juce::MouseEvent::getDoubleClickTimeout() + 5 ); //Let it happend if it is double-click
        }
    }

    void RecentItems::mouseDoubleClick( const juce::MouseEvent& e )
    {
        isDoubleClicked = true;
    }

    void RecentItems::timerCallback()
    {
        if( !isDragging && getTimerInterval() == longPressThreshold )
        {
            DBG( "Longpressed timer " );

            isLongPressTriggered = true;
        }
        else //mouseDown or mouseDoubleClick
        {
            juce::Timer::callAfterDelay( 20, [&]() { //Give some delay to draw the border
                if( onMouseDown )
                    onMouseDown( index, path );
            } );
            isDoubleClicked = false;
        }
        stopTimer();
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////         LongPressComp      ////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    LongPressComp::LongPressComp()
    {
        mt::utils::addAndMakeVisible( this, loadBtn, removeBtn );
        loadBtn.onClick = [&]() {
            if( onLoadTriggered )
                onLoadTriggered( currentIndex );
        };

        removeBtn.onClick = [&]() {
            if( onRemoveTriggered )
                onRemoveTriggered( currentIndex );
        };
    }

    void LongPressComp::setCompBounds( juce::Rectangle<int> Bounds )
    {
        bounds = Bounds;
        resized();
    }

    void LongPressComp::showComp( int Index )
    {
        currentIndex = Index;
        setVisible( true );
        enterModalState( true, juce::ModalCallbackFunction::create( [this]( int R ) {
                             setVisible( false );
                         } ),
                         false );
    }

    void LongPressComp::mouseDown( const juce::MouseEvent& e )
    {
        if( !bounds.contains( e.getPosition() ) ) //checks whether clicking on outside
            setVisible( false );
    }

    void LongPressComp::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, bounds, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, true, true );
        mt::utils::renderRoundedRect( g, bounds, ui::Theme::textColour( 1 ), 1.0f, 2.0f, true, true, true, true );
    }

    void LongPressComp::resized()
    {
        auto b = bounds;
        loadBtn.setBounds( b.removeFromTop( bounds.getHeight() / 2 ).withTrimmedTop( 10 ).withTrimmedBottom( 5 ).withTrimmedLeft( 10 ).withTrimmedRight( 10 ) );
        removeBtn.setBounds( b.removeFromBottom( bounds.getHeight() / 2 ).withTrimmedTop( 5 ).withTrimmedBottom( 10 ).withTrimmedLeft( 10 ).withTrimmedRight( 10 ) );
    }
}
