// NON COMPILÉ dans l'environnement de rédaction : à valider avec JUCE 8 sur Windows puis macOS.
#include "PluginProcessor.h"
#include <cmath>

MaqProcessor::MaqProcessor()
    : AudioProcessor(BusesProperties()) {}

void MaqProcessor::prepareToPlay(double sampleRate, int) {
    sampleRate_ = sampleRate;
    wasPlaying_ = false;
}

void MaqProcessor::emit(const std::vector<maq::MidiMsg>& msgs, int samplePos, juce::MidiBuffer& out) {
    using T = maq::MidiMsg::Type;
    for (const auto& m : msgs) {
        const int ch = m.channel + 1;  // JUCE : canaux 1..16
        switch (m.type) {
            case T::NoteOn:
                out.addEvent(juce::MidiMessage::noteOn(ch, m.data1, static_cast<juce::uint8>(m.data2)), samplePos); break;
            case T::NoteOff:
                out.addEvent(juce::MidiMessage::noteOff(ch, m.data1, static_cast<juce::uint8>(0)), samplePos); break;
            case T::ControlChange:
                out.addEvent(juce::MidiMessage::controllerEvent(ch, m.data1, m.data2), samplePos); break;
            case T::PitchBend:
                out.addEvent(juce::MidiMessage::pitchWheel(ch, m.data1), samplePos); break;
            case T::ChannelPressure:
                out.addEvent(juce::MidiMessage::channelPressureChange(ch, m.data1), samplePos); break;
            case T::ProgramChange:
                out.addEvent(juce::MidiMessage::programChange(ch, m.data1), samplePos); break;
        }
    }
}

void MaqProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    midi.clear();  // plugin générateur : on ne laisse pas passer les notes entrantes
    const int numSamples = buffer.getNumSamples();

    juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();

    const bool playing = pos.hasValue() && pos->getIsPlaying();
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
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaqProcessor(); }
