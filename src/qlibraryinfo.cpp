// MIT License - see LICENSE
#include <QtCore/qlibraryinfo.h>

/*!
    \class QLibraryInfo
    \brief The QLibraryInfo class reports how the Nt library was built.

    version() returns the version of the library the program is running
    against, which comes from the \c VERSION file at the root of the Nt
    repository:

    \code
    if (QLibraryInfo::version() < QVersionNumber(6, 1))
        qWarning() << "This program needs Nt 6.1 or newer";
    \endcode

    \differences
    There is no \c path() or \c platformPluginArguments(), as Nt has no
    installed plugin or translation directories yet.
    \enddifferences

    \sa QVersionNumber
*/

/*!
    Returns the version of the Nt library. It is compiled into the library, so
    a program built against other headers still sees the version it runs with.

    \sa build()
*/
QVersionNumber QLibraryInfo::version() noexcept {
    return QVersionNumber(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
}

/*!
    Returns a line of text describing the build, including the version.

    \sa version()
*/
const char *QLibraryInfo::build() noexcept {
    return "Nt " QT_VERSION_STR " ("
#if defined(__clang__)
           "clang"
#elif defined(__GNUC__)
           "gcc"
#else
           "unknown compiler"
#endif
#ifdef NDEBUG
           ", release build"
#else
           ", debug build"
#endif
           ")";
}

/*!
    Returns \c true if the library was built with debugging enabled.
*/
bool QLibraryInfo::isDebugBuild() noexcept {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

/*!
    Returns \c true if Nt is a shared library rather than a static one.
*/
bool QLibraryInfo::isSharedBuild() noexcept {
#ifdef QT_SHARED
    return true;
#else
    return false;
#endif
}

// The runtime version, unlike QT_VERSION_STR, which is fixed at compile time.
const char *qVersion() noexcept { return QT_VERSION_STR; }
