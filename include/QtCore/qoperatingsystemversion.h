// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qversionnumber.h>

#include <initializer_list>

class QOperatingSystemVersion {
public:
    /*!
        \enum QOperatingSystemVersion::OSType

        The operating system families that Qt knows by name.

        \value Unknown An operating system that has no value of its own; Linux and the other Unix systems are reported this way.
        \value Windows Microsoft Windows.
        \value MacOS Apple macOS.
        \value IOS Apple iOS.
        \value TvOS Apple tvOS.
        \value WatchOS Apple watchOS.
        \value Android Google Android.
        \value VisionOS Apple visionOS.
    */
    enum OSType { Unknown, Windows, MacOS, IOS, TvOS, WatchOS, Android, VisionOS };

    /*!
        Constructs a version of the operating system \a osType with the major
        version \a vmajor, the minor version \a vminor and the micro version
        \a vmicro. Pass \c -1 for the numbers that do not exist.
    */
    QOperatingSystemVersion(OSType osType, int vmajor, int vminor = -1, int vmicro = -1)
        : m_os(osType), m_major(vmajor), m_minor(vminor), m_micro(vmicro) {}

    static QOperatingSystemVersion current();
    /*!
        Returns the type of the operating system the program is running on,
        which is always Unknown on Linux.

        \sa current()
    */
    static OSType currentType() { return Unknown; }

    /*!
        Returns the major version number, or \c -1 if there is none.
    */
    constexpr int majorVersion() const noexcept { return m_major; }
    /*!
        Returns the minor version number, or \c -1 if there is none.
    */
    constexpr int minorVersion() const noexcept { return m_minor; }
    /*!
        Returns the micro version number, or \c -1 if there is none.
    */
    constexpr int microVersion() const noexcept { return m_micro; }
    /*!
        Returns the number of version numbers that are set: 0 to 3.
    */
    constexpr int segmentCount() const noexcept { return m_micro >= 0 ? 3 : m_minor >= 0 ? 2 : m_major >= 0 ? 1 : 0; }
    QVersionNumber version() const;
    /*!
        Returns the operating system family.
    */
    constexpr OSType type() const noexcept { return m_os; }
    QString name() const;
    bool isAnyOfType(std::initializer_list<OSType> types) const;

    /*!
        Returns \c true if \a lhs and \a rhs are the same operating system and
        \a lhs is an earlier version than \a rhs.
    */
    friend bool operator<(const QOperatingSystemVersion &lhs, const QOperatingSystemVersion &rhs) {
        return lhs.m_os == rhs.m_os && lhs.version() < rhs.version();
    }
    /*!
        Returns \c true if \a lhs and \a rhs are the same operating system and
        \a lhs is an earlier or the same version as \a rhs.
    */
    friend bool operator<=(const QOperatingSystemVersion &lhs, const QOperatingSystemVersion &rhs) {
        return lhs.m_os == rhs.m_os && lhs.version() <= rhs.version();
    }
    /*!
        Returns \c true if \a lhs and \a rhs are the same operating system and
        \a lhs is a later version than \a rhs.
    */
    friend bool operator>(const QOperatingSystemVersion &lhs, const QOperatingSystemVersion &rhs) {
        return lhs.m_os == rhs.m_os && lhs.version() > rhs.version();
    }
    /*!
        Returns \c true if \a lhs and \a rhs are the same operating system and
        \a lhs is a later or the same version as \a rhs.
    */
    friend bool operator>=(const QOperatingSystemVersion &lhs, const QOperatingSystemVersion &rhs) {
        return lhs.m_os == rhs.m_os && lhs.version() >= rhs.version();
    }

private:
    OSType m_os;
    int m_major;
    int m_minor;
    int m_micro;
    QString m_name;
};
