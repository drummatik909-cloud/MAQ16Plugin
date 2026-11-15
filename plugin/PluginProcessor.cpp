#include "PluginProcessor.h"
#include "Facade.h"
#include <cmath>

using namespace juce;

namespace {
int toInt(const std::atomic<float>* a) { return static_cast<int>(std::lround(a->load())); }
}  // namespace

// Fonction membre : seule la classe a accès au type protégé AudioProcessor::BusesProperties
auto MaqProcessor::makeBuses() -> BusesProperties {
#if MAQ_AS_INSTRUMENT
    return BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true);  // sortie audio silencieuse
#else
    return BusesProperties();
#endif
}

MaqProcessor::MaqProcessor()
    : AudioProcessor(makeBuses()),
      apvts(*this, nullptr, "MAQ16", createLayout()) {
    for (auto& unitSteps : uiStepAll)
        for (auto& s : unitSteps) s.store(0);
    folders.load(maq::FolderSettings::defaultSettingsFile());
    prefs.load(maq::Preferences::defaultFile());
    followKey_ = apvts.getRawParameterValue("followkey");
    scaleRoot_ = apvts.getRawParameterValue("scaleroot");
    scaleType_ = apvts.getRawParameterValue("scaletype");
    scaleMode_ = apvts.getRawParameterValue("scalemode");

    for (int u = 0; u < maq::kUnits; ++u) {
        auto& q = up_[static_cast<size_t>(u)];
        q.active = apvts.getRawParameterValue(unitId("active", u));
        q.outMode = apvts.getRawParameterValue(unitId("outmode", u));
        q.outChan = apvts.getRawParameterValue(unitId("outchan", u));
        q.outOff = apvts.getRawParameterValue(unitId("outoff", u));
        for (int r = 0; r < maq::kRows; ++r) {
            auto& p = rp_[static_cast<size_t>(u)][static_cast<size_t>(r)];
            p.event = apvts.getRawParameterValue(rowId("event", r, u));
            p.cc = apvts.getRawParameterValue(rowId("cc", r, u));
            p.chan = apvts.getRawParameterValue(rowId("chan", r, u));
            p.first = apvts.getRawParameterValue(rowId("first", r, u));
            p.last = apvts.getRawParameterValue(rowId("last", r, u));
            p.gate = apvts.getRawParameterValue(rowId("gate", r, u));
            p.noteLen = apvts.getRawParameterValue(rowId("notelen", r, u));
            p.mode = apvts.getRawParameterValue(rowId("mode", r, u));
            for (int s = 0; s < maq::kSteps; ++s) {
                p.pot[static_cast<size_t>(s)] = apvts.getRawParameterValue(potId(r, s, u));
                p.mute[static_cast<size_t>(s)] = apvts.getRawParameterValue(muteId(r, s, u));
            }
        }
    }
    chainKind_ = apvts.getRawParameterValue("chainkind");
    chainLoop_ = apvts.getRawParameterValue("chainloop");
    trOn_ = apvts.getRawParameterValue("tr_on"); trSu_ = apvts.getRawParameterValue("tr_su");
    trSr_ = apvts.getRawParameterValue("tr_sr"); trDu_ = apvts.getRawParameterValue("tr_du");
    trDr_ = apvts.getRawParameterValue("tr_dr");
    ctOn_ = apvts.getRawParameterValue("ct_on"); ctSu_ = apvts.getRawParameterValue("ct_su");
    ctSr_ = apvts.getRawParameterValue("ct_sr"); ctDu_ = apvts.getRawParameterValue("ct_du");
}

