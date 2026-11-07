// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <initializer_list>

class QVersionNumber {
public:
    /*!
        Constructs a null version number with no segments.

        \sa isNull()
    */
    QVersionNumber() = default;
    /*!
        Constructs a version number from the segments in \a seg.
    */
    explicit QVersionNumber(const QList<int> &seg) : m(seg) {}
    /*!
        Constructs a version number from the segments in \a args.
    */
    QVersionNumber(std::initializer_list<int> args) : m(args) {}
    /*!
        Constructs a version number with the single segment \a maj.
    */
    explicit QVersionNumber(int maj) { m.append(maj); }
    /*!
        Constructs a version number with the segments \a maj and \a min.
    */
    QVersionNumber(int maj, int min) { m.append(maj); m.append(min); }
    /*!
        Constructs a version number with the segments \a maj, \a min and
        \a mic.
    */
    QVersionNumber(int maj, int min, int mic) { m.append(maj); m.append(min); m.append(mic); }

    /*!
        Returns \c true if the version number has no segments; otherwise
        returns \c false.
    */
    bool isNull() const noexcept { return m.isEmpty(); }
    /*!
        Returns \c true if the version number has no trailing zero segment;
        otherwise returns \c false. A null version number is normalized.

        \sa normalized()
    */
    bool isNormalized() const noexcept { return isNull() || m.last() != 0; }
    /*!
        Returns the first segment, or 0 if there is none.

        \sa minorVersion(), microVersion()
    */
    int majorVersion() const noexcept { return segmentAt(0); }
    /*!
        Returns the second segment, or 0 if there is none.

        \sa majorVersion(), microVersion()
    */
    int minorVersion() const noexcept { return segmentAt(1); }
    /*!
        Returns the third segment, or 0 if there is none.

        \sa majorVersion(), minorVersion()
    */
    int microVersion() const noexcept { return segmentAt(2); }
    QVersionNumber normalized() const;
    /*!
        Returns the segments of the version number.

        \sa segmentAt(), segmentCount()
    */
    QList<int> segments() const { return m; }
    /*!
        Returns the segment at index \a i, or 0 if \a i is past the last
        segment.

        \sa segmentCount()
    */
    int segmentAt(qsizetype i) const noexcept { return i < m.size() ? m.at(i) : 0; }
    /*!
        Returns the number of segments.

        \sa segments()
    */
    qsizetype segmentCount() const noexcept { return m.size(); }
    bool isPrefixOf(const QVersionNumber &other) const noexcept;
    QString toString() const;

    static int compare(const QVersionNumber &a, const QVersionNumber &b) noexcept;
    static QVersionNumber commonPrefix(const QVersionNumber &a, const QVersionNumber &b);
    static QVersionNumber fromString(QStringView string, qsizetype *suffixIndex = nullptr);
    /*!
        \overload

        Parses \a string, which is interpreted as UTF-8.
    */
    static QVersionNumber fromString(const char *string, qsizetype *suffixIndex = nullptr) {
        return fromString(QStringView(QString::fromUtf8(string)), suffixIndex);
    }

    friend bool operator==(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) == 0; }
    friend bool operator!=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) != 0; }
    friend bool operator<(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) < 0; }
    friend bool operator<=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) <= 0; }
    friend bool operator>(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) > 0; }
    friend bool operator>=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) >= 0; }

private:
    QList<int> m;
};
