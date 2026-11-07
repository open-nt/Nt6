// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstddef>
#include <type_traits>

/*!
    \class QTypeRevision
    \brief The QTypeRevision class contains a QML type revision.

    A revision is a major and a minor version number, each of which can be unspecified. It is
    used to tag the members of a type with the version in which they appeared. Segments are in
    the range 0 to 254; the value 255 stands for an unspecified segment.

    \code
    QTypeRevision r = QTypeRevision::fromVersion(1, 5);
    r.majorVersion();                       // 1
    r.toEncodedVersion<quint16>();          // 0x0105
    QTypeRevision::fromMinorVersion(3).hasMajorVersion();  // false
    \endcode

    \differences
    None.
    \enddifferences

    \sa QMetaMethod::revision(), QMetaProperty::revision()
*/
class QTypeRevision {
public:
    /*!
        The type that the segments of a revision are stored in.
    */
    using SegmentType = quint8;

    /*!
        Constructs an invalid revision, with neither a major nor a minor version.

        \sa isValid()
    */
    constexpr QTypeRevision() = default;

    /*!
        Returns the revision with the segments \a majorVersion and \a minorVersion.
    */
    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    static constexpr QTypeRevision fromVersion(Integer majorVersion, Integer minorVersion) {
        return QTypeRevision(static_cast<SegmentType>(majorVersion), static_cast<SegmentType>(minorVersion));
    }
    /*!
        Returns the revision with the major version \a majorVersion and no minor version.
    */
    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    static constexpr QTypeRevision fromMajorVersion(Integer majorVersion) {
        return QTypeRevision(static_cast<SegmentType>(majorVersion), kUnspecified);
    }
    /*!
        Returns the revision with the minor version \a minorVersion and no major version.
    */
    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    static constexpr QTypeRevision fromMinorVersion(Integer minorVersion) {
        return QTypeRevision(kUnspecified, static_cast<SegmentType>(minorVersion));
    }
    /*!
        Returns the revision that \a value encodes, with the major version in the high byte and
        the minor version in the low byte. A byte of 255 is an unspecified segment.

        \sa toEncodedVersion()
    */
    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    static constexpr QTypeRevision fromEncodedVersion(Integer value) {
        return QTypeRevision(static_cast<SegmentType>((static_cast<unsigned>(value) >> 8) & 0xffu),
                             static_cast<SegmentType>(static_cast<unsigned>(value) & 0xffu));
    }
    /*!
        Returns the revision 0.0.
    */
    static constexpr QTypeRevision zero() { return QTypeRevision(0, 0); }

    /*!
        Returns \c true if the revision has a major version.

        \sa majorVersion()
    */
    constexpr bool hasMajorVersion() const { return m_major != kUnspecified; }
    /*!
        Returns the major version, or 0 if it is unspecified.

        \sa hasMajorVersion()
    */
    constexpr SegmentType majorVersion() const { return m_major == kUnspecified ? SegmentType(0) : m_major; }
    /*!
        Returns \c true if the revision has a minor version.

        \sa minorVersion()
    */
    constexpr bool hasMinorVersion() const { return m_minor != kUnspecified; }
    /*!
        Returns the minor version, or 0 if it is unspecified.

        \sa hasMinorVersion()
    */
    constexpr SegmentType minorVersion() const { return m_minor == kUnspecified ? SegmentType(0) : m_minor; }
    /*!
        Returns \c true if the revision has a major or a minor version.
    */
    constexpr bool isValid() const { return hasMajorVersion() || hasMinorVersion(); }

    /*!
        Returns the revision encoded in an integer, with the major version in the high byte and
        the minor version in the low byte, and 255 for an unspecified segment.

        \sa fromEncodedVersion()
    */
    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    constexpr Integer toEncodedVersion() const {
        return static_cast<Integer>((static_cast<unsigned>(m_major) << 8) | m_minor);
    }

    /*!
        Returns \c true if \a a and \a b are the same revision.
    */
    friend constexpr bool operator==(QTypeRevision a, QTypeRevision b) {
        return a.m_major == b.m_major && a.m_minor == b.m_minor;
    }
    /*!
        Returns \c true if \a a and \a b are different revisions.
    */
    friend constexpr bool operator!=(QTypeRevision a, QTypeRevision b) { return !(a == b); }
    /*!
        Returns \c true if \a a is before \a b, comparing the major version first. An unspecified
        segment is after every specified one.
    */
    friend constexpr bool operator<(QTypeRevision a, QTypeRevision b) {
        return a.m_major != b.m_major ? a.m_major < b.m_major : a.m_minor < b.m_minor;
    }
    /*!
        Returns \c true if \a a is after \a b.
    */
    friend constexpr bool operator>(QTypeRevision a, QTypeRevision b) { return b < a; }
    /*!
        Returns \c true if \a a is before or the same as \a b.
    */
    friend constexpr bool operator<=(QTypeRevision a, QTypeRevision b) { return !(b < a); }
    /*!
        Returns \c true if \a a is after or the same as \a b.
    */
    friend constexpr bool operator>=(QTypeRevision a, QTypeRevision b) { return !(a < b); }

private:
    static constexpr SegmentType kUnspecified = 255;

    constexpr QTypeRevision(SegmentType major, SegmentType minor) : m_major(major), m_minor(minor) {}

    SegmentType m_major = kUnspecified;
    SegmentType m_minor = kUnspecified;
};

// Returns a hash of revision, combined with seed.
constexpr std::size_t qHash(QTypeRevision revision, std::size_t seed = 0) noexcept {
    return seed ^ (revision.toEncodedVersion<std::size_t>() + 0x9e3779b9u + (seed << 6) + (seed >> 2));
}
