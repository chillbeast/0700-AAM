// Audio-region playback and bounded, one-pass capture. No file or tree access in render.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace mt
{
    struct AudioAutomation
    {
        std::array<std::vector<std::pair<int, float>>, 7> lanes;
        static int index(const juce::String& name)
        {
            const std::array<juce::String, 7> names{"Volume", "PAN", "Mute", "Send1", "Send2", "Send3", "Send4"};
            for(int i = 0; i < 7; ++i) if(name.equalsIgnoreCase(names[i])) return i;
            return -1;
        }
        float value(int lane, int tick, float fallback) const
        {
            const auto& points = lanes[(size_t)lane];
            const auto after = std::upper_bound(points.begin(), points.end(), tick, [](int t, const auto& p) { return t < p.first; });
            return after == points.begin() ? fallback : std::prev(after)->second;
        }
    };
    struct AudioPlaybackClock
    {
        struct State { double tick{0}, rate{192}; int start{0}, end{768}; bool loop{true}, muted{false}; float left{1}, right{1}; uint64_t revision{0}; };
        juce::SpinLock lock;
        State state;
        std::atomic<bool> active{false};
        double cursor{0}; uint64_t seenRevision{~uint64_t(0)}; // Audio thread only.
        void stop() { active = false; }
        void update(double tick, double rate, juce::Range<int> range, bool loop, bool muted, float level, float pan, bool reset)
        {
            const juce::SpinLock::ScopedLockType guard(lock);
            if(reset || !active.load() || tick < state.tick) ++state.revision;
            state.tick = tick; state.rate = rate; state.start = range.getStart(); state.end = range.getEnd();
            state.loop = loop; state.muted = muted;
            state.left = level * (pan > 0 ? 1 - pan : 1); state.right = level * (pan < 0 ? 1 + pan : 1);
            active = true;
        }
    };

    class AudioTimeline
    {
    public:
        struct Region
        {
            ASound::Ptr source;
            int tick{0}, length{0}, begin{0}, end{0};
            float gain{1}, fadeIn{0}, fadeOut{0}; // Fades: fraction of the region duration.
            int fadeOffset{0}, fadeLength{0};
            double pitch{1}; bool reverse{false};
        };
        using Regions = std::vector<Region>;
        using Clocks = std::vector<std::shared_ptr<AudioPlaybackClock>>;
        void setRegions(Regions regions) { std::atomic_store(&clips, std::make_shared<const Regions>(std::move(regions))); }
        void setAutomation(std::shared_ptr<const AudioAutomation> curves) { std::atomic_store(&automation, std::move(curves)); }
        std::shared_ptr<const AudioAutomation> getAutomation() const { return std::atomic_load(&automation); }
        bool isAnyClockActive() const { for(const auto& c : *std::atomic_load(&clocks)) if(c->active.load()) return true; return false; }
        void attachClock(const std::shared_ptr<AudioPlaybackClock>& clock)
        {
            auto next = std::make_shared<Clocks>();
            for(auto& old : *std::atomic_load(&clocks)) if(old.use_count() > 1) next->push_back(old);
            next->push_back(clock); std::atomic_store(&clocks, std::shared_ptr<const Clocks>(next));
        }
        void render(juce::AudioBuffer<float>& output, double sampleRate, double programPitch = 1)
        {
            const auto regions = std::atomic_load(&clips);
            const auto curves = getAutomation();
            if(sampleRate <= 0) return;
            for(const auto& clock : *std::atomic_load(&clocks))
            {
                if(!clock->active.load()) continue;
                AudioPlaybackClock::State state;
                { const juce::SpinLock::ScopedTryLockType guard(clock->lock); if(!guard.isLocked()) continue; state = clock->state; }
                if(state.rate <= 0 || state.end <= state.start) continue;
                if(clock->seenRevision != state.revision || std::abs(clock->cursor - state.tick) > juce::jmax(4.0, state.rate * .1))
                { clock->cursor = state.tick; clock->seenRevision = state.revision; }
                const double tickStep = state.rate / sampleRate;
                int curveTick = -1; float volume = 1, panLeft = 1, panRight = 1; bool muted = false;
                for(int frame = 0; frame < output.getNumSamples(); ++frame)
                {
                    if(clock->cursor >= state.end - 1.e-9)
                    {
                        if(!state.loop) break;
                        clock->cursor = state.start + std::fmod(juce::jmax((double)state.end, clock->cursor) - state.start, state.end - state.start);
                    }
                    const int tick = (int)std::floor(clock->cursor + 1.e-9);
                    if(tick != curveTick)
                    {
                        curveTick = tick; volume = curves->value(0, tick, 100) / 100;
                        const float pan = (curves->value(1, tick, 50) - 50) / 50;
                        panLeft = pan > 0 ? 1 - pan : 1; panRight = pan < 0 ? 1 + pan : 1;
                        muted = curves->value(2, tick, 0) >= 50;
                    }
                    if(!state.muted && !muted) for(const auto& region : *regions)
                    {
                        const double elapsed = clock->cursor - region.tick;
                        if(elapsed < 0 || elapsed >= region.length || !region.source) continue;
                        const auto& data = region.source->getData(); // Sources are loaded before publication.
                        if(region.begin < 0 || region.end <= region.begin || region.end > data.getNumSamples() || data.getNumChannels() == 0) continue;
                        const double distance = elapsed / state.rate * region.source->getSampleRate() * region.pitch * programPitch;
                        if(distance >= region.end - region.begin) continue;
                        const double pos = region.reverse ? region.end - 1 - distance : region.begin + distance;
                        const int first = juce::jlimit(region.begin, region.end - 1, (int)pos);
                        const int last = juce::jmin(region.end - 1, first + 1);
                        float gain = region.gain;
                        const int fadeLength = region.fadeLength > 0 ? region.fadeLength : region.length;
                        const double fadeTime = elapsed + region.fadeOffset;
                        if(region.fadeIn > 0) gain *= (float)juce::jlimit(0.0, 1.0, fadeTime / (fadeLength * region.fadeIn));
                        if(region.fadeOut > 0) gain *= (float)juce::jlimit(0.0, 1.0, (fadeLength - fadeTime) / (fadeLength * region.fadeOut));
                        for(int ch = 0; ch < juce::jmin(2, output.getNumChannels()); ++ch)
                        {
                            const auto* in = data.getReadPointer(juce::jmin(ch, data.getNumChannels() - 1));
                            output.addSample(ch, frame, (float)(in[first] + (in[last] - in[first]) * (pos - first)) * gain * volume * (ch ? state.right * panRight : state.left * panLeft));
                        }
                    }
                    clock->cursor += tickStep;
                }
            }
        }
        bool prepareCapture(int frames, double sampleRate, std::shared_ptr<AudioPlaybackClock> clock)
        {
            // 256 MiB maximum, checked before allocation; a take ends at the active range end.
            if(frames < 1 || frames > (256 * 1024 * 1024 / (2 * (int)sizeof(float))) || sampleRate <= 0 || !clock) return false;
            const juce::SpinLock::ScopedLockType guard(captureLock);
            capture.setSize(2, frames); capture.clear(); captured = 0; captureRate = sampleRate;
            captureClock = std::move(clock); complete = false; failed = false; recording = true;
            return true;
        }
        void record(const float* left, const float* right, int frames, double sampleRate)
        {
            if(!recording.load()) return;
            const juce::SpinLock::ScopedTryLockType guard(captureLock);
            if(!guard.isLocked()) { failed = true; recording = false; return; }
            if(!captureClock || !captureClock->active.load()) return; // Count-in has not reached the timeline.
            if(!left || !right || sampleRate != captureRate) { failed = true; recording = false; return; }
            const int count = juce::jmin(frames, capture.getNumSamples() - captured);
            capture.copyFrom(0, captured, left, count); capture.copyFrom(1, captured, right, count); captured += count;
            if(captured == capture.getNumSamples()) { recording = false; complete = true; }
        }
        bool finishCapture(juce::AudioBuffer<float>& result, int& frames, double& rate)
        {
            const juce::SpinLock::ScopedLockType guard(captureLock);
            recording = false; frames = captured; rate = captureRate; result = std::move(capture); captureClock.reset();
            return !failed.load();
        }
        bool captureDone() const { return complete.load() || failed.load(); }
        void invalidateCapture() { if(recording.exchange(false)) failed = true; }
    private:
        std::shared_ptr<const Regions> clips{std::make_shared<const Regions>()};
        std::shared_ptr<const Clocks> clocks{std::make_shared<const Clocks>()};
        std::shared_ptr<const AudioAutomation> automation{std::make_shared<const AudioAutomation>()};
        juce::SpinLock captureLock;
        juce::AudioBuffer<float> capture;
        std::shared_ptr<AudioPlaybackClock> captureClock;
        std::atomic<bool> recording{false}, complete{false}, failed{false};
        int captured{0}; double captureRate{0};
    };
}
