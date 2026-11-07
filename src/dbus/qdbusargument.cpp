// MIT License - see LICENSE
#include "qdbusmarshaller_p.h"

#include <QtCore/qdebug.h>
#include <QtDBus/qdbusmetatype.h>

#include <algorithm>
#include <cstring>
#include <type_traits>

/*!
    \class QDBusArgument
    \brief The QDBusArgument class is used to marshall and demarshall D-Bus arguments.

    To send a type of your own, give it stream operators and register it with
    qDBusRegisterMetaType(). A structure maps to a D-Bus structure with beginStructure() and
    endStructure():

    \code
    struct Fix { double lat; double lon; };
    Q_DECLARE_METATYPE(Fix)

    QDBusArgument &operator<<(QDBusArgument &arg, const Fix &fix) {
        arg.beginStructure();
        arg << fix.lat << fix.lon;
        arg.endStructure();
        return arg;
    }
    const QDBusArgument &operator>>(const QDBusArgument &arg, Fix &fix) {
        arg.beginStructure();
        arg >> fix.lat >> fix.lon;
        arg.endStructure();
        return arg;
    }

    qDBusRegisterMetaType<Fix>();
    \endcode

    Containers, structures and dictionaries of a received message that Nt has no C++ type for
    arrive in a QVariant as a QDBusArgument. qdbus_cast() turns one into a type that is
    registered:

    \code
    QVariantMap map = qdbus_cast<QVariantMap>(reply.arguments().first());
    \endcode

    A QDBusArgument that was default-constructed is for writing; one that came from a message is
    for reading. Reading from a copy does not move the position of the original.

    \differences
    The stream operators for QDate, QTime, QDateTime and the geometry types are missing. A
    \c std::pair takes the place of QPair, and \c std::tuple maps to a structure as well.
    Marshalling \c long and \c{unsigned long} is supported because Nt's \c qint64 is \c long on
    Linux. A value of the wrong type is not read: the function prints a warning, returns a
    default value and skips the value on the wire.
    \enddifferences

    \sa QDBusMetaType, QDBusVariant, QDBusMessage
*/

/*!
    \enum QDBusArgument::ElementType

    Describes the next value of an argument that is being read.

    \value BasicType A basic type: a number, a string, an object path or a signature.
    \value VariantType A D-Bus variant.
    \value ArrayType An array, read with beginArray().
    \value StructureType A structure, read with beginStructure().
    \value MapType A dictionary, read with beginMap().
    \value MapEntryType An entry of a dictionary, read with beginMapEntry().
    \value UnknownType There is no value at this position.
*/

namespace {

template <typename T> T byteSwapped(T value) {
    char *bytes = reinterpret_cast<char *>(&value);
    std::reverse(bytes, bytes + sizeof(T));
    return value;
}

char charAt(const QByteArray &bytes, int i) { return i >= 0 && i < bytes.size() ? bytes.at(i) : '\0'; }

} // namespace

// ---- QDBusWriter ---------------------------------------------------------------------------

void QDBusWriter::fail(const QString &why) {
    if (!failed)
        errorText = why;
    failed = true;
}

void QDBusWriter::note(char code) {
    if (!m_skipSignature)
        signature.append(code);
}

void QDBusWriter::note(const QByteArray &code) {
    if (!m_skipSignature)
        signature.append(code);
}

void QDBusWriter::align(int n) {
    const qsizetype pad = (n - data.size() % n) % n;
    if (pad)
        data.append(pad, '\0');
}

template <typename T> void QDBusWriter::put(T value, char code) {
    note(code);
    align(static_cast<int>(sizeof(T)));
    data.append(reinterpret_cast<const char *>(&value), static_cast<qsizetype>(sizeof(T)));
}

void QDBusWriter::writeByte(uchar v) { put(v, 'y'); }
void QDBusWriter::writeBool(bool v) { put(static_cast<uint>(v ? 1 : 0), 'b'); }
void QDBusWriter::writeInt16(short v) { put(v, 'n'); }
void QDBusWriter::writeUInt16(ushort v) { put(v, 'q'); }
void QDBusWriter::writeInt32(int v) { put(v, 'i'); }
void QDBusWriter::writeUInt32(uint v) { put(v, 'u'); }
void QDBusWriter::writeInt64(qlonglong v) { put(v, 'x'); }
void QDBusWriter::writeUInt64(qulonglong v) { put(v, 't'); }
void QDBusWriter::writeDouble(double v) { put(v, 'd'); }

void QDBusWriter::writeString(const QString &v) {
    note('s');
    const QByteArray utf8 = v.toUtf8();
    align(4);
    const uint size = static_cast<uint>(utf8.size());
    data.append(reinterpret_cast<const char *>(&size), 4);
    data.append(utf8.constData(), utf8.size());
    data.append('\0');
}

void QDBusWriter::writeObjectPath(const QString &v) {
    if (!QDBusUtil::isValidObjectPath(v))
        fail(QStringLiteral("Invalid object path: %1").arg(v));
    note('o');
    const QByteArray utf8 = v.toUtf8();
    align(4);
    const uint size = static_cast<uint>(utf8.size());
    data.append(reinterpret_cast<const char *>(&size), 4);
    data.append(utf8.constData(), utf8.size());
    data.append('\0');
}

