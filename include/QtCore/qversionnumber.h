// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>

class QVersionNumber {
public:
    /*!
        \since 6.6
        A read-only iterator over the segments of the version number.
    */
    using const_iterator = QList<int>::const_iterator;
    /*!
        \since 6.6
        A read-only iterator over the segments in reverse order.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /*!
        \since 6.6
        The type of a segment, \c int.
    */
    using value_type = int;
    /*!
        \since 6.6
        The type of the distance between two iterators.
    */
    using difference_type = qsizetype;
    /*!
        \since 6.6
        The type of a segment count or index.
    */
    using size_type = qsizetype;

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
    static QVersionNumber fromString(QAnyStringView string, qsizetype *suffixIndex = nullptr);

    /*!
        \since 6.6
        Returns an iterator to the first segment.

        \sa end(), segments()
    */
    const_iterator begin() const noexcept { return m.constBegin(); }
    /*!
        \since 6.6
        Returns an iterator to the position after the last segment.

        \sa begin()
    */
    const_iterator end() const noexcept { return m.constEnd(); }
    /*!
        \since 6.6
        Returns an iterator to the first segment.
    */
    const_iterator cbegin() const noexcept { return m.constBegin(); }
    /*!
        \since 6.6
        Returns an iterator to the position after the last segment.
    */
    const_iterator cend() const noexcept { return m.constEnd(); }
    /*!
        \since 6.6
        Returns an iterator to the first segment.
    */
    const_iterator constBegin() const noexcept { return m.constBegin(); }
    /*!
        \since 6.6
        Returns an iterator to the position after the last segment.
    */
    const_iterator constEnd() const noexcept { return m.constEnd(); }
    /*!
        \since 6.6
        Returns a reverse iterator to the last segment.

        \sa rend()
    */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /*!
        \since 6.6
        Returns a reverse iterator to the position before the first segment.

        \sa rbegin()
    */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /*!
        \since 6.6
        Returns a reverse iterator to the last segment.
    */
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    /*!
        \since 6.6
        Returns a reverse iterator to the position before the first segment.
    */
    const_reverse_iterator crend() const noexcept { return rend(); }

    friend bool operator==(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) == 0; }
    friend bool operator!=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) != 0; }
    friend bool operator<(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) < 0; }
    friend bool operator<=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) <= 0; }
    friend bool operator>(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) > 0; }
    friend bool operator>=(const QVersionNumber &a, const QVersionNumber &b) noexcept { return compare(a, b) >= 0; }

private:
    QList<int> m;
};

// Returns a hash of the segments of key, mixed with seed.
inline std::size_t qHash(const QVersionNumber &key, std::size_t seed = 0) noexcept {
    return qHashRange(key.begin(), key.end(), seed);
}
