// MIT License - see LICENSE
//
// Standard MIDI Files on the FM engine. MidiFile turns a file into notes with times and lengths in
// seconds, following its tempo map, the sustain pedal and each channel's volume, expression, pan
// and pitch bend. MidiPlayer streams the notes into an Engine a little ahead of time, each placed
// on its own sample with Engine::playAt(), using a bank of FM instruments: by default a General
// MIDI bank of one patch per instrument family and a drum kit. Input is untrusted: every read is
// bounds-checked and the number of notes is capped.

#include "qfmopl_p.h"
#include "qfmpatch_p.h"

#include <QtCore/qendian.h>
#include <QtCore/qtimer.h>
#include <QtFM/qfmmidi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

namespace QtFM {

namespace {

constexpr qsizetype kMaxNotes = 1000000;

struct Event {
    quint64 tick;
    int order;
    quint8 status;
    quint8 a;
    quint8 b;
    quint32 tempo; // for tempo meta events, else 0
};

struct Track {
    const quint8 *data;
    std::size_t size;
};

bool readVlq(const quint8 *d, std::size_t size, std::size_t &pos, quint32 *value) {
    quint32 v = 0;
    for (int i = 0; i < 4; ++i) {
        if (pos >= size)
            return false;
        const quint8 b = d[pos++];
        v = (v << 7) | (b & 0x7fu);
        if (!(b & 0x80)) {
            *value = v;
            return true;
        }
    }
    return false;
}

// Appends the events of one track. Malformed data ends the track where it breaks.
void readTrack(const Track &t, int &order, std::vector<Event> &events) {
    std::size_t pos = 0;
    quint64 tick = 0;
    quint8 running = 0;
    while (pos < t.size && events.size() < std::size_t(kMaxNotes) * 4) {
        quint32 delta = 0;
        if (!readVlq(t.data, t.size, pos, &delta) || pos >= t.size)
            return;
        tick += delta;
        quint8 status = t.data[pos];
        if (status == 0xff) {
            if (pos + 2 > t.size)
                return;
            const quint8 type = t.data[pos + 1];
            pos += 2;
            quint32 length = 0;
            if (!readVlq(t.data, t.size, pos, &length) || length > t.size - pos)
                return;
            if (type == 0x51 && length >= 3) {
                const quint32 tempo = (quint32(t.data[pos]) << 16) | (quint32(t.data[pos + 1]) << 8) | t.data[pos + 2];
                if (tempo > 0)
                    events.push_back({tick, order++, 0xff, 0, 0, tempo});
            } else if (type == 0x2f) {
                return;
            }
            pos += length;
            continue;
        }
        if (status == 0xf0 || status == 0xf7) {
            ++pos;
            quint32 length = 0;
            if (!readVlq(t.data, t.size, pos, &length) || length > t.size - pos)
                return;
            pos += length;
            continue;
        }
        if (status & 0x80) {
            running = status;
            ++pos;
        } else if (!running) {
            return; // data without a status
        }
        status = running;
        const int kind = status >> 4;
        const std::size_t params = (kind == 0xc || kind == 0xd) ? 1 : 2;
        if (params > t.size - pos)
            return;
        const quint8 a = t.data[pos] & 0x7f;
        const quint8 b = params == 2 ? t.data[pos + 1] & 0x7f : 0;
        pos += params;
        events.push_back({tick, order++, status, a, b, 0});
    }
}

float levelDb(int value) { return value <= 0 ? -96.0f : 40.0f * std::log10(float(value) / 127.0f); }

} // namespace

/*!
    \class QtFM::MidiFile
    \inmodule NtFM
    \brief The MidiFile class reads the notes of a Standard MIDI File.

    fromData() reads formats 0, 1 and 2, with musical or SMPTE timing, and gives each note its time
    and length in seconds according to the file's tempo changes. The sustain pedal lengthens notes;
    each note starts with the channel's program, volume and expression (in its gain), pan and pitch
    bend, and controls() lists the changes of those that MidiPlayer applies to notes already
    sounding. MidiPlayer plays the notes on an Engine.

    \differences
    Aftertouch, system exclusive messages and the controllers other than volume, expression, pan,
    sustain, the pitch-bend range and the resets are ignored.
    \enddifferences
*/

/*!
    \class QtFM::MidiFile::Note
    \inmodule NtFM
    \brief The Note struct is one note of a MidiFile.
*/

/*!
    \class QtFM::MidiFile::Control
    \inmodule NtFM
    \brief The Control struct is a change of a channel's level, pan or pitch bend.
*/

/*!
    \enum QtFM::MidiFile::Control::Kind

    What changed.

    \value Level The channel's level in decibels, from its volume and expression.
    \value Pan The channel's pan, from -1 to 1.
    \value Bend The channel's pitch bend in semitones.
*/

/*!
    \variable QtFM::MidiFile::Control::time
    When the change happens, in seconds.
*/

/*!
    \variable QtFM::MidiFile::Control::channel
    The channel that changed.
*/

/*!
    \variable QtFM::MidiFile::Control::kind
    What changed.
*/

/*!
    \variable QtFM::MidiFile::Control::value
    The new value: decibels, a pan or semitones, by kind.
*/

/*!
    \variable QtFM::MidiFile::Note::time
    The start of the note in seconds.
*/

/*!
    \variable QtFM::MidiFile::Note::length
    The length of the note in seconds, including the time the sustain pedal held it.
*/

/*!
    \variable QtFM::MidiFile::Note::channel
    The MIDI channel, from 0; channel 9 is the General MIDI drum channel.
*/

/*!
    \variable QtFM::MidiFile::Note::program
    The program (instrument) of the channel, from 0.
*/

/*!
    \variable QtFM::MidiFile::Note::key
    The MIDI key; 60 is middle C.
*/

/*!
    \variable QtFM::MidiFile::Note::velocity
    The velocity, 1 to 127.
*/

/*!
    \variable QtFM::MidiFile::Note::gain
    The gain in decibels from the velocity, the channel volume and the expression.
*/

/*!
    \variable QtFM::MidiFile::Note::pan
    The pan of the channel, from -1 to 1.
*/

/*!
    \variable QtFM::MidiFile::Note::bend
    The pitch bend of the channel in semitones.
*/

/*!
    Constructs an empty, invalid file.
*/
MidiFile::MidiFile() = default;

/*!
    Returns the file in \a data, or an invalid file, with the reason in \a error.
*/
MidiFile MidiFile::fromData(const QByteArray &data, QString *error) {
    auto fail = [error](const char *message) {
        if (error)
            *error = QString::fromLatin1(message);
        return MidiFile();
    };
    const auto *d = reinterpret_cast<const quint8 *>(data.constData());
    const auto size = static_cast<std::size_t>(data.size());
    // Some files wrap the MIDI data in a RIFF "RMID" container.
    std::size_t pos = 0;
    if (size >= 20 && data.startsWith("RIFF") && data.mid(8, 4) == "RMID")
        pos = 20;
    if (size < pos + 14 || std::memcmp(d + pos, "MThd", 4) != 0)
        return fail("not a MIDI file");
    const quint32 headerLength = qFromBigEndian<quint32>(d + pos + 4);
    if (headerLength < 6 || headerLength > size - pos - 8)
        return fail("bad MIDI header");
    MidiFile file;
    file.m_format = qFromBigEndian<quint16>(d + pos + 8);
    const int trackCount = qFromBigEndian<quint16>(d + pos + 10);
    const quint16 division = qFromBigEndian<quint16>(d + pos + 12);
    if (division == 0)
        return fail("bad MIDI time division");
    pos += 8 + headerLength;

    std::vector<Event> events;
    int order = 0;
    for (int t = 0; t < trackCount && pos + 8 <= size; ++t) {
        const quint32 length = qFromBigEndian<quint32>(d + pos + 4);
        const bool isTrack = std::memcmp(d + pos, "MTrk", 4) == 0;
        const std::size_t start = pos + 8;
        const std::size_t available = std::min<std::size_t>(length, size - start);
        if (isTrack) {
            readTrack(Track{d + start, available}, order, events);
            ++file.m_tracks;
        }
        pos = start + available;
    }
    if (!file.m_tracks)
        return fail("the MIDI file has no tracks");
    std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.tick < b.tick; });

    // Ticks to seconds: SMPTE divisions are fixed; musical ones follow the tempo map.
    const bool smpte = division & 0x8000;
    const double smpteSecondsPerTick =
        smpte ? 1.0 / (double(-static_cast<qint8>(division >> 8)) * double(division & 0xff)) : 0.0;
    double secondsPerTick = smpte ? smpteSecondsPerTick : 0.5 / division;
    quint64 lastTick = 0;
    double now = 0;

    struct Channel {
        int program = 0;
        int volume = 100;
        int expression = 127;
        int pan = 64;
        int bend = 8192;
        int bendRange = 2;
        int rpn = 0x3fff;
        bool sustain = false;
        std::array<std::deque<qsizetype>, 128> held;
        std::vector<qsizetype> sustained;
    };
    std::array<Channel, 16> channels;
    QList<Note> &notes = file.m_notes;
    QList<Control> &controls = file.m_controls;
    auto control = [&controls, &now](int channel, Control::Kind kind, float value) {
        if (controls.size() < kMaxNotes)
            controls.append(Control{now, channel, kind, value});
    };
    auto level = [](const Channel &c) { return levelDb(c.volume) + levelDb(c.expression); };
    auto pan = [](const Channel &c) { return std::clamp(float(c.pan - 64) / 63.0f, -1.0f, 1.0f); };
    auto bend = [](const Channel &c) { return float(c.bend - 8192) / 8192.0f * float(c.bendRange); };
    auto end = [&notes, &now](qsizetype index) { notes[index].length = std::max(now - notes[index].time, 0.005); };

    for (const Event &e : events) {
        now += double(e.tick - lastTick) * secondsPerTick;
        lastTick = e.tick;
        if (e.status == 0xff) {
            if (!smpte)
                secondsPerTick = e.tempo / 1e6 / division;
            continue;
        }
        Channel &ch = channels[e.status & 0x0f];
        const int channel = e.status & 0x0f;
        switch (e.status >> 4) {
        case 0x9:
            if (e.b > 0) {
                if (notes.size() >= kMaxNotes)
                    break;
                Note n;
                n.time = now;
                n.channel = channel;
                n.program = ch.program;
                n.key = e.a;
                n.velocity = e.b;
                n.gain = levelDb(e.b) + level(ch);
                n.pan = pan(ch);
                n.bend = bend(ch);
                ch.held[e.a].push_back(notes.size());
                notes.append(n);
                break;
            }
            [[fallthrough]];
        case 0x8:
            if (!ch.held[e.a].empty()) {
                const qsizetype index = ch.held[e.a].front();
                ch.held[e.a].pop_front();
                if (ch.sustain)
                    ch.sustained.push_back(index);
                else
                    end(index);
            }
            break;
        case 0xb:
            switch (e.a) {
            case 7:
                ch.volume = e.b;
                control(channel, Control::Level, level(ch));
                break;
            case 10:
                ch.pan = e.b;
                control(channel, Control::Pan, pan(ch));
                break;
            case 11:
                ch.expression = e.b;
                control(channel, Control::Level, level(ch));
                break;
            case 64:
                ch.sustain = e.b >= 64;
                if (!ch.sustain) {
                    for (qsizetype index : ch.sustained)
                        end(index);
                    ch.sustained.clear();
                }
                break;
            case 100:
                ch.rpn = (ch.rpn & 0x3f80) | e.b;
                break;
            case 101:
                ch.rpn = (ch.rpn & 0x7f) | (e.b << 7);
                break;
            case 6:
                if (ch.rpn == 0)
                    ch.bendRange = std::clamp(int(e.b), 0, 24);
                break;
            case 121: // reset all controllers
                ch.volume = 100;
                ch.expression = 127;
                ch.pan = 64;
                ch.bend = 8192;
                ch.sustain = false;
                control(channel, Control::Level, level(ch));
                control(channel, Control::Pan, pan(ch));
                control(channel, Control::Bend, bend(ch));
                break;
            case 120: // all sound off
            case 123: // all notes off
                for (auto &keys : ch.held) {
                    for (qsizetype index : keys)
                        end(index);
                    keys.clear();
                }
                for (qsizetype index : ch.sustained)
                    end(index);
                ch.sustained.clear();
                break;
            default:
                break;
            }
            break;
        case 0xc:
            ch.program = e.a;
            break;
        case 0xe:
            ch.bend = e.a | (e.b << 7);
            control(channel, Control::Bend, bend(ch));
            break;
        default:
            break;
        }
    }
    // Notes still sounding end with the last event.
    for (Channel &ch : channels) {
        for (auto &keys : ch.held) {
            for (qsizetype index : keys)
                end(index);
        }
        for (qsizetype index : ch.sustained)
            end(index);
    }
    for (const Note &n : std::as_const(notes))
        file.m_duration = std::max(file.m_duration, n.time + n.length);
    return file;
}

