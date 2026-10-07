#pragma once

#include "Common.h"
#include <vector>

namespace rc
{
class Arpeggiator
{
public:
    struct Event { int pos; int note; float vel; bool on; };
    struct Settings { int mode = 0; int rate = 3; int octaves = 1; float gate = 0.5f; float swing = 0.0f; bool latch = false; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        held.reserve (128); playing.reserve (256); pendingOffs.reserve (256); seq.reserve (512); base.reserve (128);
        reset();
    }

    void reset()
    {
        held.clear(); physicalDown = 0; counter = 0.0; step = 0;
        playing.clear(); pendingOffs.clear();
    }

    void noteOn (int note, float vel, bool latch)
    {
        if (latch && physicalDown == 0) held.clear();
        ++physicalDown;
        for (auto& h : held) if (h.note == note) return;
        held.push_back ({ note, vel });
        if (held.size() == 1) { counter = 0.0; step = 0; startPending = true; }
    }

    void noteOff (int note, bool latch)
    {
        physicalDown = std::max (0, physicalDown - 1);
        if (latch) return;
        held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
    }

    void clearLatched() { held.clear(); }

    bool isIdle() const { return held.empty() && playing.empty(); }

    void process (int n, double bpm, const Settings& s, std::vector<Event>& out)
    {
        static const double beats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double stepLen = 60.0 / std::max (20.0, bpm) * sr * beats[std::clamp (s.rate, 0, 5)];

        for (int i = 0; i < n; ++i)
        {
            // note-offs due
            for (size_t k = 0; k < pendingOffs.size();)
            {
                if (--pendingOffs[k].remaining <= 0)
                {
                    out.push_back ({ i, pendingOffs[k].note, 0.0f, false });
                    playing.erase (std::remove (playing.begin(), playing.end(), pendingOffs[k].note), playing.end());
                    pendingOffs.erase (pendingOffs.begin() + (long) k);
                }
                else ++k;
            }

            if (held.empty()) { counter = 0.0; continue; }

            const double swingOffset = (step % 2 == 1) ? s.swing * stepLen * 0.5 : 0.0;
            if (startPending || counter >= stepLen + swingOffset)
            {
                if (! startPending) counter -= stepLen;
                startPending = false;
                fireStep (i, s, (int) (stepLen * s.gate), out);
            }
            counter += 1.0;
        }
    }

    void allOff (int pos, std::vector<Event>& out)
    {
        for (int nte : playing) out.push_back ({ pos, nte, 0.0f, false });
        playing.clear(); pendingOffs.clear();
    }

private:
    struct Held { int note; float vel; };
    struct Off { int note; int remaining; };

    void fireStep (int pos, const Settings& s, int gateLen, std::vector<Event>& out)
    {
        seq.clear();
        base = held;
        if (s.mode != 4)
            std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
        for (int o = 0; o < std::max (1, s.octaves); ++o)
            for (auto& h : base)
                if (h.note + 12 * o < 128) seq.push_back ({ h.note + 12 * o, h.vel });
        if (seq.empty()) return;
        gateLen = std::max (gateLen, 16);

        if (s.mode == 5) // chord
        {
            for (auto& h : base) trigger (pos, h.note, h.vel, gateLen, out);
            ++step;
            return;
        }

        const int len = (int) seq.size();
        int idx = 0;
        switch (s.mode)
        {
            case 0: case 4: idx = step % len; break;
            case 1: idx = len - 1 - (step % len); break;
            case 2:
            {
                const int period = std::max (1, 2 * len - 2);
                const int p = step % period;
                idx = p < len ? p : period - p;
                break;
            }
            case 3: idx = (int) (rng.next01() * (float) len) % len; break;
            default: break;
        }
        trigger (pos, seq[(size_t) idx].note, seq[(size_t) idx].vel, gateLen, out);
        ++step;
    }

    void trigger (int pos, int note, float vel, int gateLen, std::vector<Event>& out)
    {
        // if still sounding, cut it first
        for (size_t k = 0; k < pendingOffs.size(); ++k)
            if (pendingOffs[k].note == note)
            {
                out.push_back ({ pos, note, 0.0f, false });
                pendingOffs.erase (pendingOffs.begin() + (long) k);
                break;
            }
        out.push_back ({ pos, note, vel, true });
        playing.push_back (note);
        pendingOffs.push_back ({ note, gateLen });
    }

    double sr = 48000.0, counter = 0.0;
    int step = 0, physicalDown = 0;
    bool startPending = false;
    std::vector<Held> held, seq, base;
    std::vector<int> playing;
    std::vector<Off> pendingOffs;
    Rng rng;
};
} // namespace rc
