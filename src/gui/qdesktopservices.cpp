// MIT License - see LICENSE
#include <QtGui/qdesktopservices.h>

#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>

namespace {

struct Handler {
    QPointer<QObject> receiver;
    QByteArray method;
};

QMutex &handlerMutex() {
    static QMutex mutex;
    return mutex;
}

QHash<QString, Handler> &handlers() {
    static QHash<QString, Handler> map;
    return map;
}

// Hands `url` to the program of the desktop that opens URLs.
bool openWithDesktop(const QUrl &url) {
    const QString target = url.isLocalFile() ? url.toLocalFile() : QString::fromLatin1(url.toEncoded());
#if defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {target});
#elif defined(Q_OS_WIN)
    return QProcess::startDetached(QStringLiteral("rundll32"), {QStringLiteral("url.dll,FileProtocolHandler"), target});
#else
    return QProcess::startDetached(QStringLiteral("xdg-open"), {target});
#endif
}

} // namespace

/*!
    \class QDesktopServices
    \inmodule NtGui
    \since 6.0
    \brief The QDesktopServices class opens URLs with the programs of the desktop.

    openUrl() shows a web page in the browser, opens a file with the program that the user chose for its type, or
    starts a mail to a \c mailto: address:

    \code
    QDesktopServices::openUrl(QUrl::fromLocalFile(exportDir.absolutePath()));
    \endcode

    An application can open the URLs of a scheme itself with setUrlHandler().

    \differences
    On Linux the URL goes to \c xdg-open, on macOS to \c open, and on Windows to the shell; openUrl() returns
    \c true when that program was started, not when it opened the URL. There is no \c storageLocation().
    \enddifferences
*/

/*!
    Opens \a url with the handler of its scheme if setUrlHandler() set one, or else with the program of the desktop.
    Returns \c true if the URL was handed on, \c false if it is not valid or nothing could be started.
*/
bool QDesktopServices::openUrl(const QUrl &url) {
    if (!url.isValid())
        return false;
    QPointer<QObject> receiver;
    QByteArray method;
    {
        QMutexLocker lock(&handlerMutex());
        const auto it = handlers().constFind(url.scheme());
        if (it != handlers().constEnd()) {
            receiver = it->receiver;
            method = it->method;
        }
    }
    if (receiver)
        return QMetaObject::invokeMethod(receiver.data(), method.constData(), Qt::DirectConnection, Q_ARG(QUrl, url));
    if (url.scheme().isEmpty())
        return openWithDesktop(QUrl::fromLocalFile(url.toString()));
    return openWithDesktop(url);
}

/*!
    Makes openUrl() call the slot or invokable \a method of \a receiver, with the URL as a QUrl argument, for each URL
    of the scheme \a scheme. \a method is the name of the function, without its arguments. The handler is removed
    when \a receiver is destroyed.

    \code
    QDesktopServices::setUrlHandler("help", helpWindow, "showTopic");
    \endcode

    \sa unsetUrlHandler()
*/
void QDesktopServices::setUrlHandler(const QString &scheme, QObject *receiver, const char *method) {
    if (!receiver) {
        unsetUrlHandler(scheme);
        return;
    }
    QMutexLocker lock(&handlerMutex());
    handlers().insert(scheme, Handler{receiver, QByteArray(method)});
}

/*!
    Removes the handler of the scheme \a scheme, so that openUrl() hands its URLs to the desktop again.

    \sa setUrlHandler()
*/
void QDesktopServices::unsetUrlHandler(const QString &scheme) {
    QMutexLocker lock(&handlerMutex());
    handlers().remove(scheme);
}