AudioProcessorValueTreeState::ParameterLayout MaqProcessor::createLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Réglages globaux
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"valnote", 1}, "Show note names", false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"notefr", 1}, "Note names in French", true));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"preset", 1}, "Preset slot", 0, maq::kPresetSlots, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"followkey", 1}, "Follow key via MIDI (ch.16)", false));

    {   // Gamme : fondamentale, type, mode (aucune / verrouillage / quantification en sortie)
        StringArray roots, scales, modes;
        for (int pc = 0; pc < 12; ++pc) roots.add(String::fromUTF8(maq::pitchClassName(pc, false).c_str()));
        for (int t = 0; t < maq::kScaleCount; ++t)
            scales.add(String::fromUTF8(maq::scaleName(t, maq::Lang::English).c_str()));
        modes.add("Off");
        modes.add("Lock");
        modes.add("Output quantize");
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"scaleroot", 1}, "Scale root", roots, 0));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"scaletype", 1}, "Scale type", scales, 0));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"scalemode", 1}, "Scale mode", modes, 0));
    }

    // Unités A à D : activation et canal de sortie
    for (int u = 0; u < maq::kUnits; ++u) {
        const String un = String::charToString(static_cast<juce_wchar>('A' + u)) + " ";
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{unitId("active", u), 1}, un + "Unit active", u == 0));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{unitId("outmode", u), 1}, un + "Output channel mode",
                                                          StringArray{"Per row", "Forced", "Offset"}, 0));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{unitId("outchan", u), 1}, un + "Forced channel", 1, 16, 1));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{unitId("outoff", u), 1}, un + "Channel offset", -15, 15, 0));
    }

    // Chaînage en série et liens entre unités
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"chainkind", 1}, "Chain",
                                                      StringArray{"Off", "Series by row", "Series by unit"}, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"chainloop", 1}, "Chain loops to first unit", true));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"tr_on", 1}, "Transposition link", false));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"tr_su", 1}, "Transposition source unit", 0, 3, 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"tr_sr", 1}, "Transposition source row", 0, 2, 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"tr_du", 1}, "Transposition target unit", 0, 3, 1));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"tr_dr", 1}, "Transposition target row (0 = all)", 0, 3, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"ct_on", 1}, "Control link", false));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"ct_su", 1}, "Control source unit", 0, 3, 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"ct_sr", 1}, "Control source row", 0, 2, 0));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{"ct_du", 1}, "Control target unit", 0, 3, 1));

    for (int u = 0; u < maq::kUnits; ++u) {
        const String un = u == 0 ? String() : String::charToString(static_cast<juce_wchar>('A' + u)) + " ";
        for (int r = 0; r < maq::kRows; ++r) {
            const String rn = un + "Row " + String(r + 1);
            // Valeurs par défaut du preset 0 du manuel : nA1, canaux 1/2/3, pas 1 à 16, g = 6, n = 1, Forward
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("event", r, u), 1}, rn + " Event", 0, maq::kEventCount - 1, 2));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("cc", r, u), 1}, rn + " Free CC number", 0, 127, 0));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("chan", r, u), 1}, rn + " MIDI channel", 1, 16, r + 1));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("first", r, u), 1}, rn + " First step", 1, 16, 1));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("last", r, u), 1}, rn + " Last step", 1, 16, 16));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("gate", r, u), 1}, rn + " Gate time g", 1, 99, 6));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("notelen", r, u), 1}, rn + " Note length n", 1, 99, 1));
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("mode", r, u), 1}, rn + " Mode", 0, maq::kModeCount - 1, 0));
            for (int s = 0; s < maq::kSteps; ++s) {
                const String sn = rn + " Step " + String(s + 1);
                layout.add(std::make_unique<AudioParameterInt>(ParameterID{potId(r, s, u), 1}, sn + " Knob", 0, 127, 0));
                layout.add(std::make_unique<AudioParameterBool>(ParameterID{muteId(r, s, u), 1}, sn + " Mute", false));
            }
        }
    }
    return layout;
}

void MaqProcessor::syncEngineFromParams() { configureProject(project_); }