void QDBusWriter::writeSignature(const QByteArray &v) {
    if (!QDBusUtil::isValidSignature(v))
        fail(QStringLiteral("Invalid signature: %1").arg(QString::fromLatin1(v)));
    note('g');
    data.append(static_cast<char>(v.size()));
    data.append(v.constData(), v.size());
    data.append('\0');
}

void QDBusWriter::writeUnixFd(const QDBusUnixFileDescriptor &v) {
    if (!v.isValid())
        fail(QStringLiteral("Invalid file descriptor"));
    const uint index = static_cast<uint>(fds.size());
    fds.append(v);
    put(index, 'h');
}

void QDBusWriter::writeByteArray(const QByteArray &v) {
    beginArray("y");
    data.append(v.constData(), v.size());
    endArray();
}

void QDBusWriter::writeDBusVariant(const QVariant &inner) {
    QByteArray sig = signatureOf(inner);
    note('v');
    ++m_skipSignature;
    if (sig.isEmpty() || !QDBusUtil::isValidSingleSignature(sig)) {
        fail(QStringLiteral("Cannot put a value of type %1 in a D-Bus variant")
                 .arg(QString::fromLatin1(inner.typeName() ? inner.typeName() : "invalid")));
        // Keep the data well formed: typeToSignature() marshals default-constructed values.
        writeSignature("i");
        writeInt32(0);
    } else {
        writeSignature(sig);
        appendVariant(inner);
    }
    --m_skipSignature;
}

void QDBusWriter::beginStructure() {
    note('(');
    align(8);
}

void QDBusWriter::endStructure() { note(')'); }

void QDBusWriter::beginArray(const QByteArray &elementSignature) {
    if (elementSignature.isEmpty()) {
        fail(QStringLiteral("An array needs an element type with a D-Bus signature"));
        note("ai");
    } else {
        note('a');
        note(elementSignature);
    }
    ++m_skipSignature;
    align(4);
    const uint size = 0;
    const int lengthAt = static_cast<int>(data.size());
    data.append(reinterpret_cast<const char *>(&size), 4);
    align(elementSignature.isEmpty() ? 4 : QDBusUtil::alignmentOf(elementSignature.at(0)));
    m_arrays.push_back({lengthAt, static_cast<int>(data.size())});
}

void QDBusWriter::endArray() {
    if (m_arrays.empty()) {
        fail(QStringLiteral("endArray() without beginArray()"));
        return;
    }
    const ArrayFrame frame = m_arrays.back();
    m_arrays.pop_back();
    --m_skipSignature;
    const qsizetype bytes = data.size() - frame.start;
    if (bytes > QDBusUtil::kMaxArrayBytes)
        fail(QStringLiteral("Array is too large for D-Bus"));
    const uint size = static_cast<uint>(bytes);
    std::memcpy(data.data() + frame.lengthAt, &size, 4);
}

void QDBusWriter::beginMap(const QByteArray &keySignature, const QByteArray &valueSignature) {
    if (keySignature.size() != 1 || valueSignature.isEmpty()) {
        fail(QStringLiteral("A map needs a basic key type and a value type with a D-Bus signature"));
        beginArray("{sv}");
        return;
    }
    beginArray("{" + keySignature + valueSignature + "}");
}

void QDBusWriter::endMap() { endArray(); }

void QDBusWriter::beginMapEntry() { align(8); }

void QDBusWriter::endMapEntry() {}

QByteArray QDBusWriter::signatureOf(const QVariant &value) {
    if (!value.isValid())
        return QByteArray();
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QDBusArgument argument = value.value<QDBusArgument>();
        return argument.currentSignature().toLatin1();
    }
    return QByteArray(QDBusMetaType::typeToSignature(value.metaType()));
}

void QDBusWriter::appendArgument(const QDBusArgument &argument) {
    QDBusReader reader = QDBusArgumentPrivate::readerOf(argument);
    while (!reader.atEnd())
        reader.copyValue(*this);
}

void QDBusWriter::appendVariant(const QVariant &value) {
    const QMetaType type = value.metaType();
    switch (type.id()) {
    case QMetaType::UnknownType:
        fail(QStringLiteral("Cannot send an invalid QVariant over D-Bus"));
        return;
    case QMetaType::UChar:
        return writeByte(value.value<uchar>());
    case QMetaType::Bool:
        return writeBool(value.toBool());
    case QMetaType::Short:
        return writeInt16(value.value<short>());
    case QMetaType::UShort:
        return writeUInt16(value.value<ushort>());
    case QMetaType::Int:
        return writeInt32(value.toInt());
    case QMetaType::UInt:
        return writeUInt32(value.toUInt());
    case QMetaType::LongLong:
    case QMetaType::Long:
        return writeInt64(value.toLongLong());
    case QMetaType::ULongLong:
    case QMetaType::ULong:
        return writeUInt64(value.toULongLong());
    case QMetaType::Double:
        return writeDouble(value.toDouble());
    case QMetaType::QString:
        return writeString(value.toString());
    case QMetaType::QByteArray:
        return writeByteArray(value.toByteArray());
    case QMetaType::QStringList: {
        beginArray("s");
        const QStringList list = value.toStringList();
        for (const QString &item : list)
            writeString(item);
        endArray();
        return;
    }
    default:
        break;
    }
    if (type == QMetaType::fromType<QDBusVariant>())
        return writeDBusVariant(value.value<QDBusVariant>().variant());
    if (type == QMetaType::fromType<QDBusObjectPath>())
        return writeObjectPath(value.value<QDBusObjectPath>().path());
    if (type == QMetaType::fromType<QDBusSignature>())
        return writeSignature(value.value<QDBusSignature>().signature().toLatin1());
    if (type == QMetaType::fromType<QDBusUnixFileDescriptor>())
        return writeUnixFd(value.value<QDBusUnixFileDescriptor>());
    if (type == QMetaType::fromType<QDBusArgument>())
        return appendArgument(value.value<QDBusArgument>());
    QDBusArgument argument = QDBusArgumentPrivate::forWriter(this);
    if (!QDBusMetaType::marshall(argument, type, value.constData()))
        fail(QStringLiteral("Type %1 is not registered with the D-Bus type system")
                 .arg(QString::fromLatin1(value.typeName())));
}