/*!
    Returns the changes of the channels' level, pan and pitch bend, in time order. MidiPlayer applies
    them to the notes that sound when they happen.
*/
QList<MidiFile::Control> MidiFile::controls() const { return m_controls; }

/*!
    Returns \c true if the file was read.
*/
bool MidiFile::isValid() const { return m_format >= 0; }

/*!
    Returns the notes, in the order they start.
*/
QList<MidiFile::Note> MidiFile::notes() const { return m_notes; }

/*!
    Returns the length of the music in seconds: the end of its last note.
*/
double MidiFile::duration() const { return m_duration; }

/*!
    Returns the format of the file: 0 for one track, 1 for parallel tracks, 2 for sequential ones;
    -1 for an invalid file.
*/
int MidiFile::format() const { return m_format; }

/*!
    Returns the number of tracks.
*/
int MidiFile::trackCount() const { return m_tracks; }

// --- The General MIDI bank ------------------------------------------------------------------------

namespace {

// One patch per General MIDI instrument family (programs 0-7, 8-15 and so on), at middle C.
const char *const familyNames[16] = {"piano",    "chromatic", "organ",      "guitar", "bass", "strings",
                                     "ensemble", "brass",     "reed",       "pipe",   "lead", "pad",
                                     "synth",    "ethnic",    "percussive", "effects"};

const char *const familyPatches[16] = {
    // Piano: a hammered pair over a bright attack.
    R"({"algorithm":4,"operators":[{"ratio":1,"level":0.45,"env":{"a":0,"d":1.5,"s":0,"r":0.3,"curve":-4}},
        {"ratio":1,"level":0.6,"keyScaleRate":0.3,"env":{"a":0.002,"d":2.5,"s":0,"r":0.3,"curve":-4}},
        {"ratio":7,"level":0.15,"env":{"a":0,"d":0.2,"s":0,"r":0.1}},
        {"ratio":2,"level":0.2,"env":{"a":0.002,"d":1.2,"s":0,"r":0.3,"curve":-4}}],"gain":0})",
    // Chromatic percussion: celesta, vibraphone, marimba.
    R"({"operators":[{"ratio":3.5,"level":0.35,"env":{"a":0,"d":0.4,"s":0,"r":0.2}},
        {"ratio":1,"level":0.6,"env":{"a":0,"d":1.5,"s":0,"r":0.4,"curve":-4}}],"gain":0})",
    // Organ: four drawbars.
    R"({"algorithm":7,"operators":[{"ratio":0.5,"level":0.3,"env":{"a":0.005,"d":0,"s":1,"r":0.06,"curve":0}},
        {"ratio":1,"level":0.45,"env":{"a":0.005,"d":0,"s":1,"r":0.06,"curve":0}},
        {"ratio":2,"level":0.3,"env":{"a":0.005,"d":0,"s":1,"r":0.06,"curve":0}},
        {"ratio":4,"level":0.15,"env":{"a":0.005,"d":0,"s":1,"r":0.06,"curve":0}}],"gain":-3})",
    // Guitar: a warm pluck, as a jazz guitar.
    R"({"feedback":1,"operators":[{"ratio":1,"level":0.35,"env":{"a":0,"d":0.6,"s":0.15,"r":0.15,"curve":-4}},
        {"ratio":1,"level":0.55,"keyScaleRate":0.2,"env":{"a":0.002,"d":1.4,"s":0.2,"r":0.2,"curve":-4}}],
        "filter":{"type":"lowpass","cutoff":3500,"resonance":0.1},"gain":1})",
    // Bass.
    R"({"feedback":2,"operators":[{"ratio":1,"level":0.45,"env":{"a":0,"d":0.4,"s":0.2,"r":0.1}},
        {"ratio":1,"level":0.7,"env":{"a":0.002,"d":1,"s":0.4,"r":0.1,"curve":-4}}],
        "filter":{"type":"lowpass","cutoff":2000},"gain":3})",
    // Strings: two detuned pairs with vibrato.
    R"({"algorithm":4,"operators":[{"ratio":1,"level":0.25,"env":{"a":0.2,"d":0.3,"s":0.8,"r":0.4,"curve":-2}},
        {"ratio":1,"level":0.35,"env":{"a":0.18,"d":0.3,"s":0.9,"r":0.4,"curve":-2}},
        {"ratio":1,"offset":0.7,"level":0.25,"env":{"a":0.2,"d":0.3,"s":0.8,"r":0.4,"curve":-2}},
        {"ratio":1,"offset":-0.5,"level":0.35,"env":{"a":0.18,"d":0.3,"s":0.9,"r":0.4,"curve":-2}}],
        "lfo":{"rate":5.5,"pitch":0.12},"filter":{"type":"lowpass","cutoff":5000},"gain":-3})",
    // Ensemble: strings and choirs, slower.
    R"({"algorithm":4,"operators":[{"ratio":1,"level":0.15,"env":{"a":0.35,"d":0.3,"s":0.8,"r":0.5,"curve":-2}},
        {"ratio":1,"level":0.35,"env":{"a":0.3,"d":0.3,"s":0.9,"r":0.5,"curve":-2}},
        {"ratio":2,"offset":0.6,"level":0.1,"env":{"a":0.35,"d":0.3,"s":0.8,"r":0.5,"curve":-2}},
        {"ratio":1,"offset":-0.6,"level":0.35,"env":{"a":0.3,"d":0.3,"s":0.9,"r":0.5,"curve":-2}}],
        "lfo":{"rate":5,"pitch":0.1},"gain":-3})",
    // Brass.
    R"({"feedback":3,"operators":[{"ratio":1,"level":0.55,"env":{"a":0.06,"d":0.2,"s":0.7,"r":0.15}},
        {"ratio":1,"level":0.55,"env":{"a":0.04,"d":0.15,"s":0.85,"r":0.15}}],
        "filter":{"type":"lowpass","cutoff":4500},"gain":0})",
    // Reed: saxophone, clarinet, oboe.
    R"({"feedback":4,"operators":[{"ratio":1,"level":0.4,"env":{"a":0.05,"d":0.2,"s":0.8,"r":0.1}},
        {"ratio":1,"level":0.55,"env":{"a":0.04,"d":0.1,"s":0.9,"r":0.1}}],"lfo":{"rate":5,"pitch":0.1},"gain":0})",
    // Pipe: flutes, with breath.
    R"({"operators":[{"ratio":1,"level":0.1,"env":{"a":0.08,"d":0.1,"s":0.9,"r":0.1}},
        {"ratio":1,"level":0.55,"env":{"a":0.06,"d":0.1,"s":0.9,"r":0.12}}],
        "noise":{"level":0.04,"env":{"a":0.06,"d":0.1,"s":0.9,"r":0.1}},"lfo":{"rate":5,"pitch":0.08},"gain":1})",
    // Synth lead.
    R"({"operators":[{"ratio":1,"level":0.2,"env":{"a":0,"d":0,"s":1,"r":0.1,"curve":0}},
        {"ratio":1,"level":0.4,"wave":"square","env":{"a":0.005,"d":0.1,"s":0.9,"r":0.1}}],
        "filter":{"type":"lowpass","cutoff":3500},"gain":-3})",
    // Synth pad: detuned saws.
    R"({"algorithm":7,"operators":[{"level":0},{"level":0},
        {"ratio":1,"level":0.25,"wave":"saw","env":{"a":0.5,"d":0.5,"s":0.8,"r":0.8,"curve":-2}},
        {"ratio":1,"offset":1.2,"level":0.25,"wave":"saw","env":{"a":0.5,"d":0.5,"s":0.8,"r":0.8,"curve":-2}}],
        "filter":{"type":"lowpass","cutoff":1800},"lfo":{"rate":0.3,"cutoff":0.3},"gain":-3})",
    // Synth effects.
    R"({"operators":[{"ratio":1.41,"level":0.4,"env":{"a":0.3,"d":0.5,"s":0.7,"r":1}},
        {"ratio":1,"level":0.45,"env":{"a":0.3,"d":0.5,"s":0.8,"r":1}}],"lfo":{"rate":0.7,"index":0.5},"gain":-3})",
    // Ethnic: sitar, banjo, shamisen.
    R"({"feedback":3,"operators":[{"ratio":3,"level":0.4,"env":{"a":0,"d":0.3,"s":0,"r":0.1}},
        {"ratio":1,"level":0.55,"env":{"a":0,"d":1,"s":0.1,"r":0.2,"curve":-4}}],"gain":0})",
    // Percussive: bells, steel drums, wood blocks.
    R"({"operators":[{"ratio":2.7,"level":0.5,"env":{"a":0,"d":0.15,"s":0,"r":0.05}},
        {"ratio":1,"level":0.6,"env":{"a":0,"d":0.5,"s":0,"r":0.1,"curve":-4}}],"gain":0})",
    // Sound effects: breath and noise.
    R"({"operators":[{"ratio":1,"level":0.3,"env":{"a":0.1,"d":0.3,"s":0.7,"r":0.5}},
        {"ratio":1,"level":0.3,"env":{"a":0.1,"d":0.3,"s":0.7,"r":0.5}}],
        "noise":{"level":0.3,"env":{"a":0.1,"d":0.3,"s":0.7,"r":0.5}},"filter":{"type":"bandpass","cutoff":1500},"gain":-3})",
};

