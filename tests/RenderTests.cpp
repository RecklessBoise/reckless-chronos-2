// Offline validation for Reckless Chronos 2.
//   RC2Tests                 -> renders every factory Program and Combi, checks for silence / NaN / clipping
//   RC2Tests --wav <dir>     -> also writes one demo .wav per Program into <dir>
//   RC2Tests --host <plugin> -> loads a built .vst3 / .component through the JUCE hosting layer and renders it
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
struct Stats
{
    float peak = 0.0f;
    double sumSq = 0.0;
    long count = 0;
    bool finite = true;
    void add (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getReadPointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                if (! std::isfinite (d[i])) finite = false;
                peak = std::max (peak, std::abs (d[i]));
                sumSq += (double) d[i] * d[i];
                ++count;
            }
        }
    }
    double rms() const { return count ? std::sqrt (sumSq / (double) count) : 0.0; }
};

constexpr double kSr = 48000.0;
std::vector<juce::String> loudnessRows; // "kind,name,rms,peak"
constexpr int kBlock = 256;

void renderSeconds (juce::AudioProcessor& p, double secs, juce::MidiBuffer& firstBlockMidi, Stats& st,
                    juce::AudioBuffer<float>* capture, int& captureOffset)
{
    juce::AudioBuffer<float> buf (2, kBlock);
    const int blocks = (int) std::ceil (secs * kSr / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (b == 0) midi = firstBlockMidi;
        p.processBlock (buf, midi);
        st.add (buf);
        if (capture != nullptr && captureOffset + kBlock <= capture->getNumSamples())
        {
            for (int c = 0; c < 2; ++c) capture->copyFrom (c, captureOffset, buf, c, 0, kBlock);
            captureOffset += kBlock;
        }
    }
}

std::vector<int> testNotes (RecklessChronosProcessor& p, bool combi)
{
    const int eng = (int) p.apvts.getRawParameterValue ("t1_engine")->load();
    if (! combi && eng == rc::engDrums) return { 36, 38, 42, 46, 49 };
    return { 48, 60, 64, 67 };
}

bool playOne (RecklessChronosProcessor& p, const juce::String& label, bool combi, juce::File wavDir, double& cpuMs)
{
    // let parameters settle and clear tails
    p.panic();
    juce::MidiBuffer none;
    Stats warm;
    int off = 0;
    renderSeconds (p, 0.05, none, warm, nullptr, off);

    const auto notes = testNotes (p, combi);
    juce::MidiBuffer on, offMidi;
    for (int n : notes) on.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    for (int n : notes) offMidi.addEvent (juce::MidiMessage::noteOff (1, n), 0);

    juce::AudioBuffer<float> capture (2, (int) (kSr * 3.0));
    capture.clear();
    off = 0;
    auto* cap = wavDir.isDirectory() ? &capture : nullptr;

    Stats held, tail;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    renderSeconds (p, 1.2, on, held, cap, off);
    renderSeconds (p, 1.5, offMidi, tail, cap, off);
    cpuMs = juce::Time::getMillisecondCounterHiRes() - t0;

    bool ok = true;
    juce::String why;
    if (! held.finite || ! tail.finite) { ok = false; why << " NaN/Inf"; }
    if (held.rms() < 1.0e-3) { ok = false; why << " too quiet (rms " << held.rms() << ")"; }
    if (held.peak > 1.0f) { ok = false; why << " peak " << held.peak; }
    if (! ok)
        std::cout << "  FAIL " << label << ":" << why << std::endl;
    loudnessRows.push_back (juce::String (combi ? "C" : "P") + "," + label.fromFirstOccurrenceOf (" ", false, false)
                            + "," + juce::String (held.rms(), 5) + "," + juce::String (held.peak, 4));

    if (cap != nullptr)
    {
        auto f = wavDir.getChildFile (juce::File::createLegalFileName (label) + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (f.createOutputStream().release());
        const auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (kSr).withNumChannels (2).withBitsPerSample (24);
        if (stream != nullptr)
            if (auto w = wav.createWriterFor (stream, opts))
                w->writeFromAudioSampleBuffer (capture, 0, off);
    }
    return ok;
}

// Estimates the pitch error (cents, folded to the nearest octave of 220 Hz) of a single A3 note.
double pitchErrorCents (RecklessChronosProcessor& p)
{
    p.panic();
    juce::MidiBuffer none, on;
    Stats st;
    int off = 0;
    renderSeconds (p, 0.05, none, st, nullptr, off);
    on.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> cap (2, (int) (kSr * 0.6));
    cap.clear();
    off = 0;
    renderSeconds (p, 0.6, on, st, &cap, off);
    juce::MidiBuffer offM;
    offM.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
    renderSeconds (p, 0.3, offM, st, nullptr, off);

    // analyse 0.2 .. 0.5 s, mono
    const int start = (int) (kSr * 0.2), len = (int) (kSr * 0.3);
    std::vector<double> x ((size_t) len);
    for (int i = 0; i < len; ++i) x[(size_t) i] = cap.getSample (0, start + i) + cap.getSample (1, start + i);
    const int minLag = (int) (kSr / 1000.0), maxLag = (int) (kSr / 50.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double a = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < len; i += 2)
        {
            a += x[(size_t) i] * x[(size_t) (i + lag)];
            e1 += x[(size_t) i] * x[(size_t) i];
            e2 += x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
        }
        ac[(size_t) lag] = a / std::sqrt (e1 * e2 + 1e-20);
    }
    double best = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, ac[(size_t) lag]);
    int lag = minLag;
    for (int l = minLag + 1; l <= maxLag; ++l)
        if (ac[(size_t) l] > 0.9 * best && ac[(size_t) l] >= ac[(size_t) (l - 1)] && ac[(size_t) l] >= ac[(size_t) (l + 1)]) { lag = l; break; }
    // parabolic interpolation
    const double y0 = ac[(size_t) (lag - 1)], y1 = ac[(size_t) lag], y2 = ac[(size_t) (lag + 1)];
    const double d = (y0 - y2) / (2.0 * (y0 - 2.0 * y1 + y2) + 1e-12);
    const double f = kSr / ((double) lag + d);
    double cents = 1200.0 * std::log2 (f / 220.0);
    cents = std::fmod (cents + 600.0 + 1200.0 * 100.0, 1200.0) - 600.0;
    return cents;
}