// Copie les réglages actuels (paramètres) dans le projet : unités, canaux de sortie, chaînage et liens
void MaqProcessor::configureProject(maq::Project& p) const {
    for (int u = 0; u < maq::kUnits; ++u) {
        const auto& q = up_[static_cast<size_t>(u)];
        p.active[static_cast<size_t>(u)] = q.active->load() > 0.5f;
        configureUnit(p.units[static_cast<size_t>(u)], u);
        auto& o = p.output[static_cast<size_t>(u)];
        o.mode = static_cast<maq::ChannelMode>(jlimit(0, 2, toInt(q.outMode)));
        o.forced = toInt(q.outChan);
        o.offset = toInt(q.outOff);
    }
    const int kind = toInt(chainKind_);
    p.chain.kind = kind == 0 ? maq::ChainKind::None : maq::ChainKind::Series;
    p.chain.scope = kind == 2 ? maq::ChainScope::WholeUnit : maq::ChainScope::PerRow;
    p.chain.loopToFirst = chainLoop_->load() > 0.5f;
    p.configureChain();

    maq::TransposeLink t;
    t.enabled = trOn_->load() > 0.5f;
    t.srcUnit = toInt(trSu_);
    t.srcRow = toInt(trSr_);
    t.dstUnit = toInt(trDu_);
    t.dstRow = toInt(trDr_) - 1;  // 0 = toutes les rangées
    p.transposeLinks[0] = t;

    maq::ControlLink c;
    c.enabled = ctOn_->load() > 0.5f;
    c.srcUnit = toInt(ctSu_);
    c.srcRow = toInt(ctSr_);
    c.dstUnit = toInt(ctDu_);
    if (!p.setControlLink(0, c)) p.controlLinks[0].enabled = false;  // lien vers soi-même ou en boucle : ignoré
}

maq::Project MaqProcessor::makeProjectSnapshot() const {
    maq::Project p;
    configureProject(p);
    return p;
}

std::vector<maq::ChannelCollision> MaqProcessor::channelCollisions() const { return makeProjectSnapshot().collisions(); }

// Copie les réglages d'une unité (paramètres) dans une unité du moteur
void MaqProcessor::configureUnit(maq::Unit& u, int unit) const {
    const int followed = followRoot.load();
    u.scaleRoot = (followKey_->load() > 0.5f && followed >= 0) ? followed : toInt(scaleRoot_);  // fondamentale reçue par MIDI
    u.scaleType = toInt(scaleType_);
    u.scaleMode = toInt(scaleMode_);

    for (int r = 0; r < maq::kRows; ++r) {
        const auto& p = rp_[static_cast<size_t>(unit)][static_cast<size_t>(r)];
        auto& cfg = u.rows[static_cast<size_t>(r)];

        maq::EventConfig e = maq::eventForIndex(toInt(p.event));
        if (e.kind == maq::EventKind::FreeCC) e.ccNumber = toInt(p.cc);
        cfg.event = e;

        cfg.channel = toInt(p.chan);
        cfg.first = toInt(p.first);
        cfg.last = toInt(p.last);
        cfg.gate = toInt(p.gate);
        cfg.noteLen = toInt(p.noteLen);
        cfg.mode = static_cast<maq::PlayMode>(toInt(p.mode));
        // Une rangée de vélocité s'applique à toutes les autres rangées (l'assignation fine viendra plus tard).
        cfg.velocityTargets = (e.kind == maq::EventKind::Velocity) ? (0x7 & ~(1 << r)) : 0;

        for (int s = 0; s < maq::kSteps; ++s) {
            cfg.pot[static_cast<size_t>(s)] = toInt(p.pot[static_cast<size_t>(s)]);
            cfg.mute[static_cast<size_t>(s)] = p.mute[static_cast<size_t>(s)]->load() > 0.5f;
        }
    }
}

void MaqProcessor::prepareToPlay(double sampleRate, int) {
    sampleRate_ = sampleRate;
    wasPlaying_ = false;
}

void MaqProcessor::emit(const std::vector<maq::MidiMsg>& msgs, int samplePos, MidiBuffer& out) {
    using T = maq::MidiMsg::Type;
    for (const auto& m : msgs) {
        const int ch = m.channel + 1;  // JUCE : canaux 1..16
        switch (m.type) {
            case T::NoteOn:
                out.addEvent(MidiMessage::noteOn(ch, m.data1, static_cast<uint8>(m.data2)), samplePos); break;
            case T::NoteOff:
                out.addEvent(MidiMessage::noteOff(ch, m.data1, static_cast<uint8>(0)), samplePos); break;
            case T::ControlChange:
                out.addEvent(MidiMessage::controllerEvent(ch, m.data1, m.data2), samplePos); break;
            case T::PitchBend:
                out.addEvent(MidiMessage::pitchWheel(ch, m.data1), samplePos); break;
            case T::ChannelPressure:
                out.addEvent(MidiMessage::channelPressureChange(ch, m.data1), samplePos); break;
            case T::ProgramChange:
                out.addEvent(MidiMessage::programChange(ch, m.data1), samplePos); break;
        }
    }
}

void MaqProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    buffer.clear();  // le plugin ne produit pas de son : sortie audio silencieuse
    const bool followOn = followKey_->load() > 0.5f;
    if (followOn) {  // suivi de la tonalité : une note sur le canal 16 donne la fondamentale
        for (const auto metadata : midi) {
            const MidiMessage m = metadata.getMessage();
            if (m.isNoteOn() && m.getChannel() == 16) followRoot.store(m.getNoteNumber() % 12);
        }
    }

    if (learnActive.load()) {  // apprentissage : on met de côté les notes jouées au clavier
        for (const auto metadata : midi) {
            const MidiMessage m = metadata.getMessage();
            if (m.isNoteOn() && !(followOn && m.getChannel() == 16)) {
                int start1, size1, start2, size2;
                noteFifo_.prepareToWrite(1, start1, size1, start2, size2);
                if (size1 > 0) {
                    NoteIn n;
                    n.note = m.getNoteNumber();
                    n.velocity = m.getVelocity();
                    noteBuf_[static_cast<size_t>(start1)] = n;
                    noteFifo_.finishedWrite(1);
                }
            }
        }
    }

    midi.clear();  // plugin générateur : les notes entrantes ne sont pas transmises
    const int numSamples = buffer.getNumSamples();

    if (panicRequested.exchange(false)) {  // bouton Panic : Note Off + All Notes Off immédiats
        std::vector<maq::MidiMsg> panicMsgs;
        project_.panic(panicMsgs);
        emit(panicMsgs, 0, midi);
    }

    syncEngineFromParams();

    Optional<AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();

    if (pos.hasValue()) {
        const double bpm = pos->getBpm().orFallback(0.0);
        if (bpm > 0.0) uiBpm.store(bpm);
    }

    const bool playing = pos.hasValue() && pos->getIsPlaying();
    uiPlaying.store(playing);
    std::vector<maq::MidiMsg> out;

    if (!playing) {
        if (wasPlaying_) {
            project_.stop(out);
            for (const auto& m : out) capture_.push(capture_.lastClock(), m);
            emit(out, 0, midi);
        }
        wasPlaying_ = false;
        return;
    }

    const double ppq = pos->getPpqPosition().orFallback(0.0);
    const double bpm = pos->getBpm().orFallback(120.0);
    const double samplesPerClock = sampleRate_ * 60.0 / (bpm * 24.0);  // 24 horloges par noire
    const double startClock = ppq * 24.0;
    const double endClock = startClock + numSamples / samplesPerClock;

    if (!wasPlaying_) {
        if (ppq < 1e-6) project_.start(); else project_.continuePlaying();
        nextClock_ = std::ceil(startClock - 1e-9);
    } else if (std::abs(startClock - expectedStart_) > 2.0) {
        nextClock_ = std::ceil(startClock - 1e-9);  // saut de position (boucle, déplacement du curseur)
    }
    wasPlaying_ = true;

    while (nextClock_ < endClock - 1e-9) {
        int offset = static_cast<int>((nextClock_ - startClock) * samplesPerClock);
        if (offset < 0) offset = 0;
        if (offset > numSamples - 1) offset = numSamples - 1;
        out.clear();
        project_.tick(out);
        for (const auto& m : out) capture_.push(captureClock_, m);  // mémoire de la capture
        capture_.setLastClock(captureClock_);
        ++captureClock_;
        emit(out, offset, midi);
        nextClock_ += 1.0;
    }
    expectedStart_ = endClock;

    for (int u = 0; u < maq::kUnits; ++u)
        for (int r = 0; r < maq::kRows; ++r)
            uiStepAll[static_cast<size_t>(u)][static_cast<size_t>(r)].store(project_.units[static_cast<size_t>(u)].currentStep(r));
}

// ------------------------------------------------------------------ Presets

void MaqProcessor::setParamValue(const String& id, int value) {
    if (auto* p = apvts.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
        p->endChangeGesture();
    }
}

