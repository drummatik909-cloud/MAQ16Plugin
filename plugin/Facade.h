#pragma once
// Façade du MAQ 16/3 : image de la Dark Edition + 48 potards, LED, afficheur, 8 boutons, encodeur DATA,
// et une barre de contrôle (VAL/NOTE, presets, import/export, dossiers) sous la façade.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "PluginProcessor.h"

namespace maqui {

constexpr int kWidth = 1369;    // taille de l'image de la façade
constexpr int kHeight = 502;
constexpr int kBarHeight = 76;  // barre de contrôle sous la façade
constexpr int kTotalHeight = kHeight + kBarHeight;

// Dessine le capuchon argenté des potards par-dessus celui de la photo.
class MaqLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

// Zone cliquable invisible posée sur un bouton de la photo.
class Hotspot : public juce::Component {
public:
    std::function<void()> onClick;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void paint(juce::Graphics&) override;

private:
    bool down_ = false;
};

// Encodeur DATA : glisser verticalement ou molette.
class DataEncoder : public juce::Component {
public:
    std::function<void(int)> onDelta;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void step(int direction);
    float angle_ = 0.0f;
    float dragAcc_ = 0.0f;
    float wheelAcc_ = 0.0f;
    int lastY_ = 0;
};

class Facade : public juce::Component, private juce::Timer {
public:
    explicit Facade(MaqProcessor&);
    ~Facade() override;
    void paint(juce::Graphics&) override;

private:
    enum class Menu { None, Event, Channel, FirstLast, Prescale, Mode, SingleStep, Preset };

    void timerCallback() override;
    void pressMenuButton(int index);
    void dataChanged(int delta);
    void toggleMuteOfSelectedStep();
    void pressEnter();  // START/STOP dans le menu PRESET
    void flash(const std::string& threeChars);

    int getInt(const juce::String& id) const;
    bool getBool(const juce::String& id) const;
    void bump(const juce::String& id, int delta);

    maq::EventConfig eventOf(int row) const;
    std::string knobSegText(int row, int step) const;
    bool isRowMenu() const;
    int ledLevelFor(int row, int step) const;
    bool menuLedOn(int index) const;
    std::string computeDisplay() const;

    static juce::Rectangle<int> stepLedRect(int row, int step);
    static juce::Rectangle<int> menuLedRect(int index);
    static juce::Rectangle<int> displayRect();

    MaqProcessor& proc_;
    juce::AudioProcessorValueTreeState& apvts_;
    MaqLookAndFeel lnf_;
    juce::Image background_;

    std::array<std::unique_ptr<juce::Slider>, maq::kRows * maq::kSteps> knobs_;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments_;  // détruits avant les potards
    std::array<Hotspot, 8> buttons_;
    DataEncoder encoder_;

    // État de l'interface (non sauvegardé)
    Menu menu_ = Menu::None;
    int row_ = 0;
    int sub_ = 0;           // 0..5 pour FIRST/LAST et PRESCALE (3 premiers : F ou g ; 3 suivants : L ou n)
    bool ccEdit_ = false;   // EVENT : DATA règle le numéro du contrôleur libre
    int singleRow_ = -1;    // SINGLE STEP : -1 = global, 0..2 = une rangée
    int selStep_ = 0;       // SINGLE STEP : pas sélectionné
    bool storeMode_ = false;  // PRESET : second appui = enregistrement (les 8 LED s'allument)

    std::string msgText_;           // message de 3 caractères affiché brièvement
    juce::uint32 msgUntil_ = 0;
    int touchRow_ = -1;             // dernier potard touché : sa valeur s'affiche un instant
    int touchStep_ = -1;
    juce::uint32 touchUntil_ = 0;

    // Ce qui est affiché (pour ne redessiner que ce qui change)
    std::array<int, maq::kRows * maq::kSteps> shownLed_{};
    std::array<bool, 8> shownMenuLed_{};
    std::string shownText_ = "   ";
};

// Barre sous la façade : noms de notes, presets, import/export, dossiers.
class ControlBar : public juce::Component, private juce::Timer {
public:
    explicit ControlBar(MaqProcessor&);
    ~ControlBar() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int selectedSlot() const;
    void bumpPreset(int delta);
    void setStatus(const juce::String& text);
    void doExport();
    void doImport();
    void chooseFolder(maq::FolderPurpose purpose, const juce::String& title);

    MaqProcessor& proc_;
    juce::LookAndFeel_V4 lnf_;

    juce::TextButton notesBtn_, frBtn_, prevBtn_, nextBtn_, storeBtn_, recallBtn_;
    juce::TextButton importBtn_, exportBtn_, importDirBtn_, exportDirBtn_;
    juce::ToggleButton keepSameBtn_;
    juce::Label titleLabel_, presetLabel_, statusLabel_;
    juce::TextEditor nameEditor_;
    std::unique_ptr<juce::ButtonParameterAttachment> notesAttach_, frAttach_;
    std::unique_ptr<juce::FileChooser> chooser_;

    int shownSlot_ = -1;
    bool shownUsed_ = false;
    juce::String shownName_;
};

class MaqEditor : public juce::AudioProcessorEditor {
public:
    explicit MaqEditor(MaqProcessor&);
    void paint(juce::Graphics&) override {}
    void resized() override;

private:
    juce::Component content_;  // façade + barre, mises à l'échelle ensemble
    Facade facade_;
    ControlBar bar_;
};

}  // namespace maqui

using MaqEditor = maqui::MaqEditor;
