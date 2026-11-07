// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

class QUuid {
public:
    enum Variant { VarUnknown = -1, NCS = 0, DCE = 2, Microsoft = 6, Reserved = 7 };
    enum Version { VerUnknown = -1, Time = 1, EmbeddedPOSIX = 2, Md5 = 3, Name = Md5, Random = 4, Sha1 = 5 };
    enum StringFormat { WithBraces = 0, WithoutBraces = 1, Id128 = 3 };

    constexpr QUuid() noexcept = default;
    constexpr QUuid(uint l, ushort w1, ushort w2, uchar b1, uchar b2, uchar b3, uchar b4, uchar b5, uchar b6, uchar b7, uchar b8) noexcept
        : data1(l), data2(w1), data3(w2), data4{b1, b2, b3, b4, b5, b6, b7, b8} {}
    explicit QUuid(QStringView text) : QUuid(fromString(text)) {}

    QString toString(StringFormat mode = WithBraces) const;
    QByteArray toByteArray(StringFormat mode = WithBraces) const;
    QByteArray toRfc4122() const;
    bool isNull() const noexcept;
    Variant variant() const noexcept;
    Version version() const noexcept;

    static QUuid fromString(QStringView text);
    static QUuid fromRfc4122(const QByteArray &bytes);
    static QUuid createUuid();
    static QUuid createUuidV3(const QUuid &ns, const QByteArray &name);
    static QUuid createUuidV5(const QUuid &ns, const QByteArray &name);
    static QUuid createUuidV3(const QUuid &ns, const QString &name) { return createUuidV3(ns, name.toUtf8()); }
    static QUuid createUuidV5(const QUuid &ns, const QString &name) { return createUuidV5(ns, name.toUtf8()); }

    friend bool operator==(const QUuid &a, const QUuid &b) noexcept;
    friend bool operator!=(const QUuid &a, const QUuid &b) noexcept { return !(a == b); }
    friend bool operator<(const QUuid &a, const QUuid &b) noexcept;
    friend bool operator>(const QUuid &a, const QUuid &b) noexcept { return b < a; }

    uint data1 = 0;
    ushort data2 = 0;
    ushort data3 = 0;
    uchar data4[8] = {};
};

std::size_t qHash(const QUuid &uuid, std::size_t seed = 0) noexcept;