// The same families for the OPL2 and OPL3: two operators each, as an AdLib driver would set them up.
// Sustained families are loops, so that their envelopes hold until the note is released.
const char *const oplFamilyPatches[16] = {
    R"({"feedback":1,"operators":[{"ratio":1,"level":0.35,"env":{"a":0,"d":1.2,"s":0,"r":0.3}},
        {"ratio":1,"level":1,"env":{"a":0,"d":2.5,"s":0,"r":0.3}}]})",
    R"({"operators":[{"ratio":4,"level":0.3,"env":{"a":0,"d":0.4,"s":0,"r":0.2}},
        {"ratio":1,"level":1,"env":{"a":0,"d":1.5,"s":0,"r":0.4}}]})",
    R"({"loop":true,"algorithm":1,"operators":[{"ratio":1,"level":0.7,"env":{"a":0,"d":0,"s":1,"r":0.06}},
        {"ratio":2,"level":0.6,"env":{"a":0,"d":0,"s":1,"r":0.06}}]})",
    R"({"feedback":2,"operators":[{"ratio":1,"level":0.35,"env":{"a":0,"d":0.6,"s":0.15,"r":0.15}},
        {"ratio":1,"level":1,"env":{"a":0,"d":1.4,"s":0.2,"r":0.2}}]})",
    R"({"feedback":3,"operators":[{"ratio":1,"level":0.5,"env":{"a":0,"d":0.4,"s":0.2,"r":0.1}},
        {"ratio":1,"level":1,"env":{"a":0,"d":1,"s":0.4,"r":0.1}}]})",
    R"({"loop":true,"feedback":1,"operators":[{"ratio":1,"level":0.3,"env":{"a":0.2,"d":0.3,"s":0.8,"r":0.4}},
        {"ratio":1,"level":1,"env":{"a":0.18,"d":0.3,"s":0.9,"r":0.4}}],"lfo":{"pitch":0.07}})",
    R"({"loop":true,"operators":[{"ratio":2,"level":0.15,"env":{"a":0.35,"d":0.3,"s":0.8,"r":0.5}},
        {"ratio":1,"level":1,"env":{"a":0.3,"d":0.3,"s":0.9,"r":0.5}}],"lfo":{"pitch":0.07}})",
    R"({"loop":true,"feedback":4,"operators":[{"ratio":1,"level":0.55,"env":{"a":0.06,"d":0.2,"s":0.7,"r":0.15}},
        {"ratio":1,"level":1,"env":{"a":0.04,"d":0.15,"s":0.85,"r":0.15}}]})",
    R"({"loop":true,"feedback":5,"operators":[{"ratio":1,"level":0.4,"env":{"a":0.05,"d":0.2,"s":0.8,"r":0.1}},
        {"ratio":1,"level":1,"wave":"half-sine","env":{"a":0.04,"d":0.1,"s":0.9,"r":0.1}}],"lfo":{"pitch":0.07}})",
    R"({"loop":true,"operators":[{"ratio":1,"level":0.1,"env":{"a":0.08,"d":0.1,"s":0.9,"r":0.1}},
        {"ratio":1,"level":1,"env":{"a":0.06,"d":0.1,"s":0.9,"r":0.12}}],"lfo":{"pitch":0.07}})",
    R"({"loop":true,"feedback":6,"operators":[{"ratio":1,"level":0.5,"env":{"a":0,"d":0,"s":1,"r":0.1}},
        {"ratio":1,"level":1,"wave":"quarter-sine","env":{"a":0,"d":0.1,"s":0.9,"r":0.1}}]})",
    R"({"loop":true,"operators":[{"ratio":1,"level":0.2,"env":{"a":0.5,"d":0.5,"s":0.8,"r":0.8}},
        {"ratio":1,"level":1,"env":{"a":0.5,"d":0.5,"s":0.8,"r":0.8}}]})",
    R"({"loop":true,"operators":[{"ratio":2,"level":0.4,"env":{"a":0.3,"d":0.5,"s":0.7,"r":1}},
        {"ratio":1,"level":1,"env":{"a":0.3,"d":0.5,"s":0.8,"r":1}}]})",
    R"({"feedback":3,"operators":[{"ratio":3,"level":0.4,"env":{"a":0,"d":0.3,"s":0,"r":0.1}},
        {"ratio":1,"level":1,"env":{"a":0,"d":1,"s":0.1,"r":0.2}}]})",
    R"({"operators":[{"ratio":3,"level":0.5,"env":{"a":0,"d":0.15,"s":0,"r":0.05}},
        {"ratio":1,"level":1,"env":{"a":0,"d":0.5,"s":0,"r":0.1}}]})",
    R"({"loop":true,"feedback":7,"operators":[{"ratio":1,"level":0.8,"env":{"a":0.1,"d":0.3,"s":0.7,"r":0.5}},
        {"ratio":1,"level":1,"env":{"a":0.1,"d":0.3,"s":0.7,"r":0.5}}]})",
};

