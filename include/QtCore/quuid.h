// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

class QUuid {
public:
    enum Variant { VarUnknown = -1, NCS = 0, DCE = 2, Microsoft = 6, Reserved = 7 };
    enum Version {
        VerUnknown = -1,
        Time = 1,
        EmbeddedPOSIX = 2,
        Md5 = 3,
        Name = Md5,
        Random = 4,
        Sha1 = 5,
        UnixEpoch = 7
    };
    enum StringFormat { WithBraces = 0, WithoutBraces = 1, Id128 = 3 };

    /*!
        Constructs the null UUID, with all fields zero.

        \sa isNull()
    */
    constexpr QUuid() noexcept = default;
    /*!
        Constructs a UUID from the fields \a l, \a w1 and \a w2, and the eight
        bytes \a b1, \a b2, \a b3, \a b4, \a b5, \a b6, \a b7 and \a b8.
    */
    constexpr QUuid(uint l, ushort w1, ushort w2, uchar b1, uchar b2, uchar b3, uchar b4, uchar b5, uchar b6, uchar b7, uchar b8) noexcept
        : data1(l), data2(w1), data3(w2), data4{b1, b2, b3, b4, b5, b6, b7, b8} {}
    /*!
        Constructs a UUID by parsing \a text, as fromString() does.
    */
    explicit QUuid(QStringView text) : QUuid(fromString(text)) {}
    /*!
        \overload

        Constructs a UUID by parsing the UTF-8 string \a text.
    */
    explicit QUuid(const char *text) : QUuid(fromString(text)) {}
    /*!
        \overload

        Constructs a UUID by parsing the UTF-8 string \a text.
    */
    explicit QUuid(const QByteArray &text) : QUuid(fromString(text)) {}

    QString toString(StringFormat mode = WithBraces) const;
    QByteArray toByteArray(StringFormat mode = WithBraces) const;
    QByteArray toRfc4122() const;
    bool isNull() const noexcept;
    Variant variant() const noexcept;
    Version version() const noexcept;

    static QUuid fromString(QStringView text);
    /*!
        \overload

        Parses the UTF-8 string \a text. Returns a null UUID if \a text is
        \c nullptr or not valid.
    */
    static QUuid fromString(const char *text) { return text ? fromString(QStringView(QString::fromUtf8(text))) : QUuid(); }
    /*!
        \overload

        Parses the UTF-8 string \a text. Returns a null UUID if \a text is not
        valid.
    */
    static QUuid fromString(const QByteArray &text) { return fromString(QStringView(QString::fromUtf8(text))); }
    static QUuid fromRfc4122(const QByteArray &bytes);
    static QUuid createUuid();
    static QUuid createUuidV3(const QUuid &ns, const QByteArray &name);
    static QUuid createUuidV5(const QUuid &ns, const QByteArray &name);
    static QUuid createUuidV7();
    /*!
        \overload

        Creates a version 3 UUID from \a name, converted to UTF-8, within the
        namespace \a ns.
    */
    static QUuid createUuidV3(const QUuid &ns, const QString &name) { return createUuidV3(ns, name.toUtf8()); }
    /*!
        \overload

        Creates a version 5 UUID from \a name, converted to UTF-8, within the
        namespace \a ns.
    */
    static QUuid createUuidV5(const QUuid &ns, const QString &name) { return createUuidV5(ns, name.toUtf8()); }

    friend bool operator==(const QUuid &a, const QUuid &b) noexcept;
    friend bool operator!=(const QUuid &a, const QUuid &b) noexcept { return !(a == b); }
    friend bool operator<(const QUuid &a, const QUuid &b) noexcept;
    friend bool operator>(const QUuid &a, const QUuid &b) noexcept { return b < a; }
    friend bool operator<=(const QUuid &a, const QUuid &b) noexcept { return !(b < a); }
    friend bool operator>=(const QUuid &a, const QUuid &b) noexcept { return !(a < b); }

    /*!
        The first 32 bits of the UUID.
    */
    uint data1 = 0;
    /*!
        The next 16 bits of the UUID.
    */
    ushort data2 = 0;
    /*!
        The next 16 bits of the UUID, which hold the version in the top four
        bits.
    */
    ushort data3 = 0;
    /*!
        The last 64 bits of the UUID. The top bits of \c data4[0] hold the
        variant.
    */
    uchar data4[8] = {};
};

std::size_t qHash(const QUuid &uuid, std::size_t seed = 0) noexcept;
