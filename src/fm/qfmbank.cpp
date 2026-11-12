// MIT License - see LICENSE

#include "qfmopl_p.h"
#include "qfmpatch_p.h"

#include <QtCore/qcborvalue.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtFM/qfmbank.h>
#include <QtFM/qfmsequence.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace QtFM {

namespace {

constexpr int kIbkInstruments = 128;
constexpr int kIbkSize = 4 + kIbkInstruments * 16 + kIbkInstruments * 9;

bool readBank(const QJsonObject &root, PatchBank &bank, QString *error) {
    if (root.value(QLatin1String("format")).toString() != QLatin1String("qtfm-bank")) {
        if (error)
            *error = QStringLiteral("not a qtfm-bank document");
        return false;
    }
    if (root.value(QLatin1String("version")).toInt(1) > 1) {
        if (error)
            *error = QStringLiteral("unsupported bank version");
        return false;
    }
    bank.setName(root.value(QLatin1String("name")).toString());
    const QJsonArray array = root.value(QLatin1String("patches")).toArray();
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        QString patchError;
        const Patch patch = Patch::fromJson(QJsonDocument(object).toJson(QJsonDocument::Compact), &patchError);
        if (!patch.isValid()) {
            if (error)
                *error = patchError;
            return false;
        }
        const QString key = patch.name().isEmpty() ? QStringLiteral("patch-%1").arg(bank.size()) : patch.name();
        if (!bank.contains(key))
            bank.insert(key, patch);
    }
    return true;
}

} // namespace

/*!
    \class QtFM::PatchBank
    \inmodule NtFM
    \brief The PatchBank class holds a named set of patches, such as all the sounds of a game.

    A bank is stored as one JSON document, which names each patch, or as CBOR. toCHeader() turns it
    into a C array for targets without a file system; fromChipBank() reads the 128 instruments of
    an AdLib IBK bank.

    \code
    const QtFM::PatchBank bank = QtFM::PatchBank::fromJson(file.readAll());
    engine.play(bank.patch(QStringLiteral("coin")));
    \endcode

    \differences
    The IBK percussion fields are ignored. BNK, INS and WOPL banks are not read.
    \enddifferences

    \sa QtFM::Patch
*/

/*!
    Constructs an empty bank.
*/
PatchBank::PatchBank() = default;

/*!
    Returns the bank in the JSON document \a json, or an empty bank, with the reason in \a error.
    The document is an object with \c format \c "qtfm-bank", a \c name and an array of patches
    in \c patches, each named by its \c name field.
*/
PatchBank PatchBank::fromJson(const QByteArray &json, QString *error) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    PatchBank bank;
    if (!doc.isObject()) {
        if (error)
            *error = doc.isNull() ? parseError.errorString() : QStringLiteral("a bank is a JSON object");
        return bank;
    }
    if (!readBank(doc.object(), bank, error))
        return PatchBank();
    return bank;
}

/*!
    Returns the bank in the CBOR encoding \a cbor, as toCbor() writes it, or an empty bank, with the
    reason in \a error.
*/
PatchBank PatchBank::fromCbor(const QByteArray &cbor, QString *error) {
    const QCborValue value = QCborValue::fromCbor(cbor);
    PatchBank bank;
    if (!value.isMap()) {
        if (error)
            *error = QStringLiteral("a bank is a CBOR map");
        return bank;
    }
    if (!readBank(value.toJsonValue().toObject(), bank, error))
        return PatchBank();
    return bank;
}

/*!
    Returns the instruments of the chip bank \a data in \a format as patches, or an empty bank, with
    the reason in \a error. An IBK bank gives its 128 OPL2 instruments, named as in the bank; an SBI
    instrument gives a bank of one.
*/
PatchBank PatchBank::fromChipBank(const QByteArray &data, InstrumentFormat format, QString *error) {
    PatchBank bank;
    if (format == InstrumentFormat::Sbi) {
        const Patch patch = Patch::fromChipInstrument(data, format, error);
        if (patch.isValid())
            bank.insert(patch.name().isEmpty() ? QStringLiteral("instrument") : patch.name(), patch);
        return bank;
    }
    if (data.size() < kIbkSize || !data.startsWith("IBK\x1a")) {
        if (error)
            *error = QStringLiteral("not an IBK bank");
        return bank;
    }
    const auto *bytes = reinterpret_cast<const quint8 *>(data.constData());
    for (int i = 0; i < kIbkInstruments; ++i) {
        const char *rawName = data.constData() + 4 + kIbkInstruments * 16 + i * 9;
        QString name = QString::fromLatin1(rawName, static_cast<qsizetype>(qstrnlen(rawName, 9))).trimmed();
        if (name.isEmpty() || bank.contains(name))
            name = QStringLiteral("instrument-%1").arg(i);
        bank.m_names.append(name);
        bank.m_patches.append(Patch(Private::patchFromOpl(bytes + 4 + i * 16, name)));
    }
    return bank;
}