// The drum kit: the patches are one-shots at their own pitch.
const struct {
    const char *name;
    const char *json;
} drumPatches[] = {
    {"kick", R"({"operators":[{"level":0},{"ratio":1,"level":1,"env":{"a":0,"d":0.4,"s":0,"r":0.05,"curve":-4}}],
        "pitch":{"base":28,"env":{"start":24,"end":0,"time":0.04,"curve":-3}},"gain":6})"},
    {"snare", R"({"operators":[{"level":0},{"ratio":1,"level":0.5,"env":{"a":0,"d":0.1,"s":0,"r":0.02}}],
        "pitch":{"base":50},"noise":{"level":0.7,"env":{"a":0,"d":0.2,"s":0,"r":0.05,"curve":-4}},"gain":4})"},
    {"clap",
     R"({"operators":[{"level":0},{"level":0}],"noise":{"level":0.8,"env":{"a":0,"d":0.12,"s":0,"r":0.03,"curve":-5}},
        "filter":{"type":"bandpass","cutoff":1500,"resonance":0.3},"gain":6})"},
    {"rim", R"({"operators":[{"level":0},{"ratio":1,"level":0.5,"env":{"a":0,"d":0.04,"s":0,"r":0.01}}],
        "pitch":{"base":84},"noise":{"level":0.3,"env":{"a":0,"d":0.03,"s":0,"r":0.01}},"gain":3})"},
    {"hihat",
     R"({"operators":[{"level":0},{"level":0}],"noise":{"level":0.6,"env":{"a":0,"d":0.05,"s":0,"r":0.02,"curve":-5}},
        "filter":{"type":"highpass","cutoff":8000},"gain":3})"},
    {"openhat",
     R"({"operators":[{"level":0},{"level":0}],"noise":{"level":0.5,"env":{"a":0,"d":0.4,"s":0,"r":0.1,"curve":-4}},
        "filter":{"type":"highpass","cutoff":7000},"gain":2})"},
    {"tom", R"({"operators":[{"level":0},{"ratio":1,"level":0.9,"env":{"a":0,"d":0.35,"s":0,"r":0.05,"curve":-4}}],
        "pitch":{"base":45,"env":{"start":5,"end":0,"time":0.1}},"gain":5})"},
    {"crash",
     R"({"operators":[{"level":0},{"level":0}],"noise":{"level":0.5,"env":{"a":0,"d":1.5,"s":0,"r":0.3,"curve":-4}},
        "filter":{"type":"highpass","cutoff":5000},"gain":1})"},
    {"ride", R"({"operators":[{"ratio":3.7,"level":0.3,"env":{"a":0,"d":0.8,"s":0,"r":0.2}},
        {"fixed":1200,"level":0.15,"env":{"a":0,"d":0.8,"s":0,"r":0.2,"curve":-4}}],
        "noise":{"level":0.25,"env":{"a":0,"d":1,"s":0,"r":0.2,"curve":-4}},"filter":{"type":"highpass","cutoff":6000},"gain":0})"},
};

