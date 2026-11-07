// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbusargument.h>
#include <QtDBus/qdbusunixfiledescriptor.h>

#include <memory>
#include <vector>

// The D-Bus type system and wire format. Alignment is relative to the start of the buffer, and a
// message body starts on an 8 byte boundary, so a body can be built in a buffer of its own.
namespace QDBusUtil {

constexpr int kMaxSignature = 255;
constexpr int kMaxArrayBytes = 64 * 1024 * 1024;
constexpr int kMaxMessageBytes = 128 * 1024 * 1024;
constexpr int kMaxStructDepth = 32;
constexpr int kMaxArrayDepth = 32;
constexpr int kMaxTotalDepth = 64;
constexpr int kMaxUnixFds = 253; // what a single sendmsg() can carry on Linux

bool isValidObjectPath(const QString &path);
bool isValidInterfaceName(const QString &name);
bool isValidMemberName(const QString &name);
bool isValidErrorName(const QString &name);
bool isValidUniqueConnectionName(const QString &name);
bool isValidBusName(const QString &name);
bool isValidSignature(const QByteArray &signature);
bool isValidSingleSignature(const QByteArray &signature);

// The offset just after the complete type at `pos`, or -1 if there is none.
int skipType(const QByteArray &signature, int pos);
int alignmentOf(char typeCode);

// Checks the values of `signature` in data[pos...], which were written with the given byte order.
bool validateValues(const QByteArray &data, int pos, const QByteArray &signature, bool swapped, int unixFds);

} // namespace QDBusUtil

class QDBusWriter {
public:
    QByteArray data;
    QByteArray signature;
    QList<QDBusUnixFileDescriptor> fds;
    bool failed = false;
    QString errorText;

    void fail(const QString &why);

    void align(int n);
    void writeByte(uchar v);
    void writeBool(bool v);
    void writeInt16(short v);
    void writeUInt16(ushort v);
    void writeInt32(int v);
    void writeUInt32(uint v);
    void writeInt64(qlonglong v);
    void writeUInt64(qulonglong v);
    void writeDouble(double v);
    void writeString(const QString &v);
    void writeObjectPath(const QString &v);
    void writeSignature(const QByteArray &v);
    void writeUnixFd(const QDBusUnixFileDescriptor &v);
    void writeByteArray(const QByteArray &v);
    void writeDBusVariant(const QVariant &inner);

    void beginStructure();
    void endStructure();
    void beginArray(const QByteArray &elementSignature);
    void endArray();
    void beginMap(const QByteArray &keySignature, const QByteArray &valueSignature);
    void endMap();
    void beginMapEntry();
    void endMapEntry();

    // Writes a value of any type that the D-Bus type system knows.
    void appendVariant(const QVariant &value);
    // The signature of `value`, or an empty array if it has none.
    static QByteArray signatureOf(const QVariant &value);

private:
    template <typename T> void put(T value, char code);
    void note(char code);
    void note(const QByteArray &code);
    void appendArgument(const QDBusArgument &argument);

    struct ArrayFrame {
        int lengthAt;
        int start;
    };
    std::vector<ArrayFrame> m_arrays;
    int m_skipSignature = 0;
};

class QDBusReader {
public:
    QDBusReader() = default;
    QDBusReader(const QByteArray &data, int pos, const QByteArray &signature, bool swapped,
                const QList<QDBusUnixFileDescriptor> &fds);

    // The next value's type code, or 0 at the end of the current container.
    char peek() const;
    bool atEnd() const;
    QByteArray currentSignature() const;
    QDBusArgument::ElementType currentType() const;

    uchar readByte();
    bool readBool();
    short readInt16();
    ushort readUInt16();
    int readInt32();
    uint readUInt32();
    qlonglong readInt64();
    qulonglong readUInt64();
    double readDouble();
    QString readString();
    QString readObjectPath();
    QString readSignature();
    QDBusUnixFileDescriptor readUnixFd();
    QVariant readDBusVariant();
    QStringList readStringList();
    QByteArray readByteArray();

    void beginStructure();
    void endStructure();
    void beginArray();
    void endArray();
    void beginMap();
    void endMap();
    void beginMapEntry();
    void endMapEntry();

    // Reads the next value: a basic type as itself, anything else as a QDBusArgument.
    QVariant readValue();
    // Skips the next value.
    void skipValue();
    // Writes the next value to `writer`.
    void copyValue(QDBusWriter &writer);

private:
    struct Frame {
        char kind; // 'a' array, 'e' map, '(' structure, '{' map entry
        int end;
        int elementSignature;
        int afterSignature;
    };

    bool expect(char code);
    bool alignTo(int n);
    bool need(int n) const;
    template <typename T> T get(char code);
    QByteArray rawString(int lengthBytes);
    void close(char kind);
    void corrupt();
    QDBusReader sliceOfNextValue() const;

    QByteArray m_data;
    int m_pos = 0;
    QByteArray m_signature;
    int m_sigPos = 0;
    bool m_swapped = false;
    bool m_failed = false;
    QList<QDBusUnixFileDescriptor> m_fds;
    std::vector<Frame> m_frames;
};

class QDBusArgumentPrivate {
public:
    // Writes through `writer`, which must outlive the argument, or to a buffer of its own.
    explicit QDBusArgumentPrivate(QDBusWriter *writer);
    explicit QDBusArgumentPrivate(QDBusReader reader);

    static QDBusArgument forWriter(QDBusWriter *writer);
    static QDBusArgument forReader(QDBusReader reader);
    // Everything written to or left to read from `argument`, as a reader.
    static QDBusReader readerOf(const QDBusArgument &argument);

    std::shared_ptr<QDBusWriter> writer; // null when reading
    QDBusReader reader;
};
