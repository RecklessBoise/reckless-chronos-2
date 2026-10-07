#pragma once

#include "Widgets.h"

class RecklessChronosProcessor;

namespace rc::ui
{
class PlayPage;
class EditPage;
class MixerPage;
class FxPage;
class ArpPage;

// The central colour "touch screen"
class Screen : public juce::Component, private juce::Timer
{
public:
    explicit Screen (RecklessChronosProcessor&);
    ~Screen() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    void refreshAll();
    void stepPreset (int delta);
    void showCategory (int cat);
    void setEditTimbre (int t);
    int getEditTimbre() const;
    void showPage (int index);
    void openSaveDialog();

    std::function<void()> onEditTimbreChanged;

private:
    void timerCallback() override;

    RecklessChronosProcessor& proc;
    std::unique_ptr<PlayPage> play;
    std::unique_ptr<EditPage> edit;
    std::unique_ptr<MixerPage> mixer;
    std::unique_ptr<FxPage> fx;
    std::unique_ptr<ArpPage> arp;
    juce::OwnedArray<juce::TextButton> tabs, timbreButtons;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    int page = 0;
    juce::String shownName;
    int shownMode = -1, shownVoices = -1;
    juce::Rectangle<int> headerArea, nameArea, tabArea, contentArea;
    juce::Rectangle<float> heartArea;
    int favPoll = 0;
    std::unique_ptr<juce::AlertWindow> saveWindow;
};
} // namespace rc::ui
