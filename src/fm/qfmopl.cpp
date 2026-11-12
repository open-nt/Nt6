// MIT License - see LICENSE
//
// A clean-room model of the OPL2 and OPL3: see qfmopl_p.h. Levels are attenuations in steps of
// 0.1875 dB (512 steps for 96 dB), as in the chips; an operator's output is a 13-bit signed value.

#include "qfmopl_p.h"

#include <algorithm>
#include <cmath>

namespace QtFM::Private {

namespace {

struct Tables {
    quint16 logSin[256];
    quint16 exp[256];
    Tables() {
        for (int i = 0; i < 256; ++i) {
            const double s = std::sin((i + 0.5) * 3.14159265358979323846 / 512.0);
            logSin[i] = static_cast<quint16>(std::lround(-std::log2(s) * 256.0));
            exp[i] = static_cast<quint16>(std::lround((std::exp2(i / 256.0) - 1.0) * 1024.0));
        }
    }
};

const Tables &tables() {
    static const Tables t;
    return t;
}

// The slot of each operator register offset (0x00 to 0x15), or -1.
const int slotOfOffset[32] = {0,  1,  2,  3,  4,  5,  -1, -1, 6,  7,  8,  9,  10, 11, -1, -1,
                              12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

const int multX2[16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};

// Key scale level at block 7, in steps of 0.1875 dB, by the top four bits of the frequency number.
const int kslBase[16] = {0, 48, 64, 74, 80, 86, 90, 94, 96, 100, 102, 104, 106, 108, 110, 112};

// The envelope increments of the four rates within an octave of rates, over eight counter steps.
const quint8 egSteps[4][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1}, {0, 1, 0, 1, 1, 1, 0, 1}, {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1}};

const int vibratoShape[8] = {0, 1, 2, 1, 0, -1, -2, -1};

constexpr int kTremoloPeriod = 13432; // about 3.7 Hz
constexpr int kTremoloSteps = 26;     // 4.8 dB

int slotOfChannel(int ch, bool carrier) {
    const int bank = ch / 9;
    const int c = ch % 9;
    return bank * 18 + (c / 3) * 6 + c % 3 + (carrier ? 3 : 0);
}

} // namespace

OplCore::OplCore(bool opl3) : m_opl3(opl3) {
    tables();
    reset();
}

void OplCore::reset() {
    m_new = m_wse = m_nts = m_rhythm = m_amDepth = m_vibDepth = false;
    m_fourOp = m_drums = 0;
    m_egCounter = 0;
    m_tremPos = m_vibPos = m_vibCount = 0;
    m_noise = 1;
    for (Op &op : m_ops)
        op = Op();
    for (int c = 0; c < 18; ++c) {
        m_ch[c] = Channel();
        m_ch[c].op[0] = slotOfChannel(c, false);
        m_ch[c].op[1] = slotOfChannel(c, true);
    }
}

void OplCore::updateKey(Op &op) {
    const bool key = op.chKey || op.drumKey;
    if (key == op.key)
        return;
    op.key = key;
    if (key) {
        op.stage = 0;
        op.phase = 0;
    } else {
        op.stage = 3;
    }
}

void OplCore::writeOp(int reg, int slot, quint8 v) {
    Op &op = m_ops[slot];
    switch (reg & 0xe0) {
    case 0x20:
        op.am = v & 0x80;
        op.vib = v & 0x40;
        op.egt = v & 0x20;
        op.ksr = v & 0x10;
        op.multX2 = multX2[v & 0x0f];
        break;
    case 0x40:
        op.ksl = v >> 6;
        op.tl = v & 0x3f;
        break;
    case 0x60:
        op.ar = v >> 4;
        op.dr = v & 0x0f;
        break;
    case 0x80:
        op.sl = v >> 4;
        op.rr = v & 0x0f;
        break;
    case 0xe0:
        op.ws = v & 0x07;
        break;
    default:
        break;
    }
}

void OplCore::write(quint16 reg, quint8 v) {
    const int bank = m_opl3 ? (reg >> 8) & 1 : 0;
    const int r = reg & 0xff;
    if (bank == 0 && r == 0x01) {
        m_wse = v & 0x20;
        return;
    }
    if (bank == 1 && r == 0x04) {
        m_fourOp = v & 0x3f;
        return;
    }
    if (bank == 1 && r == 0x05) {
        m_new = v & 0x01;
        return;
    }
    if (bank == 0 && r == 0x08) {
        m_nts = v & 0x40;
        return;
    }
    if ((r >= 0x20 && r < 0xa0) || r >= 0xe0) {
        const int slot = slotOfOffset[r & 0x1f];
        if (slot >= 0)
            writeOp(r, bank * 18 + slot, v);
        return;
    }
    if (bank == 0 && r == 0xbd) {
        m_amDepth = v & 0x80;
        m_vibDepth = v & 0x40;
        m_rhythm = v & 0x20;
        m_drums = m_rhythm ? (v & 0x1f) : 0;
        m_ops[12].drumKey = m_ops[15].drumKey = m_drums & 0x10;
        m_ops[16].drumKey = m_drums & 0x08;
        m_ops[14].drumKey = m_drums & 0x04;
        m_ops[17].drumKey = m_drums & 0x02;
        m_ops[13].drumKey = m_drums & 0x01;
        for (int s : {12, 13, 14, 15, 16, 17})
            updateKey(m_ops[s]);
        return;
    }
    const int c = r & 0x0f;
    if (c > 8)
        return;
    Channel &ch = m_ch[bank * 9 + c];
    switch (r & 0xf0) {
    case 0xa0:
        ch.fnum = (ch.fnum & 0x300) | v;
        break;
    case 0xb0:
        ch.fnum = (ch.fnum & 0xff) | ((v & 0x03) << 8);
        ch.block = (v >> 2) & 0x07;
        ch.key = v & 0x20;
        for (int s : ch.op) {
            m_ops[s].chKey = ch.key;
            updateKey(m_ops[s]);
        }
        break;
    case 0xc0:
        ch.fb = (v >> 1) & 0x07;
        ch.cnt = v & 0x01;
        ch.left = v & 0x10;
        ch.right = v & 0x20;
        break;
    default:
        break;
    }
}

void OplCore::stepPhase(Op &op, const Channel &ch) {
    int fnum = ch.fnum;
    if (op.vib)
        fnum += ((fnum >> 7) * vibratoShape[m_vibPos]) >> (m_vibDepth ? 1 : 2);
    const auto inc = static_cast<quint32>(((std::max(fnum, 0) << ch.block) * op.multX2) >> 1);
    op.phase = (op.phase + inc) & 0xfffff;
}

void OplCore::stepEnvelope(Op &op, const Channel &ch) {
    int rate;
    switch (op.stage) {
    case 0:
        rate = op.ar;
        break;
    case 1:
        rate = op.dr;
        break;
    case 2:
        rate = op.egt ? 0 : op.rr;
        break;
    default:
        rate = op.rr;
        break;
    }
    const int sustainLevel = op.sl == 15 ? 496 : op.sl * 16;
    if (rate == 0) {
        if (op.stage == 1 && op.env >= sustainLevel)
            op.stage = 2;
        return;
    }
    const int keyCode = (ch.block << 1) | ((ch.fnum >> (m_nts ? 8 : 9)) & 1);
    const int effective = std::min(63, rate * 4 + (op.ksr ? keyCode : keyCode >> 2));
    const int octave = effective >> 2;
    int inc;
    if (octave < 12) {
        const int shift = 12 - octave;
        if (m_egCounter & ((1u << shift) - 1))
            return;
        inc = egSteps[effective & 3][(m_egCounter >> shift) & 7];
    } else {
        inc = egSteps[effective & 3][m_egCounter & 7] << (octave - 12);
        if (inc == 0)
            inc = 1 << (octave - 12);
    }
    if (op.stage == 0) {
        if (effective >= 60) {
            op.env = 0;
        } else if (inc) {
            op.env += (~op.env * inc) >> 3;
        }
        if (op.env <= 0) {
            op.env = 0;
            op.stage = 1;
        }
        return;
    }
    op.env = std::min(511, op.env + inc);
    if (op.stage == 1 && op.env >= sustainLevel)
        op.stage = 2;
}

int OplCore::attenuation(const Op &op, const Channel &ch) const {
    int att = op.env + op.tl * 4;
    if (op.ksl) {
        const int ksl = std::max(0, kslBase[ch.fnum >> 6] - 32 * (7 - ch.block));
        att += op.ksl == 3 ? ksl : ksl >> (op.ksl == 1 ? 1 : 2);
    }
    if (op.am) {
        const int tri = m_tremPos < kTremoloPeriod / 2 ? m_tremPos : kTremoloPeriod - m_tremPos;
        const int trem = tri * kTremoloSteps / (kTremoloPeriod / 2);
        att += m_amDepth ? trem : trem >> 2;
    }
    return std::min(att, 511);
}

int OplCore::ws(const Op &op) const {
    if (m_opl3 && m_new)
        return op.ws;
    return m_wse ? op.ws & 3 : 0;
}

int OplCore::opOutput(const Op &op, const Channel &ch, int phaseIndex) const {
    const int att = attenuation(op, ch);
    if (att >= 511)
        return 0;
    const Tables &t = tables();
    int p = phaseIndex & 1023;
    bool negative = false;
    int level; // log attenuation of the waveform, in 1/256 of a factor of two
    switch (ws(op)) {
    case 1: // half sine
        if (p & 0x200)
            return 0;
        level = t.logSin[(p & 0x100) ? 255 - (p & 0xff) : p & 0xff];
        break;
    case 2: // absolute sine
        level = t.logSin[(p & 0x100) ? 255 - (p & 0xff) : p & 0xff];
        break;
    case 3: // pulse sine: the rising quarters
        if (p & 0x100)
            return 0;
        level = t.logSin[p & 0xff];
        break;
    case 4: // alternating sine
        if (p & 0x200)
            return 0;
        p <<= 1;
        negative = p & 0x200;
        level = t.logSin[(p & 0x100) ? 255 - (p & 0xff) : p & 0xff];
        break;
    case 5: // camel sine
        if (p & 0x200)
            return 0;
        p <<= 1;
        level = t.logSin[(p & 0x100) ? 255 - (p & 0xff) : p & 0xff];
        break;
    case 6: // square
        negative = p & 0x200;
        level = 0;
        break;
    case 7: { // logarithmic sawtooth
        negative = p & 0x200;
        const int i = negative ? 0x1ff ^ (p & 0x1ff) : p & 0x1ff;
        level = i << 3;
        break;
    }
    default: // sine
        negative = p & 0x200;
        level = t.logSin[(p & 0x100) ? 255 - (p & 0xff) : p & 0xff];
        break;
    }
    const int x = level + (att << 3);
    const int shift = x >> 8;
    if (shift > 12)
        return 0;
    const int value = ((t.exp[(x & 0xff) ^ 0xff] | 0x400) << 1) >> shift;
    return negative ? -value : value;
}

int OplCore::runTwoOp(int c, int mod0) {
    Channel &ch = m_ch[c];
    Op &m = m_ops[ch.op[0]];
    Op &k = m_ops[ch.op[1]];
    const int fb = ch.fb ? (m.out1 + m.out2) >> (9 - ch.fb) : 0;
    const int mo = opOutput(m, ch, static_cast<int>(m.phase >> 10) + fb + mod0);
    m.out2 = m.out1;
    m.out1 = mo;
    if (ch.cnt == 0)
        return opOutput(k, ch, static_cast<int>(k.phase >> 10) + mo);
    return mo + opOutput(k, ch, static_cast<int>(k.phase >> 10));
}

bool OplCore::channelSilent(int channel) const {
    const Channel &ch = m_ch[channel];
    const Op &m = m_ops[ch.op[0]];
    const Op &k = m_ops[ch.op[1]];
    auto quiet = [](const Op &op) { return op.stage != 0 && op.env >= 480; };
    return quiet(k) && (ch.cnt == 0 || quiet(m));
}

void OplCore::generate(qint32 *stereo, int frames) {
    const int channels = channelCount();
    const int slotCount = channels * 2;
    for (int f = 0; f < frames; ++f) {
        ++m_egCounter;
        m_tremPos = (m_tremPos + 1) % kTremoloPeriod;
        if (++m_vibCount == 1024) {
            m_vibCount = 0;
            m_vibPos = (m_vibPos + 1) & 7;
        }
        if (m_noise & 1)
            m_noise ^= 0x800302;
        m_noise >>= 1;

        for (int s = 0; s < slotCount; ++s) {
            const Channel &ch = m_ch[(s / 18) * 9 + ((s % 18) / 6) * 3 + (s % 3)];
            stepPhase(m_ops[s], ch);
            stepEnvelope(m_ops[s], ch);
        }

        qint32 left = 0;
        qint32 right = 0;
        auto mix = [&](const Channel &ch, int value) {
            const bool stereo3 = m_opl3 && m_new;
            if (!stereo3 || ch.left)
                left += value;
            if (!stereo3 || ch.right)
                right += value;
        };
        for (int c = 0; c < channels; ++c) {
            const int local = c % 9;
            if (c < 9 && m_rhythm && local >= 6)
                continue;
            // OPL3 four-operator pairs: channels 0-2 with 3-5 of each bank.
            const int pairBit = (c / 9) * 3 + (local % 3);
            const bool fourOp = m_opl3 && m_new && local < 6 && (m_fourOp & (1 << pairBit));
            if (fourOp && local >= 3)
                continue;
            if (!fourOp) {
                mix(m_ch[c], runTwoOp(c, 0));
                continue;
            }
            Channel &c1 = m_ch[c];
            Channel &c2 = m_ch[c + 3];
            Op &a = m_ops[c1.op[0]];
            Op &b = m_ops[c1.op[1]];
            Op &cc = m_ops[c2.op[0]];
            Op &d = m_ops[c2.op[1]];
            const int fb = c1.fb ? (a.out1 + a.out2) >> (9 - c1.fb) : 0;
            const int ao = opOutput(a, c1, static_cast<int>(a.phase >> 10) + fb);
            a.out2 = a.out1;
            a.out1 = ao;
            auto run = [&](const Op &op, const Channel &ch, int mod) {
                return opOutput(op, ch, static_cast<int>(op.phase >> 10) + mod);
            };
            int out;
            switch (c1.cnt * 2 + c2.cnt) {
            case 0: {
                const int bo = run(b, c1, ao);
                const int co = run(cc, c2, bo);
                out = run(d, c2, co);
                break;
            }
            case 1:
                out = run(b, c1, ao) + run(d, c2, run(cc, c2, 0));
                break;
            case 2:
                out = ao + run(d, c2, run(cc, c2, run(b, c1, 0)));
                break;
            default:
                out = ao + run(cc, c2, run(b, c1, 0)) + run(d, c2, 0);
                break;
            }
            mix(c1, out);
        }

        if (m_rhythm) {
            // Bass drum: channel 6 as a two-operator voice, at twice the level.
            mix(m_ch[6], 2 * runTwoOp(6, 0));
            const int p13 = static_cast<int>(m_ops[13].phase >> 10);
            const int p17 = static_cast<int>(m_ops[17].phase >> 10);
            const int noise = static_cast<int>(m_noise & 1);
            const int res1 = (((p13 >> 2) ^ (p13 >> 7)) | (p13 >> 3)) & 1;
            const int res2 = ((p17 >> 3) ^ (p17 >> 5)) & 1;
            const int res = res1 | res2;
            const int hh = (res << 9) | ((res ^ noise) ? 0xd0 : 0x34);
            const int bit8 = (p13 >> 8) & 1;
            const int sd = (bit8 << 9) | ((bit8 ^ noise) << 8);
            const int tc = (res << 9) | 0x100;
            int drums = opOutput(m_ops[13], m_ch[7], hh) + opOutput(m_ops[16], m_ch[7], sd);
            drums += opOutput(m_ops[14], m_ch[8], static_cast<int>(m_ops[14].phase >> 10));
            drums += opOutput(m_ops[17], m_ch[8], tc);
            mix(m_ch[7], 2 * drums);
        }
        stereo[2 * f] = left;
        stereo[2 * f + 1] = right;
    }
}

bool oplFrequency(float hz, int *fnum, int *block) {
    if (!(hz > 0.0f))
        return false;
    for (int b = 0; b < 8; ++b) {
        const double f = double(hz) * double(1 << (20 - b)) / 49716.0;
        if (f < 1023.5) {
            *fnum = static_cast<int>(std::lround(f));
            *block = b;
            return true;
        }
    }
    return false;
}

bool oplInstrument(const PatchData &p, quint8 regs[11]) {
    if (p.opCount != 2)
        return false;
    // The inverse of Patch::fromChipInstrument(): see qfmpatch.cpp for the times of the rates.
    auto attackRate = [](float t) {
        if (t <= 0.0005f)
            return 15;
        return std::clamp(static_cast<int>(std::lround(1.0 + std::log2(2.826 / double(t)))), 1, 14);
    };
    auto decayRate = [](float t) {
        if (t <= 0.0f)
            return 15;
        return std::clamp(static_cast<int>(std::lround(1.0 + std::log2(39.28 / double(t)))), 1, 15);
    };
    const bool sustaining = p.loop || p.duration >= 0.0f;
    for (int i = 0; i < 2; ++i) {
        const FmOperator &op = p.ops[i];
        int mult = 1;
        float best = 1e9f;
        for (int m = 0; m < 16; ++m) {
            const float d = std::fabs(oplMultiples[m] - op.ratio);
            if (d < best) {
                best = d;
                mult = m;
            }
        }
        quint8 chr = static_cast<quint8>(mult);
        if (sustaining)
            chr |= 0x20;
        if (op.keyScaleRate >= 0.25f)
            chr |= 0x10;
        if (i == 1 && p.lfoPitch > 0.0f)
            chr |= 0x40;
        if (i == 1 && p.lfoAmp > 0.0f)
            chr |= 0x80;
        static const float kslDb[4] = {0.0f, 3.0f, 1.5f, 6.0f};
        int ksl = 0;
        for (int k = 1; k < 4; ++k) {
            if (std::fabs(kslDb[k] - op.keyScaleLevel) < std::fabs(kslDb[ksl] - op.keyScaleLevel))
                ksl = k;
        }
        const int tl =
            op.level <= 0.0f
                ? 63
                : std::clamp(static_cast<int>(std::lround(-20.0 * std::log10(double(op.level)) / 0.75)), 0, 63);
        const float sustainDb = op.env.sustain <= 0.0f ? 96.0f : -20.0f * std::log10(op.env.sustain);
        const int sl = std::clamp(static_cast<int>(std::lround(double(sustainDb) / 3.0)), 0, 15);
        const float fullDecay = op.env.decay * 96.0f / std::max(std::min(sustainDb, 96.0f), 1.0f);
        regs[i] = chr;
        regs[2 + i] = static_cast<quint8>((ksl << 6) | tl);
        regs[4 + i] =
            static_cast<quint8>((attackRate(op.env.attack) << 4) | (op.env.decay <= 0.0f ? 15 : decayRate(fullDecay)));
        regs[6 + i] = static_cast<quint8>((sl << 4) | decayRate(op.env.release));
        regs[8 + i] = static_cast<quint8>(op.wave <= Waveform::QuarterSine ? static_cast<int>(op.wave) : 0);
    }
    regs[10] = static_cast<quint8>((static_cast<int>(std::lround(p.feedback)) << 1) | (p.algorithm & 1));
    return true;
}

} // namespace QtFM::Private

namespace QtFM {

std::unique_ptr<ChipCore> ChipCore::create(ChipType type) {
    switch (type) {
    case ChipType::OPL2:
        return std::make_unique<Private::OplCore>(false);
    case ChipType::OPL3:
        return std::make_unique<Private::OplCore>(true);
    default:
        return nullptr;
    }
}

} // namespace QtFM