// The drum of each General MIDI key, 35 to 81, by name; the rest are rim clicks.
const char *drumOfKey(int key) {
    switch (key) {
    case 35:
    case 36:
        return "kick";
    case 38:
    case 40:
        return "snare";
    case 39:
        return "clap";
    case 42:
    case 44:
        return "hihat";
    case 46:
        return "openhat";
    case 41:
    case 43:
    case 45:
    case 47:
    case 48:
    case 50:
        return "tom";
    case 49:
    case 52:
    case 55:
    case 57:
        return "crash";
    case 51:
    case 53:
    case 59:
        return "ride";
    default:
        return "rim";
    }
}

} // namespace

/*!
    Returns a General MIDI bank of FM instruments for \a chip: one patch for each family of eight programs,
    named \c gm-piano, \c gm-chromatic, \c gm-organ, \c gm-guitar, \c gm-bass, \c gm-strings,
    \c gm-ensemble, \c gm-brass, \c gm-reed, \c gm-pipe, \c gm-lead, \c gm-pad, \c gm-synth,
    \c gm-ethnic, \c gm-percussive and \c gm-effects, all at middle C; and a drum kit of one-shot
    patches named \c drum-kick, \c drum-snare, \c drum-clap, \c drum-rim, \c drum-hihat,
    \c drum-openhat, \c drum-tom, \c drum-crash and \c drum-ride.

    For ChipType::OPL2 or ChipType::OPL3, the families are two-operator OPL2 instruments, as an
    AdLib driver would program them, and play on that chip when the engine has it
    (EngineConfig::chips); the velocity of each note sets the chip's total level. The drums stay on
    the native engine. For other chips, the native bank is returned.

    \sa QtFM::MidiPlayer::setInstruments()
*/
PatchBank PatchBank::generalMidi(ChipType chip) {
    const bool opl = chip == ChipType::OPL2 || chip == ChipType::OPL3;
    PatchBank bank;
    bank.setName(opl ? QStringLiteral("general-midi-opl") : QStringLiteral("general-midi"));
    for (int f = 0; f < 16; ++f) {
        QByteArray json = opl ? oplFamilyPatches[f] : familyPatches[f];
        json.insert(1, QByteArray(R"("pitch":{"base":60},)")); // every family is at middle C
        const QString name = QStringLiteral("gm-") + QLatin1String(familyNames[f]);
        Patch patch = Patch::fromJson(json);
        quint8 regs[11];
        if (opl && patch.data() && Private::oplInstrument(*patch.data(), regs)) {
            // Through the chip's registers and back, so that every value is one the chip has.
            patch = Patch(Private::patchFromOpl(regs, name));
            patch.d->baseNote = 60;
        }
        bank.insert(name, patch);
    }
    for (const auto &drum : drumPatches)
        bank.insert(QStringLiteral("drum-") + QLatin1String(drum.name), Patch::fromJson(drum.json));
    return bank;
}