// ---- QDBusReader ---------------------------------------------------------------------------

QDBusReader::QDBusReader(const QByteArray &data, int pos, const QByteArray &signature, bool swapped,
                         const QList<QDBusUnixFileDescriptor> &fds)
    : m_data(data), m_pos(pos), m_signature(signature), m_swapped(swapped), m_fds(fds) {}

void QDBusReader::corrupt() {
    m_failed = true;
    m_frames.clear();
    m_pos = static_cast<int>(m_data.size());
    m_sigPos = static_cast<int>(m_signature.size());
}

bool QDBusReader::need(int n) const { return n >= 0 && m_data.size() - m_pos >= n; }

bool QDBusReader::alignTo(int n) {
    const int aligned = (m_pos + n - 1) / n * n;
    if (aligned > m_data.size()) {
        corrupt();
        return false;
    }
    m_pos = aligned;
    return true;
}

bool QDBusReader::atEnd() const { return peek() == 0; }

char QDBusReader::peek() const {
    if (m_failed)
        return 0;
    int sigPos = m_sigPos;
    if (!m_frames.empty()) {
        const Frame &f = m_frames.back();
        if (f.kind == 'a' || f.kind == 'e') {
            if (m_pos >= f.end)
                return 0;
            sigPos = f.elementSignature;
        }
    }
    if (sigPos >= m_signature.size())
        return 0;
    const char c = m_signature.at(sigPos);
    return c == ')' || c == '}' ? 0 : c;
}

QByteArray QDBusReader::currentSignature() const {
    if (m_failed)
        return QByteArray();
    if (m_frames.empty())
        return m_signature.mid(m_sigPos);
    const Frame &f = m_frames.back();
    const int from = f.kind == 'a' || f.kind == 'e' ? f.elementSignature : m_sigPos;
    if (peek() == 0)
        return QByteArray();
    const int to = QDBusUtil::skipType(m_signature, from);
    return to < 0 ? QByteArray() : m_signature.mid(from, to - from);
}

QDBusArgument::ElementType QDBusReader::currentType() const {
    const char c = peek();
    switch (c) {
    case 0:
        return QDBusArgument::UnknownType;
    case 'v':
        return QDBusArgument::VariantType;
    case '(':
        return QDBusArgument::StructureType;
    case 'a': {
        const int sigPos = !m_frames.empty() && (m_frames.back().kind == 'a' || m_frames.back().kind == 'e')
                               ? m_frames.back().elementSignature
                               : m_sigPos;
        return charAt(m_signature, sigPos + 1) == '{' ? QDBusArgument::MapType : QDBusArgument::ArrayType;
    }
    case '{':
        return QDBusArgument::MapEntryType;
    default:
        return QDBusArgument::BasicType;
    }
}

bool QDBusReader::expect(char code) {
    const char c = peek();
    if (c == code) {
        if (!m_frames.empty() && (m_frames.back().kind == 'a' || m_frames.back().kind == 'e'))
            m_sigPos = m_frames.back().elementSignature;
        return true;
    }
    if (c == 0) {
        qWarning("QDBusArgument: read past the end of the argument (wanted '%c')", code);
        return false;
    }
    qWarning("QDBusArgument: read a '%c' where the message has a '%c'", code, c);
    skipValue();
    return false;
}

template <typename T> T QDBusReader::get(char code) {
    if (!expect(code))
        return T();
    if (!alignTo(static_cast<int>(sizeof(T))) || !need(static_cast<int>(sizeof(T)))) {
        corrupt();
        return T();
    }
    T value;
    std::memcpy(&value, m_data.constData() + m_pos, sizeof(T));
    m_pos += static_cast<int>(sizeof(T));
    ++m_sigPos;
    return m_swapped ? byteSwapped(value) : value;
}

uchar QDBusReader::readByte() { return get<uchar>('y'); }
bool QDBusReader::readBool() { return get<uint>('b') != 0; }
short QDBusReader::readInt16() { return get<short>('n'); }
ushort QDBusReader::readUInt16() { return get<ushort>('q'); }
int QDBusReader::readInt32() { return get<int>('i'); }
uint QDBusReader::readUInt32() { return get<uint>('u'); }
qlonglong QDBusReader::readInt64() { return get<qlonglong>('x'); }
qulonglong QDBusReader::readUInt64() { return get<qulonglong>('t'); }
double QDBusReader::readDouble() { return get<double>('d'); }

