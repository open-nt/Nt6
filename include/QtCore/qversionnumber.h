// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <initializer_list>

class QVersionNumber {
public:
    QVersionNumber() = default;
    explicit QVersionNumber(const QList<int> &seg) : m(seg) {}
    QVersionNumber(std::initializer_list<int> args) : m(args) {}
    explicit QVersionNumber(int maj) { m.append(maj); }
    QVersionNumber(int maj, int min) { m.append(maj); m.append(min); }
    QVersionNumber(int maj, int min, int mic) { m.append(maj); m.append(min); m.append(mic); }

    bool isNull() const noexcept { return m.isEmpty(); }
    bool isNormalized() const noexcept { return isNull() || m.last() != 0; }
    int majorVersion() const noexcept { return segmentAt(0); }
    int minorVersion() const noexcept { return segmentAt(1); }
    int microVersion() const noexcept { return segmentAt(2); }
    QVersionNumber normalized() const;
    QList<int> segments() const { return m; }
    int segmentAt(qsizetype i) const noexcept { return i < m.size() ? m.at(i) : 0; }
    qsizetype segmentCount() const noexcept { return m.size(); }
    bool isPrefixOf(const QVersionNumber &other) const noexcept;
    QString toString() const;

    static int compare(const QVersionNumber &a, const QVersionNumber &b) noexcept;
    static QVersionNumber commonPrefix(const QVersionNumber &a, const QVersionNumber &b);
    static QVersionNumber fromString(QStringView string, qsizetype *suffixIndex = nullptr);

    friend bool operator==(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) == 0; }
    friend bool operator!=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) != 0; }
    friend bool operator<(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) < 0; }
    friend bool operator<=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) <= 0; }
    friend bool operator>(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) > 0; }
    friend bool operator>=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) >= 0; }

private:
    QList<int> m;
};