// --- MidiPlayer -----------------------------------------------------------------------------------

/*!
    \class QtFM::MidiPlayer
    \inmodule NtFM
    \brief The MidiPlayer class plays a MidiFile on an Engine with FM instruments.

    The player streams the notes into the engine a fraction of a second ahead of time, placing each
    on its own sample with Engine::playAt(), so the timing is exact whatever the event loop does.
    Instruments come from a PatchBank, by default PatchBank::generalMidi(): for a note of program
    \e n, the patch named \c{program-n} if the bank has one, else the patch of its General MIDI
    family; on channel 10, \c{drum-k} for key \e k, else the kit's drum for that key.

    \snippet fm/fmplay.cpp midi

    For an engine that is not started, call schedule() before each Engine::render().

    \differences
    The native engine plays the notes: there is no OPL rendition of General MIDI yet. Each note
    uses one voice, so give the engine enough voices (EngineConfig::maxVoices) for the music.
    \enddifferences
*/

/*!
    Constructs a player for \a engine, with \a parent.
*/
MidiPlayer::MidiPlayer(Engine *engine, QObject *parent) : QObject(parent), m_engine(engine), m_timer(new QTimer(this)) {
    setInstruments(PatchBank::generalMidi());
    connect(m_timer, &QTimer::timeout, this, &MidiPlayer::tick);
}