QByteArray QDBusReader::rawString(int lengthBytes) {
    uint size = 0;
    if (lengthBytes == 1) {
        if (!need(1)) {
            corrupt();
            return QByteArray();
        }
        size = static_cast<uchar>(m_data.at(m_pos));
        ++m_pos;
    } else {
        if (!alignTo(4) || !need(4)) {
            corrupt();
            return QByteArray();
        }
        std::memcpy(&size, m_data.constData() + m_pos, 4);
        m_pos += 4;
        if (m_swapped)
            size = byteSwapped(size);
    }
    if (size >= static_cast<uint>(QDBusUtil::kMaxMessageBytes) || !need(static_cast<int>(size) + 1)) {
        corrupt();
        return QByteArray();
    }
    QByteArray text(m_data.constData() + m_pos, static_cast<qsizetype>(size));
    m_pos += static_cast<int>(size) + 1;
    ++m_sigPos;
    return text;
}

QString QDBusReader::readString() {
    if (!expect('s'))
        return QString();
    return QString::fromUtf8(rawString(4));
}

QString QDBusReader::readObjectPath() {
    if (!expect('o'))
        return QString();
    return QString::fromUtf8(rawString(4));
}

QString QDBusReader::readSignature() {
    if (!expect('g'))
        return QString();
    return QString::fromLatin1(rawString(1));
}

QDBusUnixFileDescriptor QDBusReader::readUnixFd() {
    const uint index = get<uint>('h');
    return index < static_cast<uint>(m_fds.size()) ? m_fds.at(static_cast<qsizetype>(index))
                                                   : QDBusUnixFileDescriptor();
}

QVariant QDBusReader::readDBusVariant() {
    if (!expect('v'))
        return QVariant();
    const QByteArray sig = rawString(1);
    if (m_failed)
        return QVariant();
    // rawString() has stepped over the 'v' in the signature.
    QDBusReader inner(m_data, m_pos, sig, m_swapped, m_fds);
    QVariant value = inner.readValue();
    if (inner.m_failed) {
        corrupt();
        return QVariant();
    }
    m_pos = inner.m_pos;
    return value;
}

QStringList QDBusReader::readStringList() {
    QStringList list;
    beginArray();
    while (!atEnd())
        list.append(readString());
    endArray();
    return list;
}

QByteArray QDBusReader::readByteArray() {
    if (peek() != 'a' || currentSignature() != "ay") {
        qWarning("QDBusArgument: read a byte array where the message has a '%s'", currentSignature().constData());
        skipValue();
        return QByteArray();
    }
    beginArray();
    QByteArray bytes;
    if (!m_frames.empty() && !m_failed)
        bytes = QByteArray(m_data.constData() + m_pos, m_frames.back().end - m_pos);
    endArray();
    return bytes;
}

void QDBusReader::beginStructure() {
    if (!expect('('))
        return;
    if (!alignTo(8))
        return;
    const int after = QDBusUtil::skipType(m_signature, m_sigPos);
    m_frames.push_back({'(', 0, 0, after});
    ++m_sigPos;
}

void QDBusReader::close(char kind) {
    if (m_failed)
        return;
    if (m_frames.empty() || m_frames.back().kind != kind) {
        qWarning("QDBusArgument: unbalanced end of a container");
        return;
    }
    // Whatever the caller did not read is skipped.
    while (!atEnd())
        skipValue();
    if (m_failed)
        return;
    const Frame f = m_frames.back();
    m_frames.pop_back();
    if (kind == 'a' || kind == 'e')
        m_pos = f.end;
    m_sigPos = f.afterSignature;
}

void QDBusReader::endStructure() { close('('); }

void QDBusReader::beginArray() {
    if (!expect('a'))
        return;
    if (!alignTo(4) || !need(4)) {
        corrupt();
        return;
    }
    uint size = 0;
    std::memcpy(&size, m_data.constData() + m_pos, 4);
    m_pos += 4;
    if (m_swapped)
        size = byteSwapped(size);
    const int after = QDBusUtil::skipType(m_signature, m_sigPos);
    const char element = charAt(m_signature, m_sigPos + 1);
    if (after < 0 || !alignTo(QDBusUtil::alignmentOf(element)) || size > static_cast<uint>(QDBusUtil::kMaxArrayBytes) ||
        !need(static_cast<int>(size))) {
        corrupt();
        return;
    }
    m_frames.push_back({element == '{' ? 'e' : 'a', m_pos + static_cast<int>(size), m_sigPos + 1, after});
}

void QDBusReader::endArray() { close(!m_frames.empty() && m_frames.back().kind == 'e' ? 'e' : 'a'); }

void QDBusReader::beginMap() {
    if (peek() != 'a' || currentType() != QDBusArgument::MapType) {
        qWarning("QDBusArgument: read a map where the message has a '%s'", currentSignature().constData());
        skipValue();
        // The matching endMap() and the loop in between have nothing to do.
        m_frames.push_back({'e', m_pos, m_sigPos, m_sigPos});
        return;
    }
    beginArray();
}

void QDBusReader::endMap() { close('e'); }

void QDBusReader::beginMapEntry() {
    if (!expect('{'))
        return;
    if (!alignTo(8))
        return;
    const int after = QDBusUtil::skipType(m_signature, m_sigPos);
    m_frames.push_back({'{', 0, 0, after});
    ++m_sigPos;
}

