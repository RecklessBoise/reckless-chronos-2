#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Tables.h"

using namespace rc;

RecklessChronosProcessor::RecklessChronosProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RC2", createLayout()),
      presets (apvts)
{
    for (int t = 0; t < kNumTimbres; ++t)
        for (int p = 0; p < tpCount; ++p)
            raw[(size_t) t][(size_t) p] = apvts.getRawParameterValue (timbreParamId (t, p));
    for (int g = 0; g < gpCount; ++g)
        graw[(size_t) g] = apvts.getRawParameterValue (globalParamId (g));

    presets.onPresetApplied = [this] (Mode m, const juce::String& name)
    {
        mode.store ((int) m);
        currentPresetName = name;
        panic();
        if (onPresetChanged) onPresetChanged();
    };

    initTables();
    if (! presets.programs().empty())
        loadProgram (0);
}

bool RecklessChronosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RecklessChronosProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    initTables();
    const int maxBlock = std::max (block, 512);
    for (auto& t : timbres) t.prepare ((float) sr, maxBlock);
    delay.prepare ((float) sr);
    reverb.prepare ((float) sr);
    limiter.prepare ((float) sr);
    arp.prepare (sr);
    mixBuf.setSize (2, maxBlock);
    dlySend.setSize (2, maxBlock);
    revSend.setSize (2, maxBlock);
    fxOut.setSize (2, maxBlock);
    events.reserve (1024);
    arpEvents.reserve (512);
    keyboardState.reset();
}

//==============================================================================
void RecklessChronosProcessor::noteOn (int note, float vel)
{
    const int v127 = juce::jlimit (1, 127, (int) std::round (vel * 127.0f));
    if (mode.load() == (int) Mode::Program)
    {
        timbres[0].noteOn (note, vel);
        return;
    }
    for (auto& t : timbres)
        if (t.acceptsNote (note, v127))
            t.noteOn (note, vel);
}

void RecklessChronosProcessor::noteOff (int note)
{
    for (auto& t : timbres) t.noteOff (note);
}

void RecklessChronosProcessor::handleEvent (const Event& e)
{
    switch (e.type)
    {
        case 0: noteOn (e.a, e.b); break;
        case 1: noteOff (e.a); break;
        case 2:
            if (e.a == 1) midiMod = e.b;
            else if (e.a == 64)
            {
                sustain = e.b >= 0.5f;
                for (auto& t : timbres) t.setSustain (sustain);
            }
            else if (e.a == 120 || e.a == 123)
                for (auto& t : timbres) t.allNotesOff (e.a == 120);
            break;
        case 3: midiBend = e.b; break;
        case 4: midiAT = e.b; break;
        default: break;
    }
}

float RecklessChronosProcessor::vectorGain (int t) const
{
    if (mode.load() != (int) Mode::Combi || timbres[(size_t) t].params[tpVector] < 0.5f)
        return 1.0f;
    const float x = graw[gpVecX]->load(), y = graw[gpVecY]->load();
    static const float cx[4] = { -1.0f, 0.0f, 1.0f, 0.0f }, cy[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
    float w[4], sum2 = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float d = std::sqrt ((x - cx[i]) * (x - cx[i]) + (y - cy[i]) * (y - cy[i]));
        w[i] = std::max (0.0f, 1.0f - d / 1.4142f);
        sum2 += w[i] * w[i];
    }
    return sum2 > 0.0f ? w[t] / std::sqrt (sum2) : 0.5f;
}

void RecklessChronosProcessor::renderSegment (int start, int n)
{
    RenderCtx ctx;
    ctx.sr = (float) sampleRate;
    const float ub = uiBend.load();
    ctx.bendSemis = std::abs (ub) > 0.001f ? ub : midiBend;
    ctx.modWheel = std::max (midiMod, uiMod.load());
    ctx.aftertouch = midiAT;
    ctx.timeSec = timeSec;

    float* outL = mixBuf.getWritePointer (0) + start;
    float* outR = mixBuf.getWritePointer (1) + start;
    float* dL = dlySend.getWritePointer (0) + start;
    float* dR = dlySend.getWritePointer (1) + start;
    float* rL = revSend.getWritePointer (0) + start;
    float* rR = revSend.getWritePointer (1) + start;

    const bool combi = mode.load() == (int) Mode::Combi;
    for (int t = 0; t < kNumTimbres; ++t)
    {
        auto& tb = timbres[(size_t) t];
        const bool on = (t == 0 || combi) && tb.isOn();
        if (! on && ! tb.hasActiveVoices())
            continue;
        tb.render (n, ctx);
        const float* p = tb.params;
        const float target = p[tpLevel] * vectorGain (t);
        const float pan = p[tpPan];
        const float pl = std::min (1.0f, 1.0f - pan), pr = std::min (1.0f, 1.0f + pan);
        const float s1 = p[tpSend1], s2 = p[tpSend2];
        const float* L = tb.getL();
        const float* R = tb.getR();
        float g = smoothedGain[t];
        const float k = 1.0f - std::exp (-1.0f / (0.01f * (float) sampleRate));
        for (int i = 0; i < n; ++i)
        {
            g += (target - g) * k;
            const float l = L[i] * g * pl, r = R[i] * g * pr;
            outL[i] += l; outR[i] += r;
            dL[i] += l * s1; dR[i] += r * s1;
            rL[i] += l * s2; rR[i] += r * s2;
        }
        smoothedGain[t] = g;
    }
    timeSec += (double) n / sampleRate;
}

void RecklessChronosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n > mixBuf.getNumSamples())
    {
        // host exceeded the announced block size: process in chunks
        for (int pos = 0; pos < n; pos += mixBuf.getNumSamples())
        {
            const int len = std::min (mixBuf.getNumSamples(), n - pos);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, len);
            juce::MidiBuffer subMidi;
            subMidi.addEvents (midi, pos, len, -pos);
            processBlock (sub, subMidi);
        }
        midi.clear();
        return;
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    // realtime knob offsets (+ vector joystick in Program mode)
    float rt[8];
    for (int i = 0; i < 8; ++i) rt[i] = graw[(size_t) (gpRt1 + i)]->load();
    if (mode.load() == (int) Mode::Program)
    {
        rt[0] += graw[gpVecX]->load() * 0.5f;
        rt[2] += graw[gpVecY]->load() * 0.5f;
    }
    for (int t = 0; t < kNumTimbres; ++t)
        timbres[(size_t) t].updateParams (raw[(size_t) t], rt);

    if (panicRequested.exchange (false))
    {
        for (auto& t : timbres) t.allNotesOff (true);
        arp.reset();
    }

    // gather events
    events.clear();
    const bool arpOn = graw[gpArpOn]->load() > 0.5f;
    Arpeggiator::Settings as;
    as.mode = (int) graw[gpArpMode]->load();
    as.rate = (int) graw[gpArpRate]->load();
    as.octaves = (int) graw[gpArpOct]->load();
    as.gate = graw[gpArpGate]->load();
    as.swing = graw[gpArpSwing]->load();
    as.latch = graw[gpArpLatch]->load() > 0.5f;

    arpEvents.clear();
    if (arpWasOn && ! arpOn)
    {
        arp.allOff (0, arpEvents);
        arp.reset();
    }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, n - 1, meta.samplePosition);
        if (m.isNoteOn())
        {
            if (arpOn) arp.noteOn (m.getNoteNumber(), m.getFloatVelocity(), as.latch);
            else events.push_back ({ pos, 0, m.getNoteNumber(), m.getFloatVelocity() });
        }
        else if (m.isNoteOff())
        {
            if (arpOn) arp.noteOff (m.getNoteNumber(), as.latch);
            else events.push_back ({ pos, 1, m.getNoteNumber(), 0.0f });
        }
        else if (m.isController())
            events.push_back ({ pos, 2, m.getControllerNumber(), (float) m.getControllerValue() / 127.0f });
        else if (m.isPitchWheel())
            events.push_back ({ pos, 3, 0, ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f });
        else if (m.isChannelPressure())
            events.push_back ({ pos, 4, 0, (float) m.getChannelPressureValue() / 127.0f });
        else if (m.isAftertouch())
            events.push_back ({ pos, 4, 0, (float) m.getAfterTouchValue() / 127.0f });
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            events.push_back ({ pos, 2, m.isAllSoundOff() ? 120 : 123, 0.0f });
    }
    if (arpOn) arp.process (n, bpm, as, arpEvents);
    arpWasOn = arpOn;
    for (auto& ae : arpEvents)
        events.push_back ({ ae.pos, ae.on ? 0 : 1, ae.note, ae.vel });
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.pos < b.pos; });

    // render
    mixBuf.clear(); dlySend.clear(); revSend.clear();
    int cursor = 0;
    for (auto& e : events)
    {
        if (e.pos > cursor)
        {
            renderSegment (cursor, e.pos - cursor);
            cursor = e.pos;
        }
        handleEvent (e);
    }
    if (cursor < n) renderSegment (cursor, n - cursor);

    // master effects
    static const double syncBeats[] = { 0.0, 1.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 1.5 };
    const int sync = (int) graw[gpDlySync]->load();
    const float dlyMs = sync > 0 ? (float) (60000.0 / std::max (20.0, bpm) * syncBeats[std::clamp (sync, 0, 6)])
                                 : graw[gpDlyTime]->load();
    float* fL = fxOut.getWritePointer (0);
    float* fR = fxOut.getWritePointer (1);
    float* oL = mixBuf.getWritePointer (0);
    float* oR = mixBuf.getWritePointer (1);

    delay.process (dlySend.getReadPointer (0), dlySend.getReadPointer (1), fL, fR, n, std::min (dlyMs, 2400.0f),
                   graw[gpDlyFb]->load(), graw[gpDlyTone]->load(), graw[gpDlyPing]->load());
    const float dRet = graw[gpDlyRet]->load();
    for (int i = 0; i < n; ++i)
    {
        oL[i] += fL[i] * dRet; oR[i] += fR[i] * dRet;
        // delay output also feeds the reverb a little
        revSend.getWritePointer (0)[i] += fL[i] * dRet * 0.3f;
        revSend.getWritePointer (1)[i] += fR[i] * dRet * 0.3f;
    }
    reverb.process (revSend.getReadPointer (0), revSend.getReadPointer (1), fL, fR, n,
                    graw[gpRevSize]->load(), graw[gpRevDamp]->load(), graw[gpRevPre]->load());
    const float rRet = graw[gpRevRet]->load();
    const float master = graw[gpMaster]->load();
    for (int i = 0; i < n; ++i)
    {
        oL[i] = (oL[i] + fL[i] * rRet) * master;
        oR[i] = (oR[i] + fR[i] * rRet) * master;
    }
    limiter.process (oL, oR, n);

    // output
    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, mixBuf, 0, 0, n);
        buffer.copyFrom (1, 0, mixBuf, 1, 0, n);
        for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, mixBuf.getReadPointer (0), n, 0.5f);
        buffer.addFrom (0, 0, mixBuf.getReadPointer (1), n, 0.5f);
    }

    int vc = 0;
    for (auto& t : timbres) vc += t.activeVoiceCount();
    voiceCount.store (vc);
    meterL.store (std::max (meterL.load() * 0.9f, mixBuf.getMagnitude (0, 0, n)));
    meterR.store (std::max (meterR.load() * 0.9f, mixBuf.getMagnitude (1, 0, n)));
    midi.clear();
}

