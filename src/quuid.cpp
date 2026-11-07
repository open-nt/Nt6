// MIT License - see LICENSE
#include <QtCore/qrandom.h>
#include <QtCore/quuid.h>

#include <cstring>

namespace {

int hexValue(QChar c) {
    const char16_t u = c.unicode();
    if (u >= u'0' && u <= u'9')
        return u - u'0';
    if (u >= u'a' && u <= u'f')
        return u - u'a' + 10;
    if (u >= u'A' && u <= u'F')
        return u - u'A' + 10;
    return -1;
}

QByteArray rfcBytes(const QUuid &u) {
    QByteArray b(16, '\0');
    for (int i = 0; i < 4; ++i)
        b[i] = static_cast<char>(u.data1 >> (24 - 8 * i));
    b[4] = static_cast<char>(u.data2 >> 8);
    b[5] = static_cast<char>(u.data2);
    b[6] = static_cast<char>(u.data3 >> 8);
    b[7] = static_cast<char>(u.data3);
    std::memcpy(b.data() + 8, u.data4, 8);
    return b;
}

QUuid fromNameHash(const QUuid &ns, const QByteArray &name, QCryptographicHash::Algorithm method, int version) {
    QCryptographicHash h(method);
    h.addData(rfcBytes(ns));
    h.addData(name);
    QByteArray bytes = h.result().left(16);
    bytes[6] = static_cast<char>((bytes[6] & 0x0f) | (version << 4));
    bytes[8] = static_cast<char>((bytes[8] & 0x3f) | 0x80);
    return QUuid::fromRfc4122(bytes);
}

} // namespace

/*!
    \class QUuid
    \brief The QUuid class stores a Universally Unique Identifier (UUID).

    A UUID is a 128-bit value, usually written as 32 hexadecimal digits in
    the form \c{{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}}. createUuid() returns a
    random (version 4) UUID, and createUuidV3() and createUuidV5() derive one
    from a namespace and a name.

    \code
    QUuid id = QUuid::createUuid();
    QString text = id.toString(QUuid::WithoutBraces);
    \endcode

    \differences
    There is no \c createUuidV7(), no \c QUuid(QAnyStringView) or
    \c fromString(QAnyStringView), and no QDataStream, \c QDebug or Windows
    \c GUID conversions. Strings are parsed from a QStringView, a
    \c{const char *} or a QByteArray. There are no predefined namespace
    constants.
    \enddifferences

    \sa QRandomGenerator, QCryptographicHash
*/

/*!
    \enum QUuid::Variant

    Describes the layout of a UUID, which is stored in the top bits of
    \c data4[0].

    \value VarUnknown The variant is unknown. A null UUID has this variant.
    \value NCS Reserved for NCS (Network Computing System) backward compatibility.
    \value DCE Distributed Computing Environment, the variant defined by RFC 4122.
    \value Microsoft Reserved for Microsoft Corporation backward compatibility.
    \value Reserved Reserved for future definition.

    \sa variant()
*/

/*!
    \enum QUuid::Version

    Describes how a UUID of the DCE variant was generated.

    \value VerUnknown The version is unknown, or the UUID is not of the DCE variant.
    \value Time Time-based.
    \value EmbeddedPOSIX DCE security with embedded POSIX UIDs.
    \value Md5 Name-based, hashed with MD5.
    \value Name The same as Md5.
    \value Random Random-number based.
    \value Sha1 Name-based, hashed with SHA-1.

    \sa version()
*/

/*!
    \enum QUuid::StringFormat

    Selects the text format used by toString() and toByteArray().

    \value WithBraces The canonical form with dashes, enclosed in braces: \c{{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}}.
    \value WithoutBraces The canonical form with dashes but without braces.
    \value Id128 The 32 hexadecimal digits with neither dashes nor braces.
*/

/*!
    Returns the UUID as 16 bytes in network (big-endian) order, as defined by
    RFC 4122.

    \sa fromRfc4122()
*/
QByteArray QUuid::toRfc4122() const { return rfcBytes(*this); }

/*!
    Returns the UUID as lowercase hexadecimal text, as a QByteArray, in the
    given \a mode.

    \sa toString(), fromString()
*/
QByteArray QUuid::toByteArray(StringFormat mode) const {
    QByteArray hex = rfcBytes(*this).toHex();
    if (mode == Id128)
        return hex;
    QByteArray dashed = hex.mid(0, 8) + '-' + hex.mid(8, 4) + '-' + hex.mid(12, 4) + '-' + hex.mid(16, 4) + '-' + hex.mid(20);
    return mode == WithBraces ? '{' + dashed + '}' : dashed;
}

/*!
    Returns the UUID as lowercase hexadecimal text in the given \a mode.

    \sa toByteArray(), fromString()
*/
QString QUuid::toString(StringFormat mode) const { return QString::fromUtf8(toByteArray(mode)); }