void QDBusReader::endMapEntry() { close('{'); }

QDBusReader QDBusReader::sliceOfNextValue() const {
    QDBusReader slice(m_data, m_pos, currentSignature(), m_swapped, m_fds);
    return slice;
}

QVariant QDBusReader::readValue() {
    switch (peek()) {
    case 'y':
        return QVariant::fromValue(readByte());
    case 'b':
        return QVariant(readBool());
    case 'n':
        return QVariant::fromValue(readInt16());
    case 'q':
        return QVariant::fromValue(readUInt16());
    case 'i':
        return QVariant(readInt32());
    case 'u':
        return QVariant(readUInt32());
    case 'x':
        return QVariant(readInt64());
    case 't':
        return QVariant(readUInt64());
    case 'd':
        return QVariant(readDouble());
    case 's':
        return QVariant(readString());
    case 'o':
        return QVariant::fromValue(QDBusObjectPath(readObjectPath()));
    case 'g':
        return QVariant::fromValue(QDBusSignature(readSignature()));
    case 'h':
        return QVariant::fromValue(readUnixFd());
    case 'v':
        return QVariant::fromValue(QDBusVariant(readDBusVariant()));
    case 'a': {
        const QByteArray sig = currentSignature();
        if (sig == "as")
            return QVariant(readStringList());
        if (sig == "ay")
            return QVariant(readByteArray());
        break;
    }
    case '(':
        break;
    default:
        return QVariant();
    }
    const QDBusArgument argument = QDBusArgumentPrivate::forReader(sliceOfNextValue());
    skipValue();
    return QVariant::fromValue(argument);
}

void QDBusReader::skipValue() {
    switch (peek()) {
    case 0:
        return;
    case 'y':
        (void)readByte();
        return;
    case 'b':
        (void)readBool();
        return;
    case 'n':
        (void)readInt16();
        return;
    case 'q':
        (void)readUInt16();
        return;
    case 'i':
        (void)readInt32();
        return;
    case 'u':
        (void)readUInt32();
        return;
    case 'h':
        (void)get<uint>('h');
        return;
    case 'x':
        (void)readInt64();
        return;
    case 't':
        (void)readUInt64();
        return;
    case 'd':
        (void)readDouble();
        return;
    case 's':
    case 'o':
        expect(peek());
        (void)rawString(4);
        return;
    case 'g':
        expect('g');
        (void)rawString(1);
        return;
    case 'v':
        (void)readDBusVariant();
        return;
    case 'a': {
        beginArray();
        if (m_frames.empty())
            return;
        // Nothing needs the elements, so step over them.
        m_pos = m_frames.back().end;
        close(m_frames.back().kind);
        return;
    }
    case '(':
        beginStructure();
        endStructure();
        return;
    case '{':
        beginMapEntry();
        endMapEntry();
        return;
    default:
        corrupt();
        return;
    }
}

void QDBusReader::copyValue(QDBusWriter &writer) {
    switch (peek()) {
    case 'y':
        return writer.writeByte(readByte());
    case 'b':
        return writer.writeBool(readBool());
    case 'n':
        return writer.writeInt16(readInt16());
    case 'q':
        return writer.writeUInt16(readUInt16());
    case 'i':
        return writer.writeInt32(readInt32());
    case 'u':
        return writer.writeUInt32(readUInt32());
    case 'x':
        return writer.writeInt64(readInt64());
    case 't':
        return writer.writeUInt64(readUInt64());
    case 'd':
        return writer.writeDouble(readDouble());
    case 's':
        return writer.writeString(readString());
    case 'o':
        return writer.writeObjectPath(readObjectPath());
    case 'g':
        return writer.writeSignature(readSignature().toLatin1());
    case 'h':
        return writer.writeUnixFd(readUnixFd());
    case 'v':
        return writer.writeDBusVariant(readDBusVariant());
    case '(':
        writer.beginStructure();
        beginStructure();
        while (!atEnd())
            copyValue(writer);
        endStructure();
        writer.endStructure();
        return;
    case '{':
        writer.beginMapEntry();
        beginMapEntry();
        while (!atEnd())
            copyValue(writer);
        endMapEntry();
        writer.endMapEntry();
        return;
    case 'a': {
        const QByteArray element = currentSignature().mid(1);
        writer.beginArray(element);
        beginArray();
        while (!atEnd())
            copyValue(writer);
        endArray();
        writer.endArray();
        return;
    }
    default:
        return;
    }
}

// ---- QDBusArgumentPrivate ------------------------------------------------------------------

QDBusArgumentPrivate::QDBusArgumentPrivate(QDBusWriter *w)
    : writer(w ? std::shared_ptr<QDBusWriter>(w, [](QDBusWriter *) {}) : std::make_shared<QDBusWriter>()) {}

QDBusArgumentPrivate::QDBusArgumentPrivate(QDBusReader r) : reader(std::move(r)) {}

QDBusArgument QDBusArgumentPrivate::forWriter(QDBusWriter *writer) {
    return QDBusArgument(std::make_shared<QDBusArgumentPrivate>(writer));
}

QDBusArgument QDBusArgumentPrivate::forReader(QDBusReader reader) {
    return QDBusArgument(std::make_shared<QDBusArgumentPrivate>(std::move(reader)));
}

