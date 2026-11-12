// MIT License - see LICENSE
#pragma once

// A clean-room model of the Yamaha OPL2 (YM3812) and OPL3 (YMF262), written from their published
// behaviour: an operator reads a log-sine table, adds its attenuation in the log domain and turns
// the sum back into a level through an exponent table, as the chips do. Envelopes advance on the
// global counter at the chips' rates; tremolo, vibrato, key scaling, rhythm mode and the OPL3
// four-operator connections are modelled. It runs at the chips' rate of 49716 Hz.

#include "qfmpatch_p.h"

#include <QtFM/qfmchip.h>

namespace QtFM::Private {

class OplCore final : public ChipCore {
public:
    explicit OplCore(bool opl3);

    ChipType type() const override { return m_opl3 ? ChipType::OPL3 : ChipType::OPL2; }
    int nativeRate() const override { return 49716; }
    void reset() override;
    void write(quint16 reg, quint8 value) override;
    void generate(qint32 *stereo, int frames) override;

    // For voices on chip channels.
    int channelCount() const { return m_opl3 ? 18 : 9; }
    // Returns true if the channel's carriers are silent and no key is down.
    bool channelSilent(int channel) const;

private:
    struct Op {
        quint32 phase = 0;
        int multX2 = 2;
        bool am = false;
        bool vib = false;
        bool egt = false;
        bool ksr = false;
        int ksl = 0;
        int tl = 63;
        int ar = 0;
        int dr = 0;
        int sl = 0;
        int rr = 0;
        int ws = 0;
        int env = 511;
        int stage = 3; // 0 attack, 1 decay, 2 sustain, 3 release
        bool key = false;
        bool chKey = false;
        bool drumKey = false;
        int out = 0;
        int out1 = 0;
        int out2 = 0;
    };
    struct Channel {
        int fnum = 0;
        int block = 0;
        bool key = false;
        int fb = 0;
        int cnt = 0;
        bool left = true;
        bool right = true;
        int op[2] = {0, 0};
    };

    void writeOp(int reg, int slot, quint8 value);
    void updateKey(Op &op);
    void stepEnvelope(Op &op, const Channel &ch);
    void stepPhase(Op &op, const Channel &ch);
    int attenuation(const Op &op, const Channel &ch) const;
    int opOutput(const Op &op, const Channel &ch, int phaseIndex) const;
    int runTwoOp(int c, int mod0);
    int ws(const Op &op) const;

    bool m_opl3;
    bool m_new = false;
    bool m_wse = false;
    bool m_nts = false;
    bool m_rhythm = false;
    bool m_amDepth = false;
    bool m_vibDepth = false;
    quint8 m_fourOp = 0;
    quint8 m_drums = 0;
    quint32 m_egCounter = 0;
    int m_tremPos = 0;
    int m_vibPos = 0;
    int m_vibCount = 0;
    quint32 m_noise = 1;
    Op m_ops[36];
    Channel m_ch[18];
};

// Computes the registers of an OPL2 instrument for patch, in the SBI order: characteristic (0x20),
// scaling and level (0x40), attack and decay (0x60), sustain and release (0x80) and waveform
// (0xE0) for the modulator then the carrier, then feedback and connection (0xC0). Returns false if
// the patch does not have two operators.
bool oplInstrument(const PatchData &patch, quint8 regs[11]);

// Sets the frequency number and block of the OPL for hz; returns false if it is out of range.
bool oplFrequency(float hz, int *fnum, int *block);

} // namespace QtFM::Private