void MaqProcessor::storePreset(int slot) {
    if (slot < 1 || slot > maq::kPresetSlots) return;

    maq::Preset p;
    {
        const ScopedLock sl(bankLock_);
        p = bank_.slot(slot);  // on garde le nom déjà donné
    }

    const int unit = jlimit(0, maq::kUnits - 1, selectedUnit.load());  // le preset concerne l'unité affichée
    for (int r = 0; r < maq::kRows; ++r) {
        const auto& src = rp_[static_cast<size_t>(unit)][static_cast<size_t>(r)];
        auto& dst = p.rows[static_cast<size_t>(r)];

        maq::EventConfig e = maq::eventForIndex(toInt(src.event));
        if (e.kind == maq::EventKind::FreeCC) e.ccNumber = toInt(src.cc);
        dst.event = e;
        dst.channel = toInt(src.chan);
        dst.first = toInt(src.first);
        dst.last = toInt(src.last);
        dst.gate = toInt(src.gate);
        dst.noteLen = toInt(src.noteLen);
        dst.mode = static_cast<maq::PlayMode>(toInt(src.mode));
        for (int s = 0; s < maq::kSteps; ++s) {
            dst.pot[static_cast<size_t>(s)] = toInt(src.pot[static_cast<size_t>(s)]);
            dst.mute[static_cast<size_t>(s)] = src.mute[static_cast<size_t>(s)]->load() > 0.5f;
        }
    }
    p.used = true;

    const ScopedLock sl(bankLock_);
    bank_.slot(slot) = p;
}

bool MaqProcessor::recallPreset(int slot) {
    if (slot < 1 || slot > maq::kPresetSlots) return false;

    maq::Preset p;
    {
        const ScopedLock sl(bankLock_);
        if (!bank_.slot(slot).used) return false;
        p = bank_.slot(slot);
    }

    const int unit = jlimit(0, maq::kUnits - 1, selectedUnit.load());
    for (int r = 0; r < maq::kRows; ++r) {
        const auto& c = p.rows[static_cast<size_t>(r)];
        setParamValue(rowId("event", r, unit), maq::eventIndexFor(c.event));
        if (c.event.kind == maq::EventKind::FreeCC) setParamValue(rowId("cc", r, unit), c.event.ccNumber);
        setParamValue(rowId("chan", r, unit), c.channel);
        setParamValue(rowId("first", r, unit), c.first);
        setParamValue(rowId("last", r, unit), c.last);
        setParamValue(rowId("gate", r, unit), c.gate);
        setParamValue(rowId("notelen", r, unit), c.noteLen);
        setParamValue(rowId("mode", r, unit), static_cast<int>(c.mode));
        for (int s = 0; s < maq::kSteps; ++s) {
            setParamValue(potId(r, s, unit), c.pot[static_cast<size_t>(s)]);
            setParamValue(muteId(r, s, unit), c.mute[static_cast<size_t>(s)] ? 1 : 0);
        }
    }
    return true;
}

bool MaqProcessor::presetUsed(int slot) const {
    if (slot < 1 || slot > maq::kPresetSlots) return false;
    const ScopedLock sl(bankLock_);
    return bank_.slot(slot).used;
}

String MaqProcessor::presetName(int slot) const {
    if (slot < 1 || slot > maq::kPresetSlots) return {};
    const ScopedLock sl(bankLock_);
    return String::fromUTF8(bank_.slot(slot).name.c_str());
}

void MaqProcessor::setPresetName(int slot, const String& name) {
    if (slot < 1 || slot > maq::kPresetSlots) return;
    const ScopedLock sl(bankLock_);
    bank_.slot(slot).name = name.toStdString();
}

int MaqProcessor::usedPresetCount() const {
    const ScopedLock sl(bankLock_);
    int n = 0;
    for (int i = 1; i <= maq::kPresetSlots; ++i)
        if (bank_.slot(i).used) ++n;
    return n;
}

// ------------------------------------------------------------------ Banque et dossiers

bool MaqProcessor::exportBank(const File& file) const {
    std::string text;
    {
        const ScopedLock sl(bankLock_);
        text = bank_.serialize();
    }
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(String::fromUTF8(text.c_str()));
}