QDBusReader QDBusArgumentPrivate::readerOf(const QDBusArgument &argument) {
    QDBusArgumentPrivate *d = argument.d.get();
    if (!d->writer)
        return d->reader;
    return QDBusReader(d->writer->data, 0, d->writer->signature, false, d->writer->fds);
}

// ---- QDBusArgument -------------------------------------------------------------------------

/*!
    Constructs an empty argument for writing.
*/
QDBusArgument::QDBusArgument() : d(std::make_shared<QDBusArgumentPrivate>(static_cast<QDBusWriter *>(nullptr))) {}

QDBusArgument::QDBusArgument(std::shared_ptr<QDBusArgumentPrivate> p) : d(std::move(p)) {}

/*!
    Constructs a copy of \a other. Reading from one of them does not move the position of the
    other; writing to one writes to both.
*/
QDBusArgument::QDBusArgument(const QDBusArgument &other) = default;

/*!
    Constructs an argument that takes the place of \a other.
*/
QDBusArgument::QDBusArgument(QDBusArgument &&other) noexcept = default;

/*!
    Makes this argument a copy of \a other and returns a reference to it.
*/
QDBusArgument &QDBusArgument::operator=(const QDBusArgument &other) = default;

/*!
    Moves \a other into this argument and returns a reference to it.
*/
QDBusArgument &QDBusArgument::operator=(QDBusArgument &&other) noexcept = default;

/*!
    Destroys the argument.
*/
QDBusArgument::~QDBusArgument() = default;

/*!
    Swaps this argument with \a other.
*/
void QDBusArgument::swap(QDBusArgument &other) noexcept { d.swap(other.d); }

QDBusArgumentPrivate *QDBusArgument::writing() const {
    if (!d->writer) {
        qWarning("QDBusArgument: write to an argument that is for reading");
        return nullptr;
    }
    return d.get();
}

// The position of a copy must stay where it is, so reading detaches.
QDBusArgumentPrivate *QDBusArgument::reading() const {
    if (d->writer) {
        qWarning("QDBusArgument: read from an argument that is for writing");
        return nullptr;
    }
    if (d.use_count() > 1)
        d = std::make_shared<QDBusArgumentPrivate>(d->reader);
    return d.get();
}

/*!
    Appends the byte \a arg and returns a reference to this argument.
*/
QDBusArgument &QDBusArgument::operator<<(uchar arg) {
    if (auto *p = writing())
        p->writer->writeByte(arg);
    return *this;
}

/*!
    \overload
    Appends the boolean \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(bool arg) {
    if (auto *p = writing())
        p->writer->writeBool(arg);
    return *this;
}

/*!
    \overload
    Appends the 16 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(short arg) {
    if (auto *p = writing())
        p->writer->writeInt16(arg);
    return *this;
}

/*!
    \overload
    Appends the unsigned 16 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(ushort arg) {
    if (auto *p = writing())
        p->writer->writeUInt16(arg);
    return *this;
}

/*!
    \overload
    Appends the 32 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(int arg) {
    if (auto *p = writing())
        p->writer->writeInt32(arg);
    return *this;
}

/*!
    \overload
    Appends the unsigned 32 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(uint arg) {
    if (auto *p = writing())
        p->writer->writeUInt32(arg);
    return *this;
}

/*!
    \overload
    Appends the 64 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(qlonglong arg) {
    if (auto *p = writing())
        p->writer->writeInt64(arg);
    return *this;
}

/*!
    \overload
    Appends the unsigned 64 bit integer \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(qulonglong arg) {
    if (auto *p = writing())
        p->writer->writeUInt64(arg);
    return *this;
}

/*!
    \overload
    Appends \a arg as a 64 bit integer.
*/
QDBusArgument &QDBusArgument::operator<<(long arg) { return *this << static_cast<qlonglong>(arg); }

/*!
    \overload
    Appends \a arg as an unsigned 64 bit integer.
*/
QDBusArgument &QDBusArgument::operator<<(unsigned long arg) { return *this << static_cast<qulonglong>(arg); }

/*!
    \overload
    Appends the floating-point number \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(double arg) {
    if (auto *p = writing())
        p->writer->writeDouble(arg);
    return *this;
}

/*!
    \overload
    Appends the string \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(const QString &arg) {
    if (auto *p = writing())
        p->writer->writeString(arg);
    return *this;
}

/*!
    \overload
    Appends the variant \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(const QDBusVariant &arg) {
    if (auto *p = writing())
        p->writer->writeDBusVariant(arg.variant());
    return *this;
}

/*!
    \overload
    Appends the object path \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(const QDBusObjectPath &arg) {
    if (auto *p = writing())
        p->writer->writeObjectPath(arg.path());
    return *this;
}

/*!
    \overload
    Appends the signature \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(const QDBusSignature &arg) {
    if (auto *p = writing())
        p->writer->writeSignature(arg.signature().toLatin1());
    return *this;
}

/*!
    \overload
    Appends the file descriptor \a arg.
*/
QDBusArgument &QDBusArgument::operator<<(const QDBusUnixFileDescriptor &arg) {
    if (auto *p = writing())
        p->writer->writeUnixFd(arg);
    return *this;
}

