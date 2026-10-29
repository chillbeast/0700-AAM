// Timeline editing, capture commit and routing for audio programs.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace mt
{
    void MTAudioProgram::refresh()
    {
        if(!prog) return;
        AudioTimeline::Regions regions;
        for(auto clip : sampleTree)
        {
            if((bool)clip.getProperty(IDs::mute, false) || (bool)clip.getProperty("audioSilent", false)) continue;
            const auto path = clip.getProperty(IDs::source).toString();
            if(path.isEmpty()) continue;
            auto source = ae.getAssetManager().getAssetSound(path);
            if(!source || !source->isValid()) continue;
            source->getData(); // Load before publishing; render performs no file IO.
            AudioTimeline::Region region;
            region.source = source;
            region.tick = juce::jmax(0, (int)clip.getProperty(IDs::startTime, 0));
            region.length = juce::jmax(0, (int)clip.getProperty(IDs::length, 0));
            region.begin = juce::jlimit(0, source->getLength(), (int)clip.getProperty(IDs::start, 0));
            const int end = clip.getProperty(IDs::end, 0);
            region.end = juce::jlimit(region.begin, source->getLength(), end > 0 ? end : source->getLength());
            region.gain = juce::Decibels::decibelsToGain(juce::jlimit(-60.f, 12.f, (float)clip.getProperty(IDs::level, 0)));
            region.reverse = clip.getProperty(IDs::reverse, false);
            const double semi = juce::jlimit(-12.0, 12.0, (double)clip.getProperty(IDs::semi, 0));
            const double fine = juce::jlimit(-99.0, 99.0, (double)clip.getProperty(IDs::fine, 0));
            region.pitch = std::exp2((semi + fine / 100.0) / 12.0);
            region.fadeIn = juce::jlimit(0.f, 1.f, (float)clip.getProperty("fadeIn", 0) / 100.f);
            region.fadeOut = juce::jlimit(0.f, 1.f, (float)clip.getProperty("fadeOut", 0) / 100.f);
            region.fadeOffset = clip.getProperty("fadeOffsetTicks", 0);
            region.fadeLength = clip.getProperty("fadeLengthTicks", region.length);
            if(region.length > 0 && region.end > region.begin) regions.push_back(std::move(region));
        }
        prog->timeline.setRegions(std::move(regions));
        auto curves = std::make_shared<AudioAutomation>();
        for(auto lane : vTree.getChildWithName("PARAMETERS"))
        {
            const int index = AudioAutomation::index(lane.getType().toString()); if(index < 0) continue;
            auto& points = curves->lanes[(size_t)index];
            for(int i = 0; i < lane.getNumProperties(); ++i)
            {
                auto name = lane.getPropertyName(i).toString();
                const double value = lane.getProperty(lane.getPropertyName(i));
                if(name.startsWithChar('s') && name.substring(1).isNotEmpty() && name.substring(1).containsOnly("0123456789") && std::isfinite(value))
                    points.emplace_back(name.substring(1).getIntValue(), (float)juce::jlimit(0.0, 100.0, value));
            }
            std::sort(points.begin(), points.end());
        }
        prog->timeline.setAutomation(curves);
        playbackStopped();
    }

    void MTAudioProgram::applySendAutomation(int tick)
    {
        const auto curves = prog->timeline.getAutomation(); const auto defaults = getSendGains();
        for(int i = 0; i < juce::jmin(4, (int)defaults.size()); ++i)
        {
            const float value = curves->value(i + 3, tick, defaults[(size_t)i] * 100) / 100;
            if(std::abs(prog->getGainForSends(i) - value) > 1.e-6f) prog->setGainForSend(i, value);
        }
    }

    void MTAudioProgram::playbackStopped()
    {
        if(!prog->timeline.isAnyClockActive()) prog->setGainsForSend(getSendGains());
    }

    void MTAudioProgram::clearAutomation(const juce::String& name)
    {
        auto lane = getParamTree(name); if(!lane.isValid()) return;
        if(um) um->beginNewTransaction("Clear audio automation");
        lane.removeAllProperties(um);
    }

    void MTAudioProgram::eraseAutomationPoint(const juce::String& name, int tick)
    {
        auto lane = getParamTree(name); if(lane.isValid() && tick >= 0) lane.removeProperty("s" + juce::String(tick), um);
    }

    void MTAudioProgram::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
    {
        MTIProgram::valueTreePropertyChanged(tree, property);
        if(prog && tree != vTree) triggerAsyncUpdate();
    }

    void MTAudioProgram::updateInput()
    {
        const auto& names = ae.getInputBusNames();
        const auto found = std::find(names.begin(), names.end(), input.get().toStdString());
        const uint32_t id = found == names.end() ? 0 : engine.getBusManager()->getBusId(*found);
        if(id != connectedInputId)
        {
            prog->timeline.invalidateCapture();
            if(connectedInputId) engine.unpatch(connectedInputId, prog->getId());
            connectedInputId = id && prog->connectInputTo(id) ? id : 0;
        }
        prog->inputConnected = connectedInputId != 0;
    }

    std::shared_ptr<AudioPlaybackClock> MTAudioProgram::createPlaybackClock()
    {
        auto clock = std::make_shared<AudioPlaybackClock>(); prog->timeline.attachClock(clock); return clock;
    }

    void MTAudioProgram::setSelectedAudioProperty(const juce::Identifier& property, const juce::var& value)
    {
        for(auto clip : selectedList)
        {
            clip.setProperty(property, value, um);
            if(property == juce::Identifier("fadeIn") || property == juce::Identifier("fadeOut"))
            { clip.removeProperty("fadeOffsetTicks", um); clip.removeProperty("fadeLengthTicks", um); }
        }
    }

    juce::String MTAudioProgram::getDrawingSource() const
    {
        if(drawingSource.isNotEmpty()) return drawingSource;
        for(auto clip : sampleTree) if(clip.getProperty(IDs::source).toString().isNotEmpty()) return clip.getProperty(IDs::source).toString();
        return {};
    }

    juce::ValueTree MTAudioProgram::copySelectedAudio() const
    {
        juce::ValueTree result{"AUDIO_CLIPBOARD"};
        if(selectedList.isEmpty()) return result;
        int first = INT_MAX;
        for(auto clip : selectedList) if(clip.getParent() == sampleTree) first = juce::jmin(first, (int)clip.getProperty(IDs::startTime));
        for(auto clip : selectedList) if(clip.getParent() == sampleTree)
        {
            auto copy = clip.createCopy(); copy.setProperty(IDs::startTime, (int)clip.getProperty(IDs::startTime) - first, nullptr);
            result.addChild(copy, -1, nullptr);
        }
        return result;
    }

    bool MTAudioProgram::pasteAudio(const juce::ValueTree& clipboard, int atTick, int limit)
    {
        if(!ae.isProjectLoaded() || !clipboard.hasType("AUDIO_CLIPBOARD") || clipboard.getNumChildren() == 0 || atTick < 0) return false;
        for(auto clip : clipboard)
        {
            const auto source = clip.getProperty(IDs::source).toString();
            const int offset = clip.getProperty(IDs::startTime), length = clip.getProperty(IDs::length);
            if(offset < 0 || length < 1 || int64_t(atTick) + offset + length > limit || source.isEmpty() || !juce::File(source).existsAsFile()) return false;
            auto decoded = ae.getAssetManager().getAssetSound(source); if(!decoded || !decoded->isValid()) return false;
        }
        if(um) um->beginNewTransaction("Paste audio regions");
        selectedList.clearQuick();
        for(auto clip : clipboard)
        {
            auto copy = clip.createCopy();
            copy.setProperty(IDs::startTime, atTick + (int)clip.getProperty(IDs::startTime), nullptr);
            copy.setProperty(IDs::id, CommonUtils::createID(), nullptr);
            ae.addAsset(copy.getProperty(IDs::source).toString(), AssetTypes::Audio);
            sampleTree.addChild(copy, -1, um); selectedList.add(copy);
        }
        refresh(); return true;
    }

    juce::ValueTree MTAudioProgram::croppedAudio(const juce::ValueTree& clip, int start, int finish, double tickRate)
    {
        auto copy = clip.createCopy(); const int first = clip.getProperty(IDs::startTime);
        copy.setProperty(IDs::id, CommonUtils::createID(), nullptr);
        copy.setProperty(IDs::startTime, start, nullptr); copy.setProperty(IDs::length, finish - start, nullptr);
        copy.setProperty("fadeOffsetTicks", (int)clip.getProperty("fadeOffsetTicks", 0) + start - first, nullptr);
        copy.setProperty("fadeLengthTicks", clip.getProperty("fadeLengthTicks", clip.getProperty(IDs::length)), nullptr);
        auto source = ae.getAssetManager().getAssetSound(clip.getProperty(IDs::source).toString());
        if(source && tickRate > 0)
        {
            const double ratio = std::exp2((juce::jlimit(-12.0, 12.0, (double)clip.getProperty(IDs::semi, 0)) + juce::jlimit(-99.0, 99.0, (double)clip.getProperty(IDs::fine, 0)) / 100 + MTIProgramData::semi.get() + MTIProgramData::fine.get() / 100.0) / 12);
            const double step = source->getSampleRate() / tickRate * ratio;
            const int a = juce::jlimit(0, source->getLength(), (int)clip.getProperty(IDs::start, 0));
            const int b = juce::jlimit(a, source->getLength(), (int)clip.getProperty(IDs::end, 0) > 0 ? (int)clip.getProperty(IDs::end) : source->getLength());
            const bool reverse = clip.getProperty(IDs::reverse, false);
            const double offset = (start - first) * step, to = (finish - first) * step;
            copy.setProperty(IDs::start, (int)std::llround(juce::jlimit((double)a, (double)b, reverse ? b - to : a + offset)), nullptr);
            copy.setProperty(IDs::end, (int)std::llround(juce::jlimit((double)a, (double)b, reverse ? b - offset : a + to)), nullptr);
            if((int)copy.getProperty(IDs::end) <= (int)copy.getProperty(IDs::start)) copy.setProperty("audioSilent", true, nullptr);
        }
        return copy;
    }

    bool MTAudioProgram::splitSelectedAudio(int atTick, double tickRate)
    {
        if(tickRate <= 0) return false;
        auto selected = selectedList; bool split = false;
        if(um) um->beginNewTransaction("Split audio regions");
        for(auto clip : selected)
        {
            const int first = clip.getProperty(IDs::startTime);
            const int last = (int)juce::jmin<int64_t>(INT_MAX, int64_t(first) + (int)clip.getProperty(IDs::length));
            if(clip.getParent() != sampleTree || atTick <= first || atTick >= last) continue;
            auto left = croppedAudio(clip, first, atTick, tickRate), right = croppedAudio(clip, atTick, last, tickRate);
            sampleTree.removeChild(clip, um); selectedList.removeAllInstancesOf(clip);
            sampleTree.addChild(left, -1, um); sampleTree.addChild(right, -1, um); selectedList.add(right); split = true;
        }
        if(split) refresh(); return split;
    }

    bool MTAudioProgram::prepareRecording(const std::shared_ptr<AudioPlaybackClock>& clock, int startTick, int endTick, double tickRate, bool replace)
    {
        recordingError.clear();
        if(!armed.get()) { recordingError = "Arm the selected audio track before recording"; return false; }
        if(!prog->inputConnected.load()) { recordingError = "Choose an audio input in Track Mixer"; return false; }
        if(takePrepared || !clock || tickRate <= 0 || endTick <= startTick) { recordingError = "Invalid audio recording range"; return false; }
        const double frames = std::ceil((endTick - startTick) / tickRate * prog->getSampleRate());
        if(frames < 1 || frames > 256.0 * 1024 * 1024 / (2 * sizeof(float))) { recordingError = "Audio take exceeds the 256 MiB recording limit; shorten the loop"; return false; }
        auto folder = ae.getProjectDir().getChildFile("Recorded");
        if(folder.createDirectory().failed()) { recordingError = "Cannot create the project's Recorded folder"; return false; }
        takeFile = folder.getChildFile("Take-" + juce::Uuid().toString() + ".wav");
        auto stream = takeFile.createOutputStream();
        if(!stream) { recordingError = "Cannot write an audio take in this project"; return false; }
        stream.reset(); takeFile.deleteFile();
        if(!prog->timeline.prepareCapture((int)frames, prog->getSampleRate(), clock)) { recordingError = "Unable to allocate the audio take buffer"; return false; }
        takeStart = startTick; takeEnd = endTick; takeTickRate = tickRate; replaceTake = replace; takePrepared = true;
        return true;
    }

    void MTAudioProgram::replaceAudioRange(int beginTick, int endTick, double tickRate)
    {
        for(int i = sampleTree.getNumChildren(); --i >= 0;)
        {
            auto clip = sampleTree.getChild(i);
            const int first = clip.getProperty(IDs::startTime), length = clip.getProperty(IDs::length);
            const int last = (int)juce::jmin<int64_t>(INT_MAX, int64_t(first) + length);
            if(first >= endTick || last <= beginTick) continue;
            auto piece = [&](int start, int finish) {
                sampleTree.addChild(croppedAudio(clip, start, finish, tickRate), -1, um);
            };
            if(first < beginTick) piece(first, beginTick);
            if(last > endTick) piece(endTick, last);
            sampleTree.removeChild(clip, um);
        }
        selectedList.clearQuick();
    }

    bool MTAudioProgram::finishRecording()
    {
        if(!takePrepared) return true;
        takePrepared = false;
        juce::AudioBuffer<float> data; int frames = 0; double rate = 0;
        if(!prog->timeline.finishCapture(data, frames, rate)) { recordingError = "Audio take interrupted by an input or sample-rate change; existing regions were preserved"; return false; }
        if(frames == 0) return true;
        juce::TemporaryFile temporary(takeFile);
        auto stream = temporary.getFile().createOutputStream();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream ? format.createWriterFor(stream.get(), rate, 2, 24, {}, 0) : nullptr);
        if(writer) stream.release();
        const bool written = writer && writer->writeFromAudioSampleBuffer(data, 0, frames) && writer->flush();
        writer.reset(); stream.reset();
        if(!written || takeFile.exists() || !temporary.overwriteTargetFileWithTemporary()) { recordingError = "Failed to save the audio take; existing regions were preserved"; return false; }
        const int status = ae.addAsset(takeFile.getFullPathName(), AssetTypes::Audio);
        if(status != AssetAdded && status != AssetAlreadyExists) { recordingError = "Audio take saved but could not be registered: " + takeFile.getFullPathName(); return false; }
        const int ticks = juce::jlimit(1, takeEnd - takeStart, (int)std::ceil(frames / rate * takeTickRate));
        if(replaceTake) replaceAudioRange(takeStart, takeStart + ticks, takeTickRate);
        juce::ValueTree clip{IDs::CLIP};
        clip.setProperty(IDs::source, takeFile.getFullPathName(), nullptr); clip.setProperty(IDs::slice, 0, nullptr);
        clip.setProperty(IDs::startTime, takeStart, nullptr); clip.setProperty(IDs::length, ticks, nullptr);
        clip.setProperty(IDs::start, 0, nullptr); clip.setProperty(IDs::end, frames, nullptr);
        sampleTree.addChild(clip, -1, um); drawingSource = takeFile.getFullPathName(); refresh();
        return true;
    }
}
