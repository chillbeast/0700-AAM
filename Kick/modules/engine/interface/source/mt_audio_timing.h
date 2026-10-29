// Musical timing shared by recording, editing and panel performance.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace mt
{
    struct TimingCorrect
    {
        enum Type { Start, End, Length, Legato };
        MTTimeBase division{ MTTimeBase::_16 };
        Type type{ Start };
        int swing{ 50 }, shift{ 0 }, window{ 100 }, strength{ 100 };
        bool enabled{ true }, selectedOnly{ false };
        int rangeStart{0}, rangeEnd{std::numeric_limits<int>::max()};

        int barTicks{defaultPPQN * 4};
        int grid() const { return division == MTTimeBase::_1 ? juce::jmax(1, barTicks) : juce::jmax(1, juce::roundToInt(AudIO::ticksPerBeat((int)division, defaultPPQN))); }
        double nextGridTick(double position) const
        {
            const int step = grid();
            const int base = (int)std::floor(position / step);
            double next = std::numeric_limits<double>::max();
            for(int i = base - 2; i <= base + 3; ++i)
            {
                const double tick = i * step + ((i % 2) != 0 ? juce::roundToInt(step * (juce::jlimit(50, 75, swing) - 50) / 50.0) : 0);
                if(tick > position) next = juce::jmin(next, tick);
            }
            return next;
        }

        // Swing delays alternate grid points within each pair. Looking at both
        // neighbours also handles notes on the long side of a swung interval.
        int correct(int position, bool useSwing = true) const
        {
            if(!enabled) return position;
            const int step = grid();
            const int pair = int(std::floor(double(position) / (2 * step)));
            int nearest = 0;
            int distance = std::numeric_limits<int>::max();
            for(int p = pair - 1; p <= pair + 1; ++p)
                for(int odd = 0; odd < 2; ++odd)
                {
                    const int point = 2 * p * step + (odd ? juce::roundToInt(2.0 * step * (useSwing ? juce::jlimit(50, 75, swing) : 50) / 100.0) : 0);
                    const int d = std::abs(position - point);
                    if(d < distance) { nearest = point; distance = d; }
                }
            const int radius = useSwing ? juce::jlimit(50, 75, swing) : 50;
            if(distance * 10000.0 > step * radius * juce::jlimit(0, 100, window)) return position;
            return position + juce::roundToInt((nearest - position) * juce::jlimit(0, 100, strength) / 100.0) + shift;
        }

        static TimingCorrect read(const juce::ValueTree& tree)
        {
            TimingCorrect c;
            const auto division = magic_enum::enum_cast<MTTimeBase>((int)tree.getProperty("timingDivision", (int)MTTimeBase::_16));
            if(division) c.division = *division;
            c.enabled = tree.getProperty("timingEnabled", true);
            c.swing = juce::jlimit(50, 75, (int)tree.getProperty("timingSwing", 50));
            c.shift = juce::jlimit(-48, 48, (int)tree.getProperty("timingShift", 0));
            c.window = juce::jlimit(0, 100, (int)tree.getProperty("timingWindow", 100));
            c.strength = juce::jlimit(0, 100, (int)tree.getProperty("timingStrength", 100));
            return c;
        }

        void save(juce::ValueTree tree, juce::UndoManager* undo) const
        {
            tree.setProperty("timingDivision", (int)division, undo);
            tree.setProperty("timingEnabled", enabled, undo);
            tree.setProperty("timingSwing", swing, undo);
            tree.setProperty("timingShift", shift, undo);
            tree.setProperty("timingWindow", window, undo);
            tree.setProperty("timingStrength", strength, undo);
        }
    };

    class TapTempo
    {
    public:
        double tap(double milliseconds, int taps)
        {
            taps = juce::jlimit(2, 8, taps);
            if(!std::isfinite(milliseconds)) { reset(); return 0; }
            if(count && (milliseconds <= previous || milliseconds - previous > 2000.0)) reset();
            if(count && milliseconds - previous < 60000.0 / maxTempo) return 0; // switch bounce
            if(count) intervals.push_back(milliseconds - previous);
            previous = milliseconds;
            ++count;
            while((int)intervals.size() > taps - 1) intervals.erase(intervals.begin());
            if(count < taps) return 0;
            double total = 0;
            for(auto interval : intervals) total += interval;
            return juce::jlimit(double(minTempo), double(maxTempo), 60000.0 * intervals.size() / total);
        }
        void reset() { count = 0; previous = 0; intervals.clear(); }
    private:
        int count{0};
        double previous{0};
        std::vector<double> intervals;
    };
}