/*!
    \overload
    Appends \a arg as an array of strings.
*/
QDBusArgument &QDBusArgument::operator<<(const QStringList &arg) {
    if (auto *p = writing())
        p->writer->appendVariant(QVariant(arg));
    return *this;
}

/*!
    \overload
    Appends \a arg as an array of bytes.
*/
QDBusArgument &QDBusArgument::operator<<(const QByteArray &arg) {
    if (auto *p = writing())
        p->writer->writeByteArray(arg);
    return *this;
}

/*!
    Opens a structure. The values that follow are its members, up to endStructure().
*/
void QDBusArgument::beginStructure() {
    if (auto *p = writing())
        p->writer->beginStructure();
}

/*!
    Closes the structure that beginStructure() opened.
*/
void QDBusArgument::endStructure() {
    if (auto *p = writing())
        p->writer->endStructure();
}

/*!
    Opens an array with elements of the type \a elementMetaType, which must be known to the D-Bus
    type system. The values that follow are its elements, up to endArray().
*/
void QDBusArgument::beginArray(QMetaType elementMetaType) {
    if (auto *p = writing())
        p->writer->beginArray(QByteArray(QDBusMetaType::typeToSignature(elementMetaType)));
}

/*!
    Closes the array that beginArray() opened.
*/
void QDBusArgument::endArray() {
    if (auto *p = writing())
        p->writer->endArray();
}

/*!
    Opens a dictionary with keys of the type \a keyMetaType, which must be a basic type, and
    values of the type \a valueMetaType. Enclose each pair in beginMapEntry() and endMapEntry().
*/
void QDBusArgument::beginMap(QMetaType keyMetaType, QMetaType valueMetaType) {
    if (auto *p = writing())
        p->writer->beginMap(QByteArray(QDBusMetaType::typeToSignature(keyMetaType)),
                            QByteArray(QDBusMetaType::typeToSignature(valueMetaType)));
}

/*!
    Closes the dictionary that beginMap() opened.
*/
void QDBusArgument::endMap() {
    if (auto *p = writing())
        p->writer->endMap();
}

/*!
    Opens an entry of a dictionary. The two values that follow are its key and its value.
*/
void QDBusArgument::beginMapEntry() {
    if (auto *p = writing())
        p->writer->beginMapEntry();
}

/*!
    Closes the entry that beginMapEntry() opened.
*/
void QDBusArgument::endMapEntry() {
    if (auto *p = writing())
        p->writer->endMapEntry();
}

/*!
    Appends \a value, which can have any type that is known to the D-Bus type system. It is not
    wrapped in a D-Bus variant.
*/
void QDBusArgument::appendVariant(const QVariant &value) {
    if (auto *p = writing())
        p->writer->appendVariant(value);
}

/*!
    Returns the signature of the value at the current position of an argument that is being
    read, or of everything written so far to one that is being written.
*/
QString QDBusArgument::currentSignature() const {
    if (d->writer)
        return QString::fromLatin1(d->writer->signature);
    return QString::fromLatin1(d->reader.currentSignature());
}

/*!
    Returns the kind of the value at the current position, or \c UnknownType if there is none or
    the argument is being written.
*/
QDBusArgument::ElementType QDBusArgument::currentType() const {
    return d->writer ? UnknownType : d->reader.currentType();
}

/*!
    Reads a byte into \a arg and returns a reference to this argument.
*/
const QDBusArgument &QDBusArgument::operator>>(uchar &arg) const {
    if (auto *p = reading())
        arg = p->reader.readByte();
    return *this;
}

/*!
    \overload
    Reads a boolean into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(bool &arg) const {
    if (auto *p = reading())
        arg = p->reader.readBool();
    return *this;
}

/*!
    \overload
    Reads a 16 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(short &arg) const {
    if (auto *p = reading())
        arg = p->reader.readInt16();
    return *this;
}

/*!
    \overload
    Reads an unsigned 16 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(ushort &arg) const {
    if (auto *p = reading())
        arg = p->reader.readUInt16();
    return *this;
}

/*!
    \overload
    Reads a 32 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(int &arg) const {
    if (auto *p = reading())
        arg = p->reader.readInt32();
    return *this;
}

/*!
    \overload
    Reads an unsigned 32 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(uint &arg) const {
    if (auto *p = reading())
        arg = p->reader.readUInt32();
    return *this;
}

/*!
    \overload
    Reads a 64 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(qlonglong &arg) const {
    if (auto *p = reading())
        arg = p->reader.readInt64();
    return *this;
}

/*!
    \overload
    Reads an unsigned 64 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(qulonglong &arg) const {
    if (auto *p = reading())
        arg = p->reader.readUInt64();
    return *this;
}

/*!
    \overload
    Reads a 64 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(long &arg) const {
    qlonglong value = 0;
    *this >> value;
    arg = static_cast<long>(value);
    return *this;
}

/*!
    \overload
    Reads an unsigned 64 bit integer into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(unsigned long &arg) const {
    qulonglong value = 0;
    *this >> value;
    arg = static_cast<unsigned long>(value);
    return *this;
}

/*!
    \overload
    Reads a floating-point number into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(double &arg) const {
    if (auto *p = reading())
        arg = p->reader.readDouble();
    return *this;
}

/*!
    \overload
    Reads a string into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QString &arg) const {
    if (auto *p = reading()) {
        const char c = p->reader.peek();
        arg = c == 'o' ? p->reader.readObjectPath() : c == 'g' ? p->reader.readSignature() : p->reader.readString();
    }
    return *this;
}

/*!
    \overload
    Reads a variant into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QDBusVariant &arg) const {
    if (auto *p = reading())
        arg = QDBusVariant(p->reader.readDBusVariant());
    return *this;
}

/*!
    \overload
    Reads an object path into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QDBusObjectPath &arg) const {
    if (auto *p = reading())
        arg = QDBusObjectPath(p->reader.readObjectPath());
    return *this;
}

/*!
    \overload
    Reads a signature into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QDBusSignature &arg) const {
    if (auto *p = reading())
        arg = QDBusSignature(p->reader.readSignature());
    return *this;
}

/*!
    \overload
    Reads a file descriptor into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QDBusUnixFileDescriptor &arg) const {
    if (auto *p = reading())
        arg = p->reader.readUnixFd();
    return *this;
}

/*!
    \overload
    Reads an array of strings into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QStringList &arg) const {
    if (auto *p = reading())
        arg = p->reader.readStringList();
    return *this;
}

/*!
    \overload
    Reads an array of bytes into \a arg.
*/
const QDBusArgument &QDBusArgument::operator>>(QByteArray &arg) const {
    if (auto *p = reading())
        arg = p->reader.readByteArray();
    return *this;
}

