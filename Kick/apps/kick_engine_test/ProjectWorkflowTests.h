// Actual new-project defaults and atomic/automatic XML persistence.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace
{
    void testProjectWorkflow(Results& r)
    {
        r.section("Project defaults, autosave and failed writes");
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KickProjects-" + juce::Uuid().toString());
        scratch.createDirectory();
        struct Document : mt::KickXML {
            Document() : KickXML(mt::IDs::ROOT, ".testxml", "1") {}
            bool fail{false};
            bool writeDocument(const juce::File& file, const juce::String& text) override { return !fail && KickXML::writeDocument(file, text); }
        };
        {
            Document doc, other; doc.setAutoSave(false); other.setAutoSave(false);
            const auto file = scratch.getChildFile("Documents/requested-name.testxml");
            auto state = doc.loadOrCreate(file);
            r.check(state.isValid() && file.existsAsFile() && !file.getParentDirectory().getChildFile("Documents.testxml").exists(), "Project XML: honor the explicitly requested file name");
            state.setProperty("counter", 1, nullptr); doc.save();
            r.check(doc.wasLastSaveSuccessful() && juce::ValueTree::fromXml(file.loadFileAsString()).getProperty("counter") == juce::var(1), "Project XML: checked save writes the active tree");
            const auto before = file.loadFileAsString();
            doc.fail = true; state.setProperty("counter", 2, nullptr); doc.save();
            r.check(!doc.wasLastSaveSuccessful() && file.loadFileAsString() == before, "Project XML: a failed write preserves the previous file byte-for-byte");
            doc.fail = false; doc.save();
            r.check(doc.wasLastSaveSuccessful() && juce::ValueTree::fromXml(file.loadFileAsString()).getProperty("counter") == juce::var(2), "Project XML: a later successful save recovers from failure");
            auto bad = scratch.getChildFile("bad.testxml"); bad.replaceWithText("<not-valid-xml");
            r.check(!doc.loadOrCreate(bad).isValid() && doc.getState().getProperty("counter") == juce::var(2), "Project XML: rejected input preserves the currently loaded tree");
            doc.setAutoSaveTime(50); state.setProperty("counter", 3, nullptr); pump(140);
            r.check(juce::ValueTree::fromXml(file.loadFileAsString()).getProperty("counter") == juce::var(2), "Autosave: disabled project documents do not write on edits");
            doc.setAutoSave(true);
            const auto saveDeadline = juce::Time::getMillisecondCounterHiRes() + 3000.0;
            while(juce::ValueTree::fromXml(file.loadFileAsString()).getProperty("counter") != juce::var(3)
                && juce::Time::getMillisecondCounterHiRes() < saveDeadline) pump(20);
            r.check(juce::ValueTree::fromXml(file.loadFileAsString()).getProperty("counter") == juce::var(3), "Autosave: enabling saves pending edits at the configured interval");
            r.check(!other.isAutoSaveEnabled() && doc.isAutoSaveEnabled(), "Autosave: enabling one document does not change another document's policy");
            state.setProperty("counter", 4, nullptr); doc.close(); const auto closed = file.loadFileAsString(); pump(140);
            r.check(juce::ValueTree::fromXml(closed).getProperty("counter") == juce::var(4) && file.loadFileAsString() == closed, "Autosave: closing commits once and cancels delayed writes");
        }

        {
            Document doc; doc.setAutoSave(false); const auto file = scratch.getChildFile("blocked.testxml");
            auto state = doc.loadOrCreate(file); file.deleteFile(); file.createDirectory(); state.setProperty("counter", 9, nullptr); doc.save();
            r.check(!doc.wasLastSaveSuccessful() && file.isDirectory(), "Project XML: saving cannot remove a conflicting directory");
            const auto folder = scratch.getChildFile("Blocked load"); const auto target = mt::KickXML::resolveFile(folder, ".testxml"); target.createDirectory();
            r.check(!doc.loadOrCreate(folder).isValid() && target.isDirectory() && doc.getState().getProperty("counter") == juce::var(9), "Project XML: a directory collision rejects loading and preserves the current tree");
            doc.close();
        }

        auto& audio = mt::getAudioInterface(); auto& settings = mt::getSettings();
        auto preferences = settings.getState().getChildWithName(mt::IDs::PROJECTDEFAULTS);
        const auto beforePrefs = preferences.createCopy();
        settings.projects.isAutoSave = false; settings.projects.saveAfter = "2 min";
        r.check(!audio.getManager()->isAutoSaveEnabled() && audio.getManager()->getAutoSaveInterval() == 120000, "Autosave preferences: enable and minute intervals apply without opening Preferences");
        settings.projects.isAutoSave = true; settings.projects.saveAfter = "15 Sec";
        r.check(audio.getManager()->isAutoSaveEnabled() && audio.getManager()->getAutoSaveInterval() == 15000, "Autosave preferences: live changes reconfigure the project document");
        settings.projects.saveAfter = "invalid";
        r.check(settings.projectAutosaveInterval() == 30000, "Autosave preferences: malformed saved intervals use a bounded fallback");
        settings.projects.isAutoSave = false;
        settings.projects.tempo = 137; settings.projects.bars = 3; settings.projects.numSeq = 2; settings.projects.numTrackPerSeq = 2;
        settings.projects.numerator = "7"; settings.projects.denominator = "8"; settings.projects.rootNote = 2; settings.projects.scale = 1;
        const auto folder = scratch.getChildFile("New project");
        r.check(audio.createProject(folder), "Project defaults: create a new project through the production API");
        r.check(audio.getNumSequences() == 2 && audio.getNumPrograms() == 1 && audio.getManager()->getSequence(0)->getNumTracks() == 2
            && audio.getManager()->getSequence(1)->getNumTracks() == 2, "Project defaults: sequence and track counts are applied with one initial drum program");
        auto* sequence = audio.getManager()->getSequence(0); auto* track = audio.getManager()->getTrack(0, 0);
        r.check(audio.getMasterBPM() == 137 && sequence->getBPM() == 137 && sequence->getLength() == 3, "Project defaults: tempo and bar length drive the actual transport");
        r.check(sequence->getNumerator() == 7 && sequence->getDenominator() == 8 && track->getLengthInTicks() == 1008,
            "Project defaults: the chosen meter controls sequence and track tick lengths");
        auto* program = audio.getProgram(0);
        r.check(program && track->getProgram() == program && program->performRootNote.get() == "D" && program->performScale.get() == mt::ScaleAndNotes::scaleNames[1],
            "Project defaults: initial tracks and musical key are connected to their program");
        settings.projects.rootNote = 9; settings.projects.scale = 3;
        const int plugin = audio.createProgram(mt::MTProgramType::Plugin, "Default key");
        const auto layout = mt::PadLayout::fromTree(audio.getProgram(plugin)->getState().getChildWithName(mt::IDs::PAD_LAYOUT));
        r.check(layout.root == 2 && layout.scaleIndex == 1, "Project defaults: later programs inherit the project's saved key instead of changed global preferences");
        r.check(!audio.createProject(folder) && audio.isProjectLoaded() && audio.getNumSequences() == 2,
            "Project defaults: creating over an existing project is rejected without replacing the open project");
        auto conflict = scratch.getChildFile("Project conflict"); const auto conflictFile = mt::KickXML::resolveFile(conflict, ".kickPrj"); conflictFile.createDirectory();
        r.check(!audio.createProject(conflict) && !audio.loadProject(conflict) && conflictFile.isDirectory() && audio.isProjectLoaded(), "Project I/O: creation and loading preserve directory collisions and the current project");
        auto broken = scratch.getChildFile("broken.kickPrj"); broken.replaceWithText("<broken");
        r.check(!audio.loadProject(broken) && audio.isProjectLoaded() && audio.getNumSequences() == 2,
            "Project load: corrupt input is rejected before closing the current project");
        r.check(audio.saveProject(), "Project defaults: save complete project state");
        auto projectFile = mt::KickXML::resolveFile(folder, ".kickPrj");
        r.check(audio.loadProject(projectFile) && audio.getNumSequences() == 2 && audio.getManager()->getSequence(1)->getNumTracks() == 2
            && audio.getManager()->getSequence(1)->getNumerator() == 7, "Project defaults: counts and meter survive real save/reload");
        const auto restored = mt::PadLayout::fromTree(audio.getProgram(plugin)->getState().getChildWithName(mt::IDs::PAD_LAYOUT));
        r.check(restored.root == 2 && restored.scaleIndex == 1, "Project defaults: instrument key survives real reload");
        audio.closeProject(); preferences.copyPropertiesAndChildrenFrom(beforePrefs, nullptr); settings.applyProjectAutosave(); scratch.deleteRecursively();
    }
}
