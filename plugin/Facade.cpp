#include "Facade.h"
#include <BinaryData.h>
#include <cmath>
#include <cstdio>

using namespace juce;

namespace maqui {

namespace {

// Positions mesurées sur la photo de la façade (1369 x 502)
constexpr float kKnobX0 = 366.5f;
constexpr float kKnobDX = 57.56f;
constexpr float kKnobY[3] = {141.5f, 282.9f, 424.5f};
constexpr float kCapRadius = 14.5f;

constexpr float kLedX0 = 360.0f;
constexpr float kLedDX = 57.53f;
constexpr int kLedY[3] = {87, 229, 370};

constexpr int kMenuLedX[4] = {138, 174, 210, 245};
constexpr int kMenuLedY[2] = {207, 257};
constexpr int kMenuBtnCx[4] = {145, 181, 217, 252};
constexpr int kMenuBtnCy[2] = {236, 286};

constexpr float kDigitCx[3] = {165.5f, 198.5f, 232.0f};
constexpr float kDigitTop = 78.0f;
constexpr float kDigitW = 20.0f;
constexpr float kDigitH = 37.0f;

constexpr int kEncoderCx = 199;
constexpr int kEncoderCy = 373;

// 0 EVENT, 1 CHN, 2 FIRST/LAST, 3 PRESCALE, 4 MODE, 5 SINGLE STEP, 6 PRESET, 7 START/STOP

uint8 glyphMask(char c) {
    switch (c) {
        case '0': return 0x3F; case '1': return 0x06; case '2': return 0x5B; case '3': return 0x4F;
        case '4': return 0x66; case '5': return 0x6D; case '6': return 0x7D; case '7': return 0x07;
        case '8': return 0x7F; case '9': return 0x6F;
        case 'A': return 0x77; case 'b': return 0x7C; case 'C': return 0x39; case 'c': return 0x58;
        case 'd': return 0x5E; case 'E': return 0x79; case 'F': return 0x71; case 'G': return 0x3D;
        case 'g': return 0x6F; case 'H': return 0x76; case 'I': return 0x30; case 'L': return 0x38;
        case 'n': return 0x54; case 'O': return 0x3F; case 'o': return 0x5C; case 'P': return 0x73;
        case 'r': return 0x50; case 'S': return 0x6D; case 't': return 0x78; case 'U': return 0x3E;
        case 'Y': return 0x6E; case '-': return 0x40;
        default: return 0x00;
    }
}

void paintDigit(Graphics& g, uint8 mask, bool dot, float x, float y, float w, float h) {
    const float t = 3.6f;
    const float half = h * 0.5f;
    const Colour on(0xffff2a1a);
    const Colour off(0x2dff2a1a);
    auto seg = [&](int bit, float sx, float sy, float sw, float sh) {
        const bool lit = (mask & (1 << bit)) != 0;
        if (lit) {
            g.setColour(on.withAlpha(0.28f));
            g.fillRoundedRectangle(sx - 1.2f, sy - 1.2f, sw + 2.4f, sh + 2.4f, 2.0f);
        }
        g.setColour(lit ? on : off);
        g.fillRoundedRectangle(sx, sy, sw, sh, 1.2f);
    };
    seg(0, x + t, y, w - 2 * t, t);                          // a
    seg(1, x + w - t, y + t * 0.8f, t, half - t * 1.4f);      // b
    seg(2, x + w - t, y + half + t * 0.6f, t, half - t * 1.4f);  // c
    seg(3, x + t, y + h - t, w - 2 * t, t);                  // d
    seg(4, x, y + half + t * 0.6f, t, half - t * 1.4f);      // e
    seg(5, x, y + t * 0.8f, t, half - t * 1.4f);             // f
    seg(6, x + t, y + half - t * 0.5f, w - 2 * t, t);        // g

    // point décimal (sert de dièse pour les noms de notes, et d'indicateur « slot utilisé »)
    const float dx = x + w + 3.5f;
    const float dy = y + h - 1.5f;
    if (dot) {
        g.setColour(on.withAlpha(0.28f));
        g.fillEllipse(dx - 3.4f, dy - 3.4f, 6.8f, 6.8f);
    }
    g.setColour(dot ? on : off);
    g.fillEllipse(dx - 2.2f, dy - 2.2f, 4.4f, 4.4f);
}

void paintLed(Graphics& g, Rectangle<int> r, int level) {
    const auto f = r.toFloat();
    if (level >= 2) {
        g.setColour(Colour(0xffff2a14).withAlpha(0.30f));
        g.fillRect(f.expanded(2.0f));
        g.setColour(Colour(0xffff2a14));
    } else if (level == 1) {
        g.setColour(Colour(0xff7a0f0f));
    } else {
        g.setColour(Colour(0xff2b0606));
    }
    g.fillRect(f);
}

// Nombre aligné à droite sur trois caractères
std::string num3(int v) {
    std::string s = std::to_string(v);
    while (s.size() < 3) s = " " + s;
    return s.substr(s.size() - 3);
}

// Nom de note sur trois afficheurs : lettre (+ point = dièse), signe de l'octave, chiffre
std::string segNote(int midi) {
    static const char letters[12] = {'C', 'C', 'd', 'd', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'b'};
    static const bool sharp[12] = {false, true, false, true, false, false, true, false, true, false, true, false};
    const int n = jlimit(0, 127, midi);
    const int octave = n / 12 - 2;
    std::string s(1, letters[n % 12]);
    if (sharp[n % 12]) s += '.';
    s += (octave < 0) ? '-' : ' ';
    s += static_cast<char>('0' + std::abs(octave));
    return s;
}

String fr(const char* utf8) { return String::fromUTF8(utf8); }

}  // namespace

// ---------------------------------------------------------------- Look and feel

void MaqLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int width, int height, float pos,
                                      float startAngle, float endAngle, Slider&) {
    const auto c = Rectangle<int>(x, y, width, height).toFloat().getCentre();
    const float r = kCapRadius;

    ColourGradient grad(Colour(0xffeeeeee), c.x - r * 0.5f, c.y - r * 0.6f,
                        Colour(0xff666666), c.x + r * 0.7f, c.y + r * 0.8f, false);
    g.setGradientFill(grad);
    g.fillEllipse(c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour(Colour(0xff151515));
    g.drawEllipse(c.x - r, c.y - r, 2.0f * r, 2.0f * r, 1.0f);

    const float a = startAngle + pos * (endAngle - startAngle);
    g.setColour(Colour(0xff1a1a1a));
    g.drawLine(c.x + std::sin(a) * r * 0.25f, c.y - std::cos(a) * r * 0.25f,
               c.x + std::sin(a) * r * 0.92f, c.y - std::cos(a) * r * 0.92f, 2.6f);
}

// ---------------------------------------------------------------- Hotspot

void Hotspot::mouseDown(const MouseEvent&) {
    down_ = true;
    repaint();
    if (onClick) onClick();
}

void Hotspot::mouseUp(const MouseEvent&) {
    down_ = false;
    repaint();
}

void Hotspot::paint(Graphics& g) {
    if (down_) {
        g.setColour(Colours::white.withAlpha(0.12f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.0f);
    }
}

// ---------------------------------------------------------------- Encodeur DATA

void DataEncoder::step(int direction) {
    angle_ += 0.25f * static_cast<float>(direction);
    repaint();
    if (onDelta) onDelta(direction);
}

void DataEncoder::paint(Graphics& g) {
    const auto c = getLocalBounds().toFloat().getCentre();
    g.setColour(Colours::white.withAlpha(0.55f));
    g.drawLine(c.x + std::sin(angle_) * 9.0f, c.y - std::cos(angle_) * 9.0f,
               c.x + std::sin(angle_) * 26.0f, c.y - std::cos(angle_) * 26.0f, 2.2f);
}

void DataEncoder::mouseDown(const MouseEvent& e) {
    lastY_ = e.getPosition().y;
    dragAcc_ = 0.0f;
}

void DataEncoder::mouseDrag(const MouseEvent& e) {
    const int y = e.getPosition().y;
    dragAcc_ += static_cast<float>(lastY_ - y);
    lastY_ = y;
    while (dragAcc_ >= 6.0f) { step(+1); dragAcc_ -= 6.0f; }
    while (dragAcc_ <= -6.0f) { step(-1); dragAcc_ += 6.0f; }
}

void DataEncoder::mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) {
    wheelAcc_ += wheel.deltaY * 12.0f;
    while (wheelAcc_ >= 1.0f) { step(+1); wheelAcc_ -= 1.0f; }
    while (wheelAcc_ <= -1.0f) { step(-1); wheelAcc_ += 1.0f; }
}

// ---------------------------------------------------------------- Façade

Rectangle<int> Facade::stepLedRect(int row, int step) {
    return Rectangle<int>(juce::roundToInt(kLedX0 + kLedDX * static_cast<float>(step)), kLedY[row], 15, 8);
}

Rectangle<int> Facade::menuLedRect(int index) {
    return Rectangle<int>(kMenuLedX[index % 4], kMenuLedY[index / 4], 15, 8);
}

Rectangle<int> Facade::displayRect() { return Rectangle<int>(146, 73, 105, 48); }

Facade::Facade(MaqProcessor& p) : proc_(p), apvts_(p.apvts) {
    background_ = ImageCache::getFromMemory(BinaryData::facade_png, BinaryData::facade_pngSize);
    setOpaque(true);
    setSize(kWidth, kHeight);

    for (int r = 0; r < maq::kRows; ++r) {
        for (int s = 0; s < maq::kSteps; ++s) {
            auto k = std::make_unique<Slider>();
            k->setSliderStyle(Slider::RotaryVerticalDrag);
            k->setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
            // L'échelle imprimée (0 à 10) couvre 288 degrés : de 216 à 504 degrés depuis le haut
            k->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
            k->setMouseDragSensitivity(220);
            k->setLookAndFeel(&lnf_);
            const Point<int> centre(roundToInt(kKnobX0 + kKnobDX * static_cast<float>(s)), roundToInt(kKnobY[r]));
            k->setBounds(Rectangle<int>(36, 36).withCentre(centre));
            addAndMakeVisible(*k);
            if (auto* prm = apvts_.getParameter(MaqProcessor::potId(r, s)))
                attachments_.push_back(std::make_unique<SliderParameterAttachment>(*prm, *k, nullptr));
            k->setDoubleClickReturnValue(true, 0.0);

            // Bulle de valeur pendant le réglage : valeur ou nom de note selon la barre
            k->setPopupDisplayEnabled(true, false, this);
            k->textFromValueFunction = [this, r](double v) {
                return fr(maq::valueText(eventOf(r), static_cast<int>(std::lround(v)),
                                         getBool("valnote"), getBool("notefr")).c_str());
            };
            // Le dernier potard touché affiche sa valeur sur l'afficheur
            Slider* raw = k.get();
            k->onValueChange = [this, r, s, raw] {
                if (raw->isMouseOverOrDragging()) {
                    touchRow_ = r;
                    touchStep_ = s;
                    touchUntil_ = Time::getMillisecondCounter() + 1500;
                }
            };
            knobs_[static_cast<size_t>(r * maq::kSteps + s)] = std::move(k);
        }
    }

    for (int i = 0; i < 8; ++i) {
        buttons_[static_cast<size_t>(i)].setBounds(
            Rectangle<int>(28, 22).withCentre(Point<int>(kMenuBtnCx[i % 4], kMenuBtnCy[i / 4])));
        buttons_[static_cast<size_t>(i)].onClick = [this, i] { pressMenuButton(i); };
        addAndMakeVisible(buttons_[static_cast<size_t>(i)]);
    }

    encoder_.setBounds(Rectangle<int>(66, 66).withCentre(Point<int>(kEncoderCx, kEncoderCy)));
    encoder_.onDelta = [this](int d) { dataChanged(d); };
    addAndMakeVisible(encoder_);

    for (int r = 0; r < maq::kRows; ++r)
        for (int s = 0; s < maq::kSteps; ++s)
            shownLed_[static_cast<size_t>(r * maq::kSteps + s)] = ledLevelFor(r, s);
    for (int i = 0; i < 8; ++i) shownMenuLed_[static_cast<size_t>(i)] = menuLedOn(i);
    shownText_ = computeDisplay();

    startTimerHz(25);
}

Facade::~Facade() {
    stopTimer();
    attachments_.clear();
    for (auto& k : knobs_)
        if (k) {
            k->textFromValueFunction = nullptr;
            k->onValueChange = nullptr;
            k->setLookAndFeel(nullptr);
        }
}

void Facade::paint(Graphics& g) {
    g.setImageResamplingQuality(Graphics::highResamplingQuality);
    g.drawImageAt(background_, 0, 0);

    // La photo étiquette les rangées 2 et 2 : on remplace le deuxième « 2 » par « 3 »
    g.setColour(Colour(0xff0d0d0d));
    g.fillRect(793, 343, 12, 16);
    g.setColour(Colours::white);
    g.setFont(g.getCurrentFont().withHeight(18.0f).boldened());
    g.drawText("3", Rectangle<int>(792, 341, 14, 19), Justification::centred, false);

    // Vitre de l'afficheur : on recouvre le « 300 » de la photo
    g.setColour(Colour(0xff4c4c4c));
    g.fillRect(displayRect());

    // Le texte contient jusqu'à trois caractères ; un « . » après un caractère allume son point décimal
    std::array<char, 3> cells{{' ', ' ', ' '}};
    std::array<bool, 3> dots{{false, false, false}};
    int n = 0;
    for (const char ch : shownText_) {
        if (ch == '.') {
            if (n > 0) dots[static_cast<size_t>(n - 1)] = true;
        } else if (n < 3) {
            cells[static_cast<size_t>(n++)] = ch;
        }
    }
    for (int i = 0; i < 3; ++i)
        paintDigit(g, glyphMask(cells[static_cast<size_t>(i)]), dots[static_cast<size_t>(i)],
                   kDigitCx[i] - kDigitW * 0.5f, kDigitTop, kDigitW, kDigitH);

    for (int r = 0; r < maq::kRows; ++r)
        for (int s = 0; s < maq::kSteps; ++s)
            paintLed(g, stepLedRect(r, s), shownLed_[static_cast<size_t>(r * maq::kSteps + s)]);
    for (int i = 0; i < 8; ++i)
        paintLed(g, menuLedRect(i), shownMenuLed_[static_cast<size_t>(i)] ? 2 : 0);
}

int Facade::getInt(const String& id) const {
    if (auto* v = apvts_.getRawParameterValue(id)) return static_cast<int>(std::lround(v->load()));
    return 0;
}

bool Facade::getBool(const String& id) const {
    if (auto* v = apvts_.getRawParameterValue(id)) return v->load() > 0.5f;
    return false;
}

void Facade::bump(const String& id, int delta) {
    if (auto* p = dynamic_cast<AudioParameterInt*>(apvts_.getParameter(id))) {
        const auto range = p->getRange();
        const int v = jlimit(range.getStart(), range.getEnd(), p->get() + delta);
        if (v != p->get()) {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
    }
}

maq::EventConfig Facade::eventOf(int row) const {
    maq::EventConfig e = maq::eventForIndex(getInt(MaqProcessor::rowId("event", row)));
    if (e.kind == maq::EventKind::FreeCC) e.ccNumber = getInt(MaqProcessor::rowId("cc", row));
    return e;
}

// Valeur ou nom de note d'un potard, sur l'afficheur à sept segments
std::string Facade::knobSegText(int row, int step) const {
    const maq::EventConfig e = eventOf(row);
    if (e.kind == maq::EventKind::Off) return "---";
    const int pot = getInt(MaqProcessor::potId(row, step));
    if (maq::isPauseValue(e, pot)) return "---";
    const int v = maq::displayValue(e, pot);
    if (getBool("valnote") && maq::isAbsoluteNoteEvent(e)) return segNote(v);
    return num3(v);
}

bool Facade::isRowMenu() const {
    return menu_ == Menu::Event || menu_ == Menu::Channel || menu_ == Menu::FirstLast ||
           menu_ == Menu::Prescale || menu_ == Menu::Mode;
}

void Facade::flash(const std::string& threeChars) {
    msgText_ = threeChars;
    msgUntil_ = Time::getMillisecondCounter() + 1200;
}

void Facade::pressEnter() {
    const int sel = getInt("preset");
    if (storeMode_) {
        if (sel >= 1) {
            proc_.storePreset(sel);
            flash("StO");
        } else {
            flash("---");
        }
        storeMode_ = false;
    } else {
        flash((sel >= 1 && proc_.recallPreset(sel)) ? "rEC" : "---");
    }
}

void Facade::pressMenuButton(int index) {
    static const Menu menuOf[8] = {Menu::Event, Menu::Channel, Menu::FirstLast, Menu::Prescale,
                                   Menu::Mode, Menu::SingleStep, Menu::Preset, Menu::None};

    if (index == 7) {  // START/STOP
        if (menu_ == Menu::SingleStep) {
            toggleMuteOfSelectedStep();
        } else if (menu_ == Menu::Preset) {
            pressEnter();  // « Enter » dans le menu PRESET
        } else {
            // Le transport est piloté par Bitwig : ce bouton ramène à l'affichage du tempo
            menu_ = Menu::None;
            ccEdit_ = false;
            singleRow_ = -1;
        }
        return;
    }

    if (index == 6 && menu_ == Menu::Event) {  // PRESET dans EVENT : numéro du contrôleur libre
        if (getInt(MaqProcessor::rowId("event", row_)) == maq::kFreeCcEventIndex) ccEdit_ = !ccEdit_;
        return;
    }

    const Menu m = menuOf[index];
    if (m == menu_) {
        switch (m) {
            case Menu::Event:
            case Menu::Channel:
            case Menu::Mode:
                row_ = (row_ + 1) % maq::kRows;
                ccEdit_ = false;
                break;
            case Menu::FirstLast:
            case Menu::Prescale:
                sub_ = (sub_ + 1) % 6;
                row_ = sub_ % 3;
                break;
            case Menu::SingleStep:
                singleRow_ = (singleRow_ < 0) ? 0 : (singleRow_ + 1) % maq::kRows;
                break;
            case Menu::Preset:
                storeMode_ = !storeMode_;  // second appui : enregistrement
                break;
            default:
                break;
        }
    } else {
        menu_ = m;
        row_ = 0;
        sub_ = 0;
        ccEdit_ = false;
        singleRow_ = -1;
        storeMode_ = false;
    }
}

void Facade::dataChanged(int delta) {
    switch (menu_) {
        case Menu::Event:
            bump(MaqProcessor::rowId(ccEdit_ ? "cc" : "event", row_), delta);
            break;
        case Menu::Channel:
            bump(MaqProcessor::rowId("chan", row_), delta);
            break;
        case Menu::FirstLast:
            bump(MaqProcessor::rowId(sub_ < 3 ? "first" : "last", row_), delta);
            break;
        case Menu::Prescale:
            bump(MaqProcessor::rowId(sub_ < 3 ? "gate" : "notelen", row_), delta);
            break;
        case Menu::Mode:
            bump(MaqProcessor::rowId("mode", row_), delta);
            break;
        case Menu::SingleStep:
            selStep_ = jlimit(0, maq::kSteps - 1, selStep_ + delta);
            break;
        case Menu::Preset:
            bump("preset", delta);
            break;
        default:
            break;
    }
}

void Facade::toggleMuteOfSelectedStep() {
    if (singleRow_ < 0) return;  // le manuel ne mute qu'en mode une rangée
    if (auto* p = dynamic_cast<AudioParameterBool*>(apvts_.getParameter(MaqProcessor::muteId(singleRow_, selStep_)))) {
        p->beginChangeGesture();
        *p = !p->get();
        p->endChangeGesture();
    }
}

int Facade::ledLevelFor(int r, int s) const {
    if (getBool(MaqProcessor::muteId(r, s))) return 0;  // un pas muet reste éteint
    if (menu_ == Menu::SingleStep) {
        if (singleRow_ < 0) return s == selStep_ ? 2 : 0;
        if (r == singleRow_) return s == selStep_ ? 2 : 1;
        return 0;
    }
    int level = 0;
    if (isRowMenu() && r == row_) level = 1;  // la rangée choisie « miroite »
    if (proc_.uiPlaying.load() && proc_.uiStep[static_cast<size_t>(r)].load() == s + 1) level = 2;
    return level;
}

bool Facade::menuLedOn(int index) const {
    if (menu_ == Menu::Preset && storeMode_) return true;  // avertissement d'écriture : les 8 LED
    switch (index) {
        case 0: return menu_ == Menu::Event;
        case 1: return menu_ == Menu::Channel;
        case 2: return menu_ == Menu::FirstLast;
        case 3: return menu_ == Menu::Prescale;
        case 4: return menu_ == Menu::Mode;
        case 5: return menu_ == Menu::SingleStep;
        case 6: return menu_ == Menu::Preset;
        case 7: return menu_ == Menu::Preset || proc_.uiPlaying.load();
        default: return false;
    }
}

std::string Facade::computeDisplay() const {
    const uint32 now = Time::getMillisecondCounter();
    if (!msgText_.empty() && now < msgUntil_) return msgText_;

    auto two = [](char c, int v) {
        char b[16];
        std::snprintf(b, sizeof b, "%c%02d", c, v);
        return std::string(b);
    };
    auto pad = [](std::string s) {
        while (s.size() < 3) s += ' ';
        return s.substr(0, 3);
    };

    switch (menu_) {
        case Menu::None: {
            if (touchRow_ >= 0 && now < touchUntil_) return knobSegText(touchRow_, touchStep_);
            const double bpm = proc_.uiBpm.load();
            return bpm > 0.0 ? num3(static_cast<int>(std::lround(bpm))) : std::string("---");
        }
        case Menu::Event: {
            const int ev = getInt(MaqProcessor::rowId("event", row_));
            if (ccEdit_ || ev == maq::kFreeCcEventIndex) return num3(getInt(MaqProcessor::rowId("cc", row_)));
            return pad(maq::eventLabel(ev));
        }
        case Menu::Channel:
            return num3(getInt(MaqProcessor::rowId("chan", row_)));
        case Menu::FirstLast:
            return sub_ < 3 ? two('F', getInt(MaqProcessor::rowId("first", row_)))
                            : two('L', getInt(MaqProcessor::rowId("last", row_)));
        case Menu::Prescale:
            return sub_ < 3 ? two('g', getInt(MaqProcessor::rowId("gate", row_)))
                            : two('n', getInt(MaqProcessor::rowId("notelen", row_)));
        case Menu::Mode:
            return std::string(maq::modeLabel(getInt(MaqProcessor::rowId("mode", row_))));
        case Menu::SingleStep:
            if (singleRow_ >= 0) return knobSegText(singleRow_, selStep_);
            return num3(selStep_ + 1);
        case Menu::Preset: {
            const int sel = getInt("preset");
            std::string t = num3(sel);
            if (sel >= 1 && proc_.presetUsed(sel)) t += '.';  // le point indique un slot déjà enregistré
            return t;
        }
    }
    return std::string("   ");
}

void Facade::timerCallback() {
    for (int r = 0; r < maq::kRows; ++r) {
        for (int s = 0; s < maq::kSteps; ++s) {
            const size_t idx = static_cast<size_t>(r * maq::kSteps + s);
            const int level = ledLevelFor(r, s);
            if (level != shownLed_[idx]) {
                shownLed_[idx] = level;
                repaint(stepLedRect(r, s).expanded(3));
            }
        }
    }
    for (int i = 0; i < 8; ++i) {
        const bool on = menuLedOn(i);
        if (on != shownMenuLed_[static_cast<size_t>(i)]) {
            shownMenuLed_[static_cast<size_t>(i)] = on;
            repaint(menuLedRect(i).expanded(3));
        }
    }
    const std::string text = computeDisplay();
    if (text != shownText_) {
        shownText_ = text;
        repaint(displayRect());
    }
}

// ---------------------------------------------------------------- Barre de contrôle

ControlBar::ControlBar(MaqProcessor& p) : proc_(p) {
    lnf_.setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
    setLookAndFeel(&lnf_);

    notesBtn_.setButtonText("Notes");
    frBtn_.setButtonText(fr("Do R\xC3\xA9 Mi"));
    prevBtn_.setButtonText("<");
    nextBtn_.setButtonText(">");
    storeBtn_.setButtonText("Enregistrer");
    recallBtn_.setButtonText("Rappeler");
    importBtn_.setButtonText("Importer...");
    exportBtn_.setButtonText("Exporter...");
    importDirBtn_.setButtonText("Dossier import");
    exportDirBtn_.setButtonText("Dossier export");
    keepSameBtn_.setButtonText(fr("Garder le m\xC3\xAA" "me dossier"));

    notesBtn_.setClickingTogglesState(true);
    frBtn_.setClickingTogglesState(true);
    notesBtn_.setColour(TextButton::buttonOnColourId, Colour(0xffb02a1a));
    frBtn_.setColour(TextButton::buttonOnColourId, Colour(0xffb02a1a));
    if (auto* prm = proc_.apvts.getParameter("valnote"))
        notesAttach_ = std::make_unique<ButtonParameterAttachment>(*prm, notesBtn_);
    if (auto* prm = proc_.apvts.getParameter("notefr"))
        frAttach_ = std::make_unique<ButtonParameterAttachment>(*prm, frBtn_);

    titleLabel_.setText("PRESET", dontSendNotification);
    titleLabel_.setJustificationType(Justification::centredRight);
    presetLabel_.setJustificationType(Justification::centred);
    statusLabel_.setJustificationType(Justification::centredLeft);
    statusLabel_.setColour(Label::textColourId, Colours::lightgrey);
    nameEditor_.setMultiLine(false);
    nameEditor_.setReturnKeyStartsNewLine(false);

    keepSameBtn_.setToggleState(proc_.folders.keepSameFolder(), dontSendNotification);

    prevBtn_.onClick = [this] { bumpPreset(-1); };
    nextBtn_.onClick = [this] { bumpPreset(+1); };
    storeBtn_.onClick = [this] {
        const int s = selectedSlot();
        if (s < 1) { setStatus(fr("Choisis un slot de 1 \xC3\xA0 99")); return; }
        proc_.storePreset(s);
        setStatus("Preset " + String(s) + fr(" enregistr\xC3\xA9"));
    };
    recallBtn_.onClick = [this] {
        const int s = selectedSlot();
        if (s < 1) { setStatus(fr("Choisis un slot de 1 \xC3\xA0 99")); return; }
        if (proc_.recallPreset(s)) setStatus("Preset " + String(s) + fr(" rappel\xC3\xA9"));
        else setStatus("Preset " + String(s) + " : slot vide");
    };
    importBtn_.onClick = [this] { doImport(); };
    exportBtn_.onClick = [this] { doExport(); };
    importDirBtn_.onClick = [this] { chooseFolder(maq::FolderPurpose::PresetImport, fr("Dossier d'import des presets")); };
    exportDirBtn_.onClick = [this] { chooseFolder(maq::FolderPurpose::PresetExport, fr("Dossier d'export des presets")); };
    keepSameBtn_.onClick = [this] {
        proc_.folders.setKeepSameFolder(keepSameBtn_.getToggleState());
        proc_.saveFolderSettings();
        setStatus(keepSameBtn_.getToggleState() ? fr("Un seul dossier pour tous les usages")
                                                : fr("Un dossier par usage"));
    };
    nameEditor_.onTextChange = [this] {
        const int s = selectedSlot();
        if (s >= 1 && proc_.presetUsed(s)) proc_.setPresetName(s, nameEditor_.getText());
    };

    for (auto* b : {&notesBtn_, &frBtn_, &prevBtn_, &nextBtn_, &storeBtn_, &recallBtn_,
                    &importBtn_, &exportBtn_, &importDirBtn_, &exportDirBtn_})
        addAndMakeVisible(*b);
    addAndMakeVisible(keepSameBtn_);
    addAndMakeVisible(titleLabel_);
    addAndMakeVisible(presetLabel_);
    addAndMakeVisible(statusLabel_);
    addAndMakeVisible(nameEditor_);

    startTimerHz(8);
}

ControlBar::~ControlBar() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void ControlBar::paint(Graphics& g) {
    g.fillAll(Colour(0xff161616));
    g.setColour(Colour(0xff3a3a3a));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void ControlBar::resized() {
    notesBtn_.setBounds(20, 10, 90, 30);
    frBtn_.setBounds(118, 10, 100, 30);
    titleLabel_.setBounds(236, 10, 76, 30);
    prevBtn_.setBounds(318, 10, 32, 30);
    presetLabel_.setBounds(354, 10, 56, 30);
    nextBtn_.setBounds(414, 10, 32, 30);
    nameEditor_.setBounds(458, 10, 224, 30);
    storeBtn_.setBounds(694, 10, 100, 30);
    recallBtn_.setBounds(800, 10, 90, 30);
    importBtn_.setBounds(906, 10, 100, 30);
    exportBtn_.setBounds(1012, 10, 100, 30);
    importDirBtn_.setBounds(1124, 10, 110, 30);
    exportDirBtn_.setBounds(1240, 10, 110, 30);
    statusLabel_.setBounds(20, 44, 860, 26);
    keepSameBtn_.setBounds(906, 44, 300, 26);
}

int ControlBar::selectedSlot() const {
    if (auto* v = proc_.apvts.getRawParameterValue("preset")) return static_cast<int>(std::lround(v->load()));
    return 0;
}

void ControlBar::bumpPreset(int delta) {
    if (auto* p = dynamic_cast<AudioParameterInt*>(proc_.apvts.getParameter("preset"))) {
        const auto range = p->getRange();
        const int v = jlimit(range.getStart(), range.getEnd(), p->get() + delta);
        if (v != p->get()) {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
    }
}

void ControlBar::setStatus(const String& text) { statusLabel_.setText(text, dontSendNotification); }

void ControlBar::timerCallback() {
    const int slot = selectedSlot();
    const bool used = proc_.presetUsed(slot);
    const String name = used ? proc_.presetName(slot) : String();

    if (slot != shownSlot_ || used != shownUsed_) {
        shownSlot_ = slot;
        shownUsed_ = used;
        presetLabel_.setText(String(slot), dontSendNotification);
        nameEditor_.setEnabled(used);
        nameEditor_.setTextToShowWhenEmpty(used ? "nom du preset" : "(slot vide)", Colours::grey);
        nameEditor_.setText(name, false);
        shownName_ = name;
    } else if (used && name != shownName_ && !nameEditor_.hasKeyboardFocus(true)) {
        nameEditor_.setText(name, false);
        shownName_ = name;
    }
}

void ControlBar::doExport() {
    const File dir(fr(proc_.folders.folderForUtf8(maq::FolderPurpose::PresetExport).c_str()));
    chooser_ = std::make_unique<FileChooser>(fr("Exporter la banque de presets"),
                                             dir.getChildFile("MAQ16_banque.maqbank"), "*.maqbank");
    chooser_->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles |
                              FileBrowserComponent::warnAboutOverwriting,
                          [this](const FileChooser& fc) {
                              File f = fc.getResult();
                              if (f == File()) return;
                              if (!f.hasFileExtension("maqbank")) f = f.withFileExtension("maqbank");
                              if (proc_.exportBank(f)) {
                                  proc_.folders.setFolderUtf8(maq::FolderPurpose::PresetExport,
                                                              f.getParentDirectory().getFullPathName().toRawUTF8());
                                  proc_.saveFolderSettings();
                                  setStatus(fr("Banque export\xC3\xA9") + fr("e : ") + f.getFileName() + " (" +
                                            String(proc_.usedPresetCount()) + " presets)");
                              } else {
                                  setStatus(fr("\xC3\x89") + fr("chec de l'export"));
                              }
                          });
}

void ControlBar::doImport() {
    const File dir(fr(proc_.folders.folderForUtf8(maq::FolderPurpose::PresetImport).c_str()));
    chooser_ = std::make_unique<FileChooser>(fr("Importer une banque de presets"), dir, "*.maqbank");
    chooser_->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this](const FileChooser& fc) {
                              const File f = fc.getResult();
                              if (f == File()) return;
                              if (proc_.importBank(f)) {
                                  proc_.folders.setFolderUtf8(maq::FolderPurpose::PresetImport,
                                                              f.getParentDirectory().getFullPathName().toRawUTF8());
                                  proc_.saveFolderSettings();
                                  setStatus(fr("Banque import\xC3\xA9") + fr("e : ") + f.getFileName() + " (" +
                                            String(proc_.usedPresetCount()) + " presets)");
                              } else {
                                  setStatus(fr("\xC3\x89") + fr("chec de l'import : fichier non valide"));
                              }
                          });
}

void ControlBar::chooseFolder(maq::FolderPurpose purpose, const String& title) {
    const File start(fr(proc_.folders.folderForUtf8(purpose).c_str()));
    chooser_ = std::make_unique<FileChooser>(title, start);
    chooser_->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this, purpose](const FileChooser& fc) {
                              const File d = fc.getResult();
                              if (!d.isDirectory()) return;
                              proc_.folders.setFolderUtf8(purpose, d.getFullPathName().toRawUTF8());
                              proc_.saveFolderSettings();
                              setStatus("Dossier : " + d.getFullPathName());
                          });
}

// ---------------------------------------------------------------- Éditeur

MaqEditor::MaqEditor(MaqProcessor& p) : AudioProcessorEditor(&p), facade_(p), bar_(p) {
    content_.addAndMakeVisible(facade_);
    content_.addAndMakeVisible(bar_);
    addAndMakeVisible(content_);

    setResizable(true, true);
    setResizeLimits(685, 289, 2738, 1156);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio(static_cast<double>(kWidth) / static_cast<double>(kTotalHeight));
    setSize(1096, 463);
}

void MaqEditor::resized() {
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kWidth);
    content_.setTransform(AffineTransform::scale(scale));
    content_.setBounds(0, 0, kWidth, kTotalHeight);
    facade_.setBounds(0, 0, kWidth, kHeight);
    bar_.setBounds(0, kHeight, kWidth, kBarHeight);
}

}  // namespace maqui