/*!
    \overload
    Enters the structure at the current position.
*/
void QDBusArgument::beginStructure() const {
    if (auto *p = reading())
        p->reader.beginStructure();
}

/*!
    \overload
    Leaves the current structure, skipping members that were not read.
*/
void QDBusArgument::endStructure() const {
    if (auto *p = reading())
        p->reader.endStructure();
}

/*!
    \overload
    Enters the array at the current position. Read elements while atEnd() is \c false.
*/
void QDBusArgument::beginArray() const {
    if (auto *p = reading())
        p->reader.beginArray();
}

/*!
    \overload
    Leaves the current array, skipping elements that were not read.
*/
void QDBusArgument::endArray() const {
    if (auto *p = reading())
        p->reader.endArray();
}

/*!
    \overload
    Enters the dictionary at the current position. Read entries while atEnd() is \c false.
*/
void QDBusArgument::beginMap() const {
    if (auto *p = reading())
        p->reader.beginMap();
}

/*!
    \overload
    Leaves the current dictionary, skipping entries that were not read.
*/
void QDBusArgument::endMap() const {
    if (auto *p = reading())
        p->reader.endMap();
}

/*!
    \overload
    Enters the dictionary entry at the current position.
*/
void QDBusArgument::beginMapEntry() const {
    if (auto *p = reading())
        p->reader.beginMapEntry();
}

/*!
    \overload
    Leaves the current dictionary entry.
*/
void QDBusArgument::endMapEntry() const {
    if (auto *p = reading())
        p->reader.endMapEntry();
}

/*!
    Returns \c true if there are no more values in the current container, or in the argument.
*/
bool QDBusArgument::atEnd() const { return d->writer || d->reader.atEnd(); }

/*!
    Reads the value at the current position and returns it in a QVariant: basic types, string
    lists and byte arrays as themselves, a variant as a QDBusVariant, and other containers as a
    QDBusArgument.
*/
QVariant QDBusArgument::asVariant() const {
    if (auto *p = reading())
        return p->reader.readValue();
    return QVariant();
}

// Writes value to arg as a D-Bus variant and returns arg.
QDBusArgument &operator<<(QDBusArgument &arg, const QVariant &value) { return arg << QDBusVariant(value); }

// Reads a D-Bus variant from arg into value and returns arg.
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariant &value) {
    QDBusVariant variant;
    arg >> variant;
    value = variant.variant();
    return arg;
}

// Writes list to arg as an array of variants and returns arg.
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantList &list) {
    arg.beginArray(QMetaType::fromType<QDBusVariant>());
    for (const QVariant &item : list)
        arg << QDBusVariant(item);
    arg.endArray();
    return arg;
}

// Reads an array of variants from arg into list and returns arg.
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantList &list) {
    list.clear();
    arg.beginArray();
    while (!arg.atEnd())
        list.append(arg.asVariant());
    arg.endArray();
    for (QVariant &item : list)
        if (item.metaType() == QMetaType::fromType<QDBusVariant>())
            item = item.value<QDBusVariant>().variant();
    return arg;
}

// Writes map to arg as a dictionary of strings to variants and returns arg.
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantMap &map) {
    arg.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QDBusVariant>());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << it.key() << QDBusVariant(it.value());
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Reads a dictionary of strings to variants from arg into map and returns arg.
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantMap &map) {
    map.clear();
    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QDBusVariant value;
        arg.beginMapEntry();
        arg >> key >> value;
        map.insert(key, value.variant());
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Writes map to arg as a dictionary of strings to variants and returns arg.
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantHash &map) {
    arg.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QDBusVariant>());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << it.key() << QDBusVariant(it.value());
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Reads a dictionary of strings to variants from arg into map and returns arg.
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantHash &map) {
    map.clear();
    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QDBusVariant value;
        arg.beginMapEntry();
        arg >> key >> value;
        map.insert(key, value.variant());
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}