bool MaqProcessor::importBank(const File& file) {
    if (!file.existsAsFile()) return false;
    maq::Bank imported;
    if (!imported.deserialize(file.loadFileAsString().toStdString())) return false;
    const ScopedLock sl(bankLock_);
    bank_ = imported;
    return true;
}

void MaqProcessor::saveFolderSettings() { folders.save(maq::FolderSettings::defaultSettingsFile()); }

std::vector<std::uint8_t> MaqProcessor::buildMidi(int source, int lengthIndex, int scope) const {
    const double current = uiBpm.load();
    const double bpm = current > 0.0 ? current : 120.0;
    const int unit = jlimit(0, maq::kUnits - 1, selectedUnit.load());
    const bool allUnits = (scope == 4);
    const int rowMask = (!allUnits && scope >= 1 && scope <= maq::kRows) ? (1 << (scope - 1)) : 0x7;
    const int unitMask = allUnits ? 0xF : (1 << unit);

    std::vector<maq::CapturedEvent> events;
    std::int64_t length = 0;
    if (source == 0) {  // boucle : rendu sans transport, reproductible
        maq::Project p = makeProjectSnapshot();
        if (!allUnits) p.active[static_cast<size_t>(unit)] = true;  // l'unité choisie est exportée même si elle est désactivée
        const bool alone = !allUnits && p.chain.kind == maq::ChainKind::None;  // sans chaînage : durée de l'unité seule
        const maq::Unit& chosen = p.units[static_cast<size_t>(unit)];
        static const int multiples[4] = {1, 2, 4, 8};
        if (lengthIndex >= 0 && lengthIndex < 4)
            length = (alone ? maq::loopLengthClocks(chosen.rows) : maq::projectLoopClocks(p)) * multiples[lengthIndex];
        else  // cycle complet, plafonné à 64 mesures
            length = alone ? maq::cycleLengthClocks(chosen.rows, 64 * maq::kClocksPerBar)
                           : maq::projectCycleClocks(p, 64 * maq::kClocksPerBar);
        events = maq::renderProject(p, length);
    } else {  // capture : les derniers passages joués
        static const int bars[6] = {1, 2, 4, 8, 16, 32};
        length = static_cast<std::int64_t>(bars[jlimit(0, 5, lengthIndex)]) * maq::kClocksPerBar;
        events = capture_.window(length);
    }

    bool anyNote = false;
    for (const auto& e : events)
        if (e.msg.type == maq::MidiMsg::Type::NoteOn && e.msg.row >= 0 && e.msg.unit >= 0 &&
            (rowMask & (1 << e.msg.row)) && (unitMask & (1 << e.msg.unit))) { anyNote = true; break; }
    if (!anyNote) return {};
    return maq::buildMidiFile(events, bpm, rowMask, unitMask, length);
}

bool MaqProcessor::popNote(NoteIn& out) {
    int start1, size1, start2, size2;
    noteFifo_.prepareToRead(1, start1, size1, start2, size2);
    if (size1 <= 0) return false;
    out = noteBuf_[static_cast<size_t>(start1)];
    noteFifo_.finishedRead(1);
    return true;
}

void MaqProcessor::savePreferences() { prefs.save(maq::Preferences::defaultFile()); }

// ------------------------------------------------------------------ État du projet

AudioProcessorEditor* MaqProcessor::createEditor() { return new MaqEditor(*this); }

void MaqProcessor::getStateInformation(MemoryBlock& dest) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) {
        std::string text;
        {
            const ScopedLock sl(bankLock_);
            text = bank_.serialize();
        }
        xml->createNewChildElement("BANK")->addTextElement(String::fromUTF8(text.c_str()));
        copyXmlToBinary(*xml, dest);
    }
}

void MaqProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        if (xml->hasTagName(apvts.state.getType())) {
            if (auto* bankXml = xml->getChildByName("BANK")) {
                maq::Bank restored;
                if (restored.deserialize(bankXml->getAllSubText().toStdString())) {
                    const ScopedLock sl(bankLock_);
                    bank_ = restored;
                }
                xml->removeChildElement(bankXml, true);
            }
            apvts.replaceState(ValueTree::fromXml(*xml));
        }
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaqProcessor(); }