/*!
    Destroys the player, releasing its notes.
*/
MidiPlayer::~MidiPlayer() {
    for (Tracked &t : m_voices)
        t.voice.stop();
}

/*!
    Sets the music to \a file, stopping what plays.
*/
void MidiPlayer::setFile(const MidiFile &file) {
    stop();
    m_file = file;
}

/*!
    Returns the music.
*/
MidiFile MidiPlayer::file() const { return m_file; }

/*!
    Sets the instruments to the patches of \a bank.
*/
void MidiPlayer::setInstruments(const PatchBank &bank) {
    m_bank = bank;
    m_programs.clear();
    m_drums.clear();
    for (int p = 0; p < 128; ++p) {
        Patch patch = bank.patch(QStringLiteral("program-%1").arg(p));
        if (!patch.isValid())
            patch = bank.patch(QStringLiteral("gm-") + QLatin1String(familyNames[p / 8]));
        m_programs.append(patch);
    }
    for (int k = 0; k < 128; ++k) {
        Patch patch = bank.patch(QStringLiteral("drum-%1").arg(k));
        if (!patch.isValid())
            patch = bank.patch(QStringLiteral("drum-") + QLatin1String(drumOfKey(k)));
        m_drums.append(patch);
    }
}

/*!
    Returns the instruments.
*/
PatchBank MidiPlayer::instruments() const { return m_bank; }

/*!
    Returns the patch that plays key \a key of program \a program on \a channel.
*/
Patch MidiPlayer::instrument(int channel, int program, int key) const {
    if (channel == 9)
        return m_drums.value(std::clamp(key, 0, 127));
    return m_programs.value(std::clamp(program, 0, 127));
}

/*!
    Sets whether the music starts again when it ends to \a loops.
*/
void MidiPlayer::setLoops(bool loops) { m_loops = loops; }

/*!
    Returns \c true if the music loops.
*/
bool MidiPlayer::loops() const { return m_loops; }

/*!
    Sets the gain of the music to \a dB, for the notes not yet started.
*/
void MidiPlayer::setGain(float dB) { m_gain = dB; }

/*!
    Returns the gain of the music in decibels.
*/
float MidiPlayer::gain() const { return m_gain; }

/*!
    Sets the transposition of every note but the drums to \a semitones.
*/
void MidiPlayer::setTranspose(int semitones) { m_transpose = semitones; }

/*!
    Returns the transposition in semitones.
*/
int MidiPlayer::transpose() const { return m_transpose; }

/*!
    Plays the music from its start.
*/
void MidiPlayer::play() {
    stop();
    if (!m_engine || !m_file.isValid())
        return;
    const EngineConfig config = m_engine->config();
    // Two blocks ahead, so the first notes are not late on a started engine.
    m_start = m_engine->frame() + (m_engine->isStarted() ? quint64(2 * config.blockSize) : 0);
    m_origin = m_start;
    m_next = 0;
    m_nextControl = 0;
    m_playing = true;
    schedule(m_engine->frame() + quint64(config.sampleRate / 5));
    if (m_engine->isStarted())
        m_timer->start(10);
}

