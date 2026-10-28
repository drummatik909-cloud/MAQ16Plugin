#include "PluginProcessor.h"
#include "Facade.h"
#include <cmath>

using namespace juce;

MaqProcessor::MaqProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this, nullptr, "MAQ16", createLayout()) {
    for (auto& s : uiStep) s.store(0);

    for (int r = 0; r < maq::kRows; ++r) {
        auto& p = rp_[r];
        p.event = apvts.getRawParameterValue(rowId("event", r));
        p.cc = apvts.getRawParameterValue(rowId("cc", r));
        p.chan = apvts.getRawParameterValue(rowId("chan", r));
        p.first = apvts.getRawParameterValue(rowId("first", r));
        p.last = apvts.getRawParameterValue(rowId("last", r));
        p.gate = apvts.getRawParameterValue(rowId("gate", r));
        p.noteLen = apvts.getRawParameterValue(rowId("notelen", r));
        p.mode = apvts.getRawParameterValue(rowId("mode", r));
        for (int s = 0; s < maq::kSteps; ++s) {
            p.pot[s] = apvts.getRawParameterValue(potId(r, s));
            p.mute[s] = apvts.getRawParameterValue(muteId(r, s));
        }
    }
}

AudioProcessorValueTreeState::ParameterLayout MaqProcessor::createLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (int r = 0; r < maq::kRows; ++r) {
        const String rn = "Row " + String(r + 1);
        // Valeurs par défaut du preset 0 du manuel : nA1, canaux 1/2/3, pas 1 à 16, g = 6, n = 1, Forward
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("event", r), 1}, rn + " Event", 0, maq::kEventCount - 1, 2));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("cc", r), 1}, rn + " Free CC number", 0, 127, 0));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("chan", r), 1}, rn + " MIDI channel", 1, 16, r + 1));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("first", r), 1}, rn + " First step", 1, 16, 1));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("last", r), 1}, rn + " Last step", 1, 16, 16));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("gate", r), 1}, rn + " Gate time g", 1, 99, 6));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("notelen", r), 1}, rn + " Note length n", 1, 99, 1));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{rowId("mode", r), 1}, rn + " Mode", 0, maq::kModeCount - 1, 0));
        for (int s = 0; s < maq::kSteps; ++s) {
            const String sn = rn + " Step " + String(s + 1);
            layout.add(std::make_unique<AudioParameterInt>(ParameterID{potId(r, s), 1}, sn + " Knob", 0, 127, 0));
            layout.add(std::make_unique<AudioParameterBool>(ParameterID{muteId(r, s), 1}, sn + " Mute", false));
        }
    }
    return layout;
}

void MaqProcessor::syncEngineFromParams() {
    for (int r = 0; r < maq::kRows; ++r) {
        const auto& p = rp_[r];
        auto& cfg = unit_.rows[r];

        maq::EventConfig e = maq::eventForIndex(static_cast<int>(std::lround(p.event->load())));
        if (e.kind == maq::EventKind::FreeCC) e.ccNumber = static_cast<int>(std::lround(p.cc->load()));
        cfg.event = e;

        cfg.channel = static_cast<int>(std::lround(p.chan->load()));
        cfg.first = static_cast<int>(std::lround(p.first->load()));
        cfg.last = static_cast<int>(std::lround(p.last->load()));
        cfg.gate = static_cast<int>(std::lround(p.gate->load()));
        cfg.noteLen = static_cast<int>(std::lround(p.noteLen->load()));
        cfg.mode = static_cast<maq::PlayMode>(static_cast<int>(std::lround(p.mode->load())));
        // Une rangée de vélocité s'applique à toutes les autres rangées (l'assignation fine viendra plus tard).
        cfg.velocityTargets = (e.kind == maq::EventKind::Velocity) ? (0x7 & ~(1 << r)) : 0;

        for (int s = 0; s < maq::kSteps; ++s) {
            cfg.pot[s] = static_cast<int>(std::lround(p.pot[s]->load()));
            cfg.mute[s] = p.mute[s]->load() > 0.5f;
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
    midi.clear();  // plugin générateur : les notes entrantes ne sont pas transmises
    const int numSamples = buffer.getNumSamples();

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
            unit_.stop(out);
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
        if (ppq < 1e-6) unit_.start(); else unit_.continuePlaying();
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
        unit_.tick(out);
        emit(out, offset, midi);
        nextClock_ += 1.0;
    }
    expectedStart_ = endClock;

    for (int r = 0; r < maq::kRows; ++r) uiStep[r].store(unit_.currentStep(r));
}

AudioProcessorEditor* MaqProcessor::createEditor() { return new MaqEditor(*this); }

void MaqProcessor::getStateInformation(MemoryBlock& dest) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void MaqProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(ValueTree::fromXml(*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaqProcessor(); }
