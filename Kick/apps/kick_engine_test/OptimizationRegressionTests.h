// Optimization round 2: playback boundaries and exact DSP cache invalidation.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace
{
    void testOptimizationWave(Results& R)
    {
        pump(500);
        R.section("Optimization round 2: sample voice isolation and loop interpolation");
        {
            mt::ASliceRenderer a, b;
            auto& first = a.getNext();
            first.setSample(0, 0, 0.75f); // A voice filter may write into its returned frame.
            const auto& second = b.getNext();
            R.check(second.getMagnitude(0, 1) == 0.f && &first != &second,
                "O1: stopped sample voices own independent silent frames");
            R.check(a.getNext().getMagnitude(0, 1) == 0.f,
                "O1: returning silence clears a frame modified by the previous filter");
            first.clear(); // Keep the pre-fix shared-buffer reproduction isolated.
        }
        const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("KickOptimize-" + juce::Uuid().toString());
        scratch.createDirectory();
        const auto wav = scratch.getChildFile("loop.wav");
        {
            juce::WavAudioFormat format;
            auto stream = wav.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 16, {}, 0));
            juce::AudioBuffer<float> source(2, 8);
            for(int ch = 0; ch < 2; ++ch)
                for(int i = 0; i < 8; ++i) source.setSample(ch, i, i * 0.1f);
            R.check(writer && writer->writeFromAudioSampleBuffer(source, 0, 8), "O2: periodic interpolation fixture writes successfully");
        }
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            mt::ASound::Ptr sound = new mt::ASound(wav.getFullPathName(), formats, 1.f, -1);
            juce::ValueTree tree(mt::IDs::SLICE);
            mt::ASoundSlice slice(tree);
            slice.setSource(sound);
            slice.setLoopPoints(2, 8);
            slice.setLooping(true);
            mt::ASliceRenderer renderer;
            renderer.setSlice(&slice);
            const float expected = (sound->getData().getSample(0, 7) + sound->getData().getSample(0, 2)) * 0.5f * slice.gL;
            for(bool reverse : {false, true})
            {
                slice.setReverse(reverse);
                renderer.reset();
                renderer.setPosition(7.5);
                R.check(std::abs(renderer.getNext(0.0).getSample(0, 0) - expected) < 1.e-6f,
                    reverse ? "O2: reverse fractional loop interpolates across its trimmed boundary"
                            : "O2: forward fractional loop interpolates across its trimmed boundary");
            }
            slice.setLooping(false);
            slice.setReverse(false);
            renderer.reset(); renderer.setPosition(7.5);
            R.check(std::abs(renderer.getNext(0.0).getSample(0, 0) - sound->getData().getSample(0, 7) * slice.gL) < 1.e-6f,
                "O2: one-shot interpolation holds its final sample without wrapping");
            R.check(sound->getReferenceCount() >= 3, "O5: sample playback retains the source behind its cached data pointers");
            mt::ASound::Ptr replacement = new mt::ASound(wav.getFullPathName(), formats, 1.f, -1);
            sound = nullptr;
            slice.setSource(replacement, true);
            R.check(renderer.getNext().getMagnitude(0, 1) == 0.f && renderer.hasFinished(),
                "O5: source replacement stops the old voice before reading its cached sample data");
            renderer.reset(); renderer.getNext();
            const bool rebound = renderer.isRunning();
            const float reboundLevel = renderer.getNext().getMagnitude(0, 1);
            R.check(rebound && reboundLevel > 0.f,
                "O5: resetting a replaced sample binds and plays the new source",
                rebound && reboundLevel > 0.f ? juce::String() : "range=" + juce::String(slice.getStart()) + "," + juce::String(slice.getEnd())
                    + " rate=" + juce::String(slice.getEffectiveResamplerRate()) + " pos=" + juce::String(renderer.getPosition())
                    + " gain=" + juce::String(slice.gL.load()) + " level=" + juce::String(reboundLevel) + " running=" + juce::String((int)rebound));
            slice.loopEnd = INT_MAX; // Malformed/restored trim must not expose ASound's four padding frames.
            renderer.reset(); renderer.advance(1.0, 8);
            R.check(renderer.hasFinished() && renderer.getPosition() == 8.0,
                "O5: playback duration excludes resampler padding even with an oversized trim");
            slice.setLoopPoints(0, 8);
            mt::ASound::Ptr other = new mt::ASound(wav.getFullPathName(), formats, 1.f, -1);
            std::atomic<bool> done{false};
            std::thread editor([&] {
                for(int i = 0; i < 500; ++i) slice.setSource(i % 2 ? replacement : other);
                done.store(true);
            });
            bool valid = true;
            do
            {
                renderer.reset();
                const auto& frame = renderer.getNext();
                valid &= std::isfinite(frame.getMagnitude(0, 1)) && renderer.getPosition() >= 0 && renderer.getPosition() <= 8;
            } while(!done.load());
            editor.join();
            R.check(valid, "O5: sample-source replacement and playback snapshots remain bounded under concurrent edits");
        }
        bool previewLifecycle = true;
        {
            mt::ASoundPlayer preview;
            preview.setSoundToPlay(wav.getFullPathName(), -1.0);
            preview.getNext();
            previewLifecycle &= preview.getNext().getMagnitude(0, 1) > 0.f;
            preview.removeSound();
            previewLifecycle &= preview.hasFinished() && preview.getNext().getMagnitude(0, 1) == 0.f;
            preview.setSoundToPlay(wav.getFullPathName(), -1.0);
            preview.getNext();
            previewLifecycle &= preview.getNext().getMagnitude(0, 1) > 0.f;
        } // ASan must also observe safe destruction of the embedded sample.
        R.check(previewLifecycle, "O5: browser preview load, purge, reload and destruction preserve embedded source lifetime");
        {
            R.section("Release review: live browser audition switching");
            const auto second = scratch.getChildFile("second.wav");
            {
                juce::WavAudioFormat format;
                auto stream = second.createOutputStream();
                std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 16, {}, 0));
                juce::AudioBuffer<float> data(2, 2048);
                for(int c = 0; c < 2; ++c) for(int i = 0; i < 2048; ++i) data.setSample(c, i, 0.2f);
                R.check(writer && writer->writeFromAudioSampleBuffer(data, 0, 2048), "O6: live audition switching fixture writes successfully");
            }
            AudIO::Design design(0, 2);
            mt::AudioPreview preview(design);
            auto& object = static_cast<AudIO::IProcessable&>(preview);
            AudIO::Buffer<float> output(0, false, 2, true);
            object.prepareToPlay(48000, output.getBlockSize());
            preview.setLooping(true);
            std::atomic<bool> done{false}, finite{true};
            std::atomic<int> blocks{0};
            std::thread render([&] {
                while(!done.load())
                {
                    object.process(output);
                    for(int c = 0; c < 2; ++c)
                        for(unsigned i = 0; i < output.getBlockSize(); ++i)
                            if(!std::isfinite(output.getOutputBuffer(c)[i]) || std::abs(output.getOutputBuffer(c)[i]) > 1.f) finite = false;
                    ++blocks;
                }
            });
            while(blocks.load() == 0) std::this_thread::yield();
            for(int i = 0; i < 300; ++i)
            {
                preview.previewOn((i % 2 ? wav : second).getFullPathName());
                preview.setGainAndPan((i % 9) / 8.0, 0.0);
                preview.setMute(i % 7 == 0);
                if(i % 3 == 0) preview.previewOff();
                if(i % 20 == 0) object.prepareToPlay(i % 40 ? 44100 : 48000, output.getBlockSize());
            }
            done = true;
            render.join();
            R.check(finite.load() && blocks.load() > 1, "O6: live audition file, gain, mute and rate changes remain bounded during concurrent rendering");
            preview.setLooping(false);
            preview.setMute(false);
            preview.setGain(1.0, true);
            preview.previewOn(wav.getFullPathName());
            object.process(output);
            juce::AudioBuffer<float> view((float* const*)output.getOutputs(), 2, output.getBlockSize());
            R.check(view.getMagnitude(0, view.getNumSamples()) > 0.f, "O6: a stable audition still renders after concurrent file switching");
            preview.previewOff();
            object.process(output);
            R.check(view.getMagnitude(0, view.getNumSamples()) == 0.f, "O6: stopping audition clears the next output block");
            preview.previewOn(wav.getFullPathName());
            preview.previewOn(scratch.getChildFile("missing.wav").getFullPathName());
            object.process(output);
            R.check(!preview.isRunning() && preview.isBypassed() && view.getMagnitude(0, view.getNumSamples()) == 0.f,
                "O6: an absent audition file stops instead of replaying the previous source");
            preview.setMute(true);
            preview.previewOn(wav.getFullPathName());
            object.process(output);
            R.check(preview.isMuted() && !preview.isRunning() && view.getMagnitude(0, view.getNumSamples()) == 0.f,
                "O6: audition mute survives file changes and muted one-shots advance to their end in silence");
        }
        scratch.deleteRecursively();

        R.section("Optimization round 2: mono effects and DSP parameter changes");
        {
            mt::modular::PatchBuilder patch("Mono effects regression", 1);
            patch.add("osc", 0, 0, {{"wave", 2}, {"level", 0.3f}});
            patch.addEffect("fxdelay", 0, {{"time", 10}, {"feedback", 0.4f}, {"mix", 0.7f}});
            mt::ModularEngine mono, stereo;
            for(auto* engine : {&mono, &stereo})
            {
                engine->prepare(48000, 512); engine->setPatch(patch.tree); engine->reset(); engine->noteOn(60, 0.8f);
            }
            bool equal = true;
            float peak = 0.f;
            for(int iteration = 0; iteration < 12; ++iteration)
            {
                if(iteration == 4) { mono.noteOff(60); stereo.noteOff(60); }
                const int n = iteration % 3 == 0 ? 33 : (iteration % 3 == 1 ? 128 : 333);
                float m[512]{}, l[512]{}, r[512]{};
                float* monoOut[]{m}; float* stereoOut[]{l, r};
                mono.process(monoOut, 1, n); stereo.process(stereoOut, 2, n);
                for(int i = 0; i < n; ++i)
                {
                    equal &= std::isfinite(m[i]) && std::abs(m[i] - l[i] - r[i]) < 3.e-6f;
                    peak = juce::jmax(peak, std::abs(m[i]));
                }
            }
            R.check(equal && peak > 0.001f, "O3: mono Modular output includes stereo effects and release tails, folded to mono");
        }

        for(const auto* type : {"osc", "filter"})
        {
            const auto* spec = mt::modular::findModuleSpec(type);
            auto cached = spec->create(), reference = spec->create();
            std::vector<float> values;
            for(const auto& param : spec->params) values.push_back(param.defaultValue);
            bool equal = true;
            for(int block = 0; block < 128; ++block)
            {
                const auto previous = values;
                auto set = [&](const char* id, float value) { const int p = spec->findParam(id); if(p >= 0) values[(size_t)p] = value; };
                set("fine", float((block / 7) % 5) * 5.f); set("tune", float((block / 17) % 3) * 12.f);
                set("unison", float(1 + (block / 11) % 7)); set("spread", float((block / 13) % 4) * 15.f);
                set("wave", float((block / 9) % 4)); set("input", float((block / 19) % 3));
                set("cutoff", 300.f + (block / 5) % 7 * 700.f); set("reso", float((block / 7) % 8) * 0.1f);
                set("track", float((block / 13) % 3) * 0.5f); set("mode", float((block / 19) % 5));
                const mt::modular::VoiceContext ctx{block / 23 % 2 ? 44100.0 : 48000.0, 48.f + float((block / 3) % 24), 0.8f, true, 120.0};
                float actual[16]{}, expected[16]{};
                for(int i = 0; i < 16; ++i) actual[i] = expected[i] = 0.1f * std::sin(float(block * 16 + i) * 0.03f);
                if(block % 31 == 0) { cached->reset(); reference->reset(); }
                reference->prepare(ctx.sampleRate); // Invalidate only coefficients, retaining signal history.
                cached->process(ctx, previous.data(), values.data(), actual, 16);
                reference->process(ctx, previous.data(), values.data(), expected, 16);
                for(int i = 0; i < 16; ++i) equal &= std::isfinite(actual[i]) && actual[i] == expected[i];
            }
            R.check(equal, juce::String("O4: ") + type + " cached calculations match fresh coefficients through control, rate and reset changes");
        }

        if(juce::SystemStats::getEnvironmentVariable("KICK_TEST_DSP_BENCH", {}).getIntValue() == 0) return;
        R.section("Optimization round 2: repeatable static-control DSP measurements");
        for(const auto* type : {"osc", "filter"})
        {
            const auto* spec = mt::modular::findModuleSpec(type);
            std::vector<float> values;
            for(const auto& param : spec->params) values.push_back(param.defaultValue);
            auto set = [&](const char* id, float value) { const int p = spec->findParam(id); if(p >= 0) values[(size_t)p] = value; };
            set("unison", 7.f); set("wave", 2.f); set("cutoff", 1000.f); set("reso", 0.4f);
            std::vector<std::unique_ptr<mt::modular::Module>> voices;
            for(int voice = 0; voice < 16; ++voice) { voices.push_back(spec->create()); voices.back()->prepare(48000); }
            std::vector<double> elapsed;
            float observed = 0.f;
            for(int pass = 0; pass < 6; ++pass)
            {
                const auto begin = juce::Time::getHighResolutionTicks();
                for(int block = 0; block < 3000; ++block)
                    for(int voice = 0; voice < 16; ++voice)
                    {
                        float data[16]; std::fill_n(data, 16, 0.1f);
                        const mt::modular::VoiceContext ctx{48000.0, 48.f + voice, 0.8f, true, 120.0};
                        voices[(size_t)voice]->process(ctx, values.data(), values.data(), data, 16);
                        observed += data[0];
                    }
                const auto ms = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) * 1000.0;
                if(pass > 0) elapsed.push_back(ms);
            }
            std::sort(elapsed.begin(), elapsed.end());
            R.note(juce::String(type) + ": 16 voices, 1 second at 48 kHz, median of 5 warmed runs = " + juce::String(elapsed[2], 3) + " ms; check=" + juce::String(observed, 3));
        }
    }
}