// Renders the editor to PNG files: <prefix>_<page>.png  (optional page list "0,1,2")
int snapshot (const juce::String& prefix, const juce::String& what)
{
    auto procPtr = std::make_unique<RecklessChronosProcessor>();
    auto& p = *procPtr;
    p.setPlayConfigDetails (0, 2, kSr, kBlock);
    p.prepareToPlay (kSr, kBlock);
    if (what.startsWith ("combi") && ! p.presets.combis().empty()) p.loadCombi (0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1680, 700);
    auto save = [&] (const juce::String& name)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile (prefix + "_" + name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
        std::cout << "wrote " << f.getFullPathName() << std::endl;
    };
    // walk the screen tabs by clicking the buttons by name
    std::function<juce::Button* (juce::Component*, const juce::String&)> findButton = [&] (juce::Component* c, const juce::String& text) -> juce::Button*
    {
        for (auto* ch : c->getChildren())
        {
            if (auto* b = dynamic_cast<juce::Button*> (ch))
                if (b->getButtonText() == text && b->isVisible() && (b->getParentComponent() == nullptr || b->getParentComponent()->isVisible())) return b;
            if (auto* r = findButton (ch, text)) return r;
        }
        return nullptr;
    };
    save ("play");
    for (auto* tab : { "EDIT", "MIXER", "FX", "ARP" })
        if (auto* b = findButton (ed.get(), tab)) { b->triggerClick(); save (juce::String (tab).toLowerCase()); }
    if (auto* e = findButton (ed.get(), "EDIT")) { e->triggerClick(); juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
    if (auto* b = findButton (ed.get(), "FILTER / AMP")) { b->triggerClick(); save ("edit_filter"); }
    if (auto* b = findButton (ed.get(), "LFO / PITCH")) { b->triggerClick(); save ("edit_lfo"); }
    if (auto* pl = findButton (ed.get(), "PLAY")) { pl->triggerClick(); juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
    if (auto* k = findButton (ed.get(), "KEYS")) { k->triggerClick(); save ("list"); }
    ed->setSize (840, 350);
    save ("small");
    return 0;
}

int hostTest (const juce::String& path)
{
    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats())
        if (f->fileMightContainThisPluginType (path))
            f->findAllTypesForFile (found, path);
    if (found.isEmpty()) { std::cout << "No plugin found in " << path << std::endl; return 1; }

    juce::String err;
    auto inst = fm.createPluginInstance (*found[0], kSr, kBlock, err);
    if (inst == nullptr) { std::cout << "Load failed: " << err << std::endl; return 1; }
    std::cout << "Loaded " << inst->getName() << " (" << found[0]->pluginFormatName << "), "
              << inst->getParameters().size() << " params, " << inst->getNumPrograms() << " programs" << std::endl;
    inst->setPlayConfigDetails (0, 2, kSr, kBlock);
    inst->prepareToPlay (kSr, kBlock);

    int failures = 0;
    const int progs = std::min (inst->getNumPrograms(), 40);
    for (int i = 0; i < progs; i += 7)
    {
        inst->setCurrentProgram (i);
        // allow async program change on the message thread
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        juce::MidiBuffer on;
        on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        Stats st;
        juce::AudioBuffer<float> buf (2, kBlock);
        for (int b = 0; b < 200; ++b)
        {
            buf.clear();
            juce::MidiBuffer m;
            if (b == 0) m = on;
            if (b == 120) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            inst->processBlock (buf, m);
            st.add (buf);
        }
        const bool ok = st.finite && st.rms() > 1.0e-4;
        std::cout << "  program " << i << " '" << inst->getProgramName (i) << "' rms " << st.rms()
                  << (ok ? " ok" : " FAIL") << std::endl;
        failures += ok ? 0 : 1;
    }

    // state round trip
    juce::MemoryBlock state;
    inst->getStateInformation (state);
    inst->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "  state size " << state.getSize() << " bytes" << std::endl;

    if (inst->hasEditor())
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
        std::cout << "  editor " << (ed ? juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()) : "none") << std::endl;
        ed.reset();
    }
    inst->releaseResources();
    std::cout << (failures == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
} // namespace