/*!
    Stops the music, releasing its notes.
*/
void MidiPlayer::stop() {
    m_timer->stop();
    for (Tracked &t : m_voices)
        t.voice.stop();
    m_voices.clear();
    m_playing = false;
}

/*!
    Returns \c true while the music plays.
*/
bool MidiPlayer::isPlaying() const { return m_playing; }

/*!
    Returns the position in the music in seconds; for looping music, within the current pass.
*/
double MidiPlayer::position() const {
    if (!m_engine || !m_playing)
        return 0;
    const quint64 now = m_engine->frame();
    const double elapsed = now > m_origin ? double(now - m_origin) / m_engine->config().sampleRate : 0.0;
    return m_loops && m_file.duration() > 0 ? std::fmod(elapsed, m_file.duration()) : elapsed;
}

/*!
    Starts the notes that begin before the engine's frame \a untilFrame. A started engine is fed by
    a timer; for an engine that is pulled with Engine::render(), call it before each render with
    the frame the render will reach. Emits finished() once the music has ended.
*/
void MidiPlayer::schedule(quint64 untilFrame) {
    if (!m_playing || !m_engine)
        return;
    const double rate = m_engine->config().sampleRate;
    const QList<MidiFile::Note> notes = m_file.notes();
    const QList<MidiFile::Control> controls = m_file.controls();
    const auto loopFrames = static_cast<quint64>(std::max(m_file.duration(), 0.01) * rate);
    auto frameOf = [this, rate](double time) { return m_start + static_cast<quint64>(std::llround(time * rate)); };
    for (;;) {
        // Notes and controller changes in time order; a note before a change at the same time.
        for (;;) {
            const quint64 noteFrame = m_next < notes.size() ? frameOf(notes.at(m_next).time) : ~quint64(0);
            const quint64 controlFrame =
                m_nextControl < controls.size() ? frameOf(controls.at(m_nextControl).time) : ~quint64(0);
            if (std::min(noteFrame, controlFrame) >= untilFrame)
                break;
            if (noteFrame <= controlFrame)
                startNote(notes.at(m_next++), noteFrame);
            else
                applyControl(controls.at(m_nextControl++), controlFrame);
        }
        if (m_next < notes.size() || m_nextControl < controls.size() || !m_loops)
            break;
        m_start += loopFrames;
        m_next = 0;
        m_nextControl = 0;
    }
    const quint64 now = m_engine->frame();
    if (m_voices.size() > 128) {
        m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(),
                                      [now](const Tracked &t) { return t.end < now && !t.voice.isPlaying(); }),
                       m_voices.end());
    }
    // Finished once the last note has been released and its release has rung out.
    if (m_next >= notes.size() && !m_loops && now >= m_start + loopFrames + quint64(rate)) {
        m_playing = false;
        m_timer->stop();
        emit finished();
    }
}

void MidiPlayer::startNote(const MidiFile::Note &n, quint64 frame) {
    const Patch patch = instrument(n.channel, n.program, n.key);
    if (!patch.isValid())
        return;
    const double rate = m_engine->config().sampleRate;
    PlayOptions o;
    o.gain = n.gain + m_gain;
    o.pan = n.pan;
    o.bus = 1;
    if (n.channel == 9) {
        o.priority = 1;
        if (patch.name() == QLatin1String("drum-tom"))
            o.pitch = float(n.key - 45) * 1.5f;
    } else {
        o.pitch = float(n.key + m_transpose) - patch.data()->baseNote + n.bend;
        o.duration = static_cast<float>(n.length);
    }
    o.seed = static_cast<quint32>(m_next) + 1u;
    const VoiceHandle v = m_engine->playAt(patch, o, frame);
    if (!v.isValid())
        return;
    // The voice takes controller changes until a second after its note-off, while it rings out.
    const auto end = frame + static_cast<quint64>((n.length + 1.0) * rate);
    m_voices.append(Tracked{v, n.channel, frame, end,
                            n.gain - (n.velocity > 0 ? 40.0f * std::log10(float(n.velocity) / 127.0f) : 0.0f), n.bend});
}

void MidiPlayer::applyControl(const MidiFile::Control &c, quint64 frame) {
    for (Tracked &t : m_voices) {
        if (t.channel != c.channel || t.start > frame || t.end < frame)
            continue;
        switch (c.kind) {
        case MidiFile::Control::Level:
            t.voice.setParameterAt(ParamId::Gain, c.value - t.level, frame);
            break;
        case MidiFile::Control::Pan:
            t.voice.setParameterAt(ParamId::Pan, c.value, frame);
            break;
        case MidiFile::Control::Bend:
            if (t.channel != 9)
                t.voice.setParameterAt(ParamId::Pitch, c.value - t.bend, frame);
            break;
        }
    }
}

void MidiPlayer::tick() {
    if (m_engine)
        schedule(m_engine->frame() + quint64(m_engine->config().sampleRate / 5));
}

/*!
    \fn void QtFM::MidiPlayer::finished()

    This signal is emitted when music that does not loop has ended.
*/

} // namespace QtFM

#include "moc_qfmmidi.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