/*!
    Returns the bank as a JSON document.
*/
QByteArray PatchBank::toJson() const {
    QJsonObject root;
    root.insert(QLatin1String("format"), QLatin1String("qtfm-bank"));
    root.insert(QLatin1String("version"), 1);
    root.insert(QLatin1String("name"), m_name);
    QJsonArray patches;
    for (int i = 0; i < m_patches.size(); ++i) {
        QJsonObject object = QJsonDocument::fromJson(m_patches.at(i).toJson()).object();
        object.insert(QLatin1String("name"), m_names.at(i));
        patches.append(object);
    }
    root.insert(QLatin1String("patches"), patches);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

/*!
    Returns the bank encoded as CBOR.
*/
QByteArray PatchBank::toCbor() const {
    return QCborValue::fromJsonValue(QJsonDocument::fromJson(toJson()).object()).toCbor();
}

/*!
    Returns the bank as a chip bank in \a format, or an empty array, with the reason in \a error.
    For InstrumentFormat::Ibk, the bank must have at most 128 patches of two operators; the rest of
    the 128 instruments are left empty.
*/
QByteArray PatchBank::toChipBank(InstrumentFormat format, QString *error) const {
    if (format != InstrumentFormat::Ibk || m_patches.size() > kIbkInstruments) {
        if (error)
            *error = format != InstrumentFormat::Ibk ? QStringLiteral("not a bank format")
                                                     : QStringLiteral("an IBK bank holds 128 instruments");
        return QByteArray();
    }
    QByteArray ibk(kIbkSize, '\0');
    std::copy_n("IBK\x1a", 4, ibk.data());
    for (int i = 0; i < m_patches.size(); ++i) {
        quint8 regs[11];
        if (!m_patches.at(i).data() || !Private::oplInstrument(*m_patches.at(i).data(), regs)) {
            if (error)
                *error = QStringLiteral("%1: an IBK instrument needs a patch of two operators").arg(m_names.at(i));
            return QByteArray();
        }
        std::copy_n(reinterpret_cast<const char *>(regs), 11, ibk.data() + 4 + i * 16);
        const QByteArray name = m_names.at(i).toLatin1().left(8);
        std::copy_n(name.constData(), name.size(), ibk.data() + 4 + kIbkInstruments * 16 + i * 9);
    }
    return ibk;
}

/*!
    Returns a C header that defines the bank's CBOR encoding as \c{static const unsigned char}
    \a symbol and its size as \c{symbol_size}, for targets without a file system. Load it with
    fromCbor(), through QByteArray::fromRawData().
*/
QByteArray PatchBank::toCHeader(const QString &symbol) const {
    QByteArray id = symbol.toLatin1();
    for (char &c : id) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
            c = '_';
    }
    if (id.isEmpty() || std::isdigit(static_cast<unsigned char>(id.at(0))))
        id.prepend('_');
    const QByteArray cbor = toCbor();
    QByteArray out =
        "// Generated by Nt FM from the bank \"" + m_name.toUtf8().replace('"', '\'') + "\".\n#pragma once\n\n";
    out += "static const unsigned char " + id + "[] = {";
    for (qsizetype i = 0; i < cbor.size(); ++i) {
        out += (i % 16 == 0) ? "\n    " : " ";
        out += "0x" + QByteArray::number(static_cast<quint8>(cbor.at(i)), 16).rightJustified(2, '0') + ",";
    }
    out += "\n};\nstatic const unsigned int " + id + "_size = " + QByteArray::number(cbor.size()) + "u;\n";
    return out;
}

/*!
    Returns the name of the bank.
*/
QString PatchBank::name() const { return m_name; }

/*!
    Sets the name of the bank to \a name.
*/
void PatchBank::setName(const QString &name) { m_name = name; }

/*!
    Returns the number of patches.
*/
int PatchBank::size() const { return static_cast<int>(m_patches.size()); }

/*!
    Returns \c true if the bank has no patches.
*/
bool PatchBank::isEmpty() const { return m_patches.isEmpty(); }

/*!
    Returns the names of the patches, in order.
*/
QStringList PatchBank::names() const { return m_names; }