static int runTests (int argc, char** argv);

int main (int argc, char** argv)
{
    std::cout << "RC2Tests " << juce::SystemStats::getOperatingSystemName() << std::endl;
    try
    {
        return runTests (argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "UNKNOWN EXCEPTION" << std::endl;
    }
    return 2;
}

static int runTests (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File wavDir, loudnessFile;
    bool checkPitch = false;
    for (int i = 1; i < argc - 1; ++i)
    {
        if (juce::String (argv[i]) == "--host") return hostTest (argv[i + 1]);
        if (juce::String (argv[i]) == "--snapshot") return snapshot (argv[i + 1], i + 2 < argc ? juce::String (argv[i + 2]) : juce::String());
        if (juce::String (argv[i]) == "--loudness")
            loudnessFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        if (juce::String (argv[i]) == "--wav")
        {
            wavDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
            wavDir.createDirectory();
        }
    }

    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--pitch") checkPitch = true;

    auto procPtr = std::make_unique<RecklessChronosProcessor>(); // large object: keep it off the stack
    auto& p = *procPtr;
    p.setPlayConfigDetails (0, 2, kSr, kBlock);
    p.prepareToPlay (kSr, kBlock);
    std::cout << "Loaded " << p.presets.programs().size() << " programs, " << p.presets.combis().size() << " combis" << std::endl;

    int failures = 0;
    for (auto& e : p.presets.parseErrors())
    {
        std::cout << "  PARSE " << e << std::endl;
        ++failures;
    }

    const auto& progs = p.presets.programs();
    const auto& combs = p.presets.combis();
    double worst = 0.0;
    juce::String worstName;
    std::map<juce::String, int> perCat;

    for (int i = 0; i < (int) progs.size(); ++i)
    {
        if (progs[(size_t) i].user) continue;
        p.loadProgram (i);
        double ms = 0;
        if (! playOne (p, "P" + juce::String (i).paddedLeft ('0', 4) + " " + progs[(size_t) i].name, false, wavDir, ms)) ++failures;
        if (ms > worst) { worst = ms; worstName = progs[(size_t) i].name; }
        perCat[progs[(size_t) i].category]++;
        if (checkPitch && (int) p.apvts.getRawParameterValue ("t1_engine")->load() != rc::engDrums)
        {
            const double c = pitchErrorCents (p);
            if (std::abs (c) > 25.0)
                std::cout << "  PITCH " << progs[(size_t) i].name << ": " << c << " cents" << std::endl;
        }
    }
    for (int i = 0; i < (int) combs.size(); ++i)
    {
        if (combs[(size_t) i].user) continue;
        p.loadCombi (i);
        double ms = 0;
        if (! playOne (p, "C" + juce::String (i).paddedLeft ('0', 3) + " " + combs[(size_t) i].name, true, juce::File(), ms)) ++failures;
        if (ms > worst) { worst = ms; worstName = combs[(size_t) i].name; }
    }

    // state round trip
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());

    if (loudnessFile != juce::File())
        loudnessFile.replaceWithText (juce::StringArray (loudnessRows.data(), (int) loudnessRows.size()).joinIntoString ("\n"));

    std::cout << "Programs: " << progs.size() << "  Combis: " << combs.size() << std::endl;
    for (auto& [c, n] : perCat) std::cout << "  " << c << ": " << n << std::endl;
    std::cout << "Slowest preset: " << worstName << " (" << worst << " ms for 2.7 s of audio)" << std::endl;
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "FAILURES: " + juce::String (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