/*!
    Creates a UUID from the 16 bytes in \a bytes, in network (big-endian)
    order. Returns a null UUID if \a bytes does not hold exactly 16 bytes.

    \sa toRfc4122()
*/
QUuid QUuid::fromRfc4122(const QByteArray &b) {
    if (b.size() != 16)
        return QUuid();
    const auto *p = reinterpret_cast<const uchar *>(b.constData());
    QUuid u;
    u.data1 = quint32(p[0]) << 24 | quint32(p[1]) << 16 | quint32(p[2]) << 8 | p[3];
    u.data2 = static_cast<ushort>(p[4] << 8 | p[5]);
    u.data3 = static_cast<ushort>(p[6] << 8 | p[7]);
    std::memcpy(u.data4, p + 8, 8);
    return u;
}

/*!
    Parses \a text as a UUID. It must be 32 hexadecimal digits, or the
    canonical dashed form, optionally enclosed in braces. Digits may be
    upper or lower case. Returns a null UUID if \a text is not valid.

    \sa toString()
*/
QUuid QUuid::fromString(QStringView text) {
    if (text.size() == 38) {
        if (text[0] != QChar(u'{') || text[37] != QChar(u'}'))
            return QUuid();
        text = text.mid(1, 36);
    }
    QByteArray bytes;
    if (text.size() == 36) {
        for (qsizetype dash : {8, 13, 18, 23})
            if (text[dash] != QChar(u'-'))
                return QUuid();
    } else if (text.size() != 32) {
        return QUuid();
    }
    bytes.reserve(16);
    int high = -1;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.size() == 36 && (i == 8 || i == 13 || i == 18 || i == 23))
            continue;
        const int v = hexValue(text[i]);
        if (v < 0)
            return QUuid();
        if (high < 0) {
            high = v;
        } else {
            bytes.append(static_cast<char>(high << 4 | v));
            high = -1;
        }
    }
    return fromRfc4122(bytes);
}

/*!
    Returns \c true if all fields are zero; otherwise returns \c false.
*/
bool QUuid::isNull() const noexcept { return *this == QUuid(); }

/*!
    Returns the variant of the UUID, or VarUnknown if it is null.

    \sa version()
*/
QUuid::Variant QUuid::variant() const noexcept {
    if (isNull())
        return VarUnknown;
    const uchar top = data4[0];
    if ((top & 0x80) == 0)
        return NCS;
    if ((top & 0xc0) == 0x80)
        return DCE;
    if ((top & 0xe0) == 0xc0)
        return Microsoft;
    return Reserved;
}

/*!
    Returns the version of the UUID, or VerUnknown if it is not of the DCE
    variant or has an unrecognized version.

    \sa variant()
*/
QUuid::Version QUuid::version() const noexcept {
    const int v = data3 >> 12;
    if (variant() != DCE || v < Time || v > Sha1)
        return VerUnknown;
    return static_cast<Version>(v);
}

/*!
    Creates a random (version 4, DCE variant) UUID using
    QRandomGenerator::system().

    \sa createUuidV3(), createUuidV5()
*/
QUuid QUuid::createUuid() {
    QUuid u;
    quint32 words[4];
    QRandomGenerator::system()->fillRange(words, 4);
    u.data1 = words[0];
    u.data2 = static_cast<ushort>(words[1]);
    u.data3 = static_cast<ushort>((words[1] >> 16 & 0x0fff) | 0x4000);
    std::memcpy(u.data4, &words[2], 8);
    u.data4[0] = static_cast<uchar>((u.data4[0] & 0x3f) | 0x80);
    return u;
}

/*!
    Creates a name-based, version 3 UUID by hashing \a name within the
    namespace \a ns with MD5. The same inputs always give the same UUID.

    \sa createUuidV5(), createUuid()
*/
QUuid QUuid::createUuidV3(const QUuid &ns, const QByteArray &name) { return fromNameHash(ns, name, QCryptographicHash::Md5, 3); }

/*!
    Creates a name-based, version 5 UUID by hashing \a name within the
    namespace \a ns with SHA-1. The same inputs always give the same UUID.

    \sa createUuidV3(), createUuid()
*/
QUuid QUuid::createUuidV5(const QUuid &ns, const QByteArray &name) { return fromNameHash(ns, name, QCryptographicHash::Sha1, 5); }

bool operator==(const QUuid &a, const QUuid &b) noexcept {
    return a.data1 == b.data1 && a.data2 == b.data2 && a.data3 == b.data3 && std::memcmp(a.data4, b.data4, 8) == 0;
}

bool operator<(const QUuid &a, const QUuid &b) noexcept {
    if (a.data1 != b.data1)
        return a.data1 < b.data1;
    if (a.data2 != b.data2)
        return a.data2 < b.data2;
    if (a.data3 != b.data3)
        return a.data3 < b.data3;
    return std::memcmp(a.data4, b.data4, 8) < 0;
}

std::size_t qHash(const QUuid &uuid, std::size_t seed) noexcept { return qHash(rfcBytes(uuid), seed); }