/*!
    Returns \c true if the bank has a patch named \a name.
*/
bool PatchBank::contains(const QString &name) const { return m_names.contains(name); }

/*!
    Returns the patch named \a name, or an invalid patch.
*/
Patch PatchBank::patch(const QString &name) const {
    const qsizetype i = m_names.indexOf(name);
    return i < 0 ? Patch() : m_patches.at(i);
}

/*!
    Returns the patch at \a index, or an invalid patch.
*/
Patch PatchBank::at(int index) const { return index < 0 || index >= m_patches.size() ? Patch() : m_patches.at(index); }

/*!
    Adds \a patch as \a name, replacing a patch of that name. The patch in the bank takes \a name as
    its Patch::name().
*/
void PatchBank::insert(const QString &name, const Patch &patch) {
    Patch named = patch;
    if (named.d && named.d->name != name)
        named.d->name = name;
    const qsizetype i = m_names.indexOf(name);
    if (i >= 0) {
        m_patches[i] = named;
        return;
    }
    m_names.append(name);
    m_patches.append(named);
}

/*!
    Removes the patch named \a name; returns \c false if there is none.
*/
bool PatchBank::remove(const QString &name) {
    const qsizetype i = m_names.indexOf(name);
    if (i < 0)
        return false;
    m_names.removeAt(i);
    m_patches.removeAt(i);
    return true;
}

/*!
    Returns \c true if this bank and \a other have the same name and patches.
*/
bool PatchBank::operator==(const PatchBank &other) const {
    return m_name == other.m_name && m_names == other.m_names && m_patches == other.m_patches;
}

/*!
    \fn bool QtFM::PatchBank::operator!=(const QtFM::PatchBank &other) const

    Returns \c true if this bank and \a other differ.
*/

/*!
    \class QtFM::Sequence
    \inmodule NtFM
    \brief The Sequence class holds timed notes on a tempo grid, for jingles and short cues.

    Notes are placed on beats; Engine::play() starts all of them at once, each on its exact sample.

    \snippet fm/fmplay.cpp sequence

    \differences
    Notes cannot be added while the sequence plays, and there is no looping or swing.
    \enddifferences
*/

/*!
    \class QtFM::Sequence::Note
    \inmodule NtFM
    \brief The Note struct is one note of a Sequence.
*/

/*!
    \variable QtFM::Sequence::Note::beat
    The beat that the note starts on, from 0.
*/

/*!
    \variable QtFM::Sequence::Note::patch
    The sound of the note.
*/

/*!
    \variable QtFM::Sequence::Note::pitch
    Semitones added to the patch's base note.
*/

/*!
    \variable QtFM::Sequence::Note::length
    The length in beats before release, or -1 for the patch's own length.
*/

/*!
    \variable QtFM::Sequence::Note::gain
    Gain in decibels.
*/

/*!
    Constructs an empty sequence at 120 beats a minute.
*/
Sequence::Sequence() = default;

/*!
    Sets the tempo to \a beatsPerMinute, from 1 to 1000.
*/
void Sequence::setTempo(float beatsPerMinute) {
    if (std::isfinite(beatsPerMinute))
        m_tempo = std::clamp(beatsPerMinute, 1.0f, 1000.0f);
}

/*!
    Returns the tempo in beats a minute.
*/
float Sequence::tempo() const { return m_tempo; }

/*!
    Adds a note of \a patch on \a beat, transposed by \a pitch semitones, released after \a length
    beats (-1 for the patch's own length) and at \a gain decibels.
*/
void Sequence::addNote(float beat, const Patch &patch, float pitch, float length, float gain) {
    if (!patch.isValid() || !std::isfinite(beat) || beat < 0.0f)
        return;
    m_notes.append(Note{beat, patch, pitch, length, gain});
}

/*!
    Removes every note.
*/
void Sequence::clear() { m_notes.clear(); }

/*!
    Returns the notes, in the order they were added.
*/
QList<Sequence::Note> Sequence::notes() const { return m_notes; }

/*!
    Returns the number of notes.
*/
int Sequence::noteCount() const { return static_cast<int>(m_notes.size()); }

/*!
    Returns the length of the sequence in beats: the end of its last note, counting a note without a
    length as one beat.
*/
float Sequence::duration() const {
    float end = 0;
    for (const Note &n : m_notes)
        end = std::max(end, n.beat + (n.length >= 0.0f ? n.length : 1.0f));
    return end;
}

/*!
    Returns the time in seconds of \a beat at the sequence's tempo.
*/
double Sequence::secondsAt(float beat) const { return double(beat) * 60.0 / double(m_tempo); }

} // namespace QtFM