//==============================================================================
void RecklessChronosProcessor::loadProgram (int index)
{
    currentProgramIndex = index;
    presets.applyProgram (index);
}

void RecklessChronosProcessor::loadCombi (int index)
{
    presets.applyCombi (index);
}

void RecklessChronosProcessor::setMode (Mode m)
{
    mode.store ((int) m);
    panic();
}

int RecklessChronosProcessor::getNumPrograms()
{
    int n = 0;
    for (auto& p : presets.programs()) if (! p.user) ++n;
    return std::max (1, n);
}

void RecklessChronosProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < (int) presets.programs().size())
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) loadProgram (index);
        else juce::MessageManager::callAsync ([this, index] { loadProgram (index); });
    }
}

const juce::String RecklessChronosProcessor::getProgramName (int index)
{
    if (index >= 0 && index < (int) presets.programs().size())
        return presets.programs()[(size_t) index].name;
    return {};
}

void RecklessChronosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("mode", mode.load(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("programIndex", currentProgramIndex, nullptr);
    for (int t = 0; t < kNumTimbres; ++t)
        state.setProperty ("timbreName" + juce::String (t), presets.timbreNames[(size_t) t], nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessChronosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);
            mode.store ((int) state.getProperty ("mode", 0));
            currentPresetName = state.getProperty ("presetName", "Init Program").toString();
            uiScale = (float) (double) state.getProperty ("uiScale", 0.8);
            currentProgramIndex = (int) state.getProperty ("programIndex", 0);
            for (int t = 0; t < kNumTimbres; ++t)
                presets.timbreNames[(size_t) t] = state.getProperty ("timbreName" + juce::String (t), "---").toString();
            panic();
            if (onPresetChanged)
                juce::MessageManager::callAsync ([this] { if (onPresetChanged) onPresetChanged(); });
        }
    }
}

juce::AudioProcessorEditor* RecklessChronosProcessor::createEditor()
{
    return new RecklessChronosEditor (*this);
}

#ifndef RC2_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessChronosProcessor();
}
#endif
