// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

class QTranslator;

class QCoreApplication : public QObject {
    Q_OBJECT
public:
    QCoreApplication(int &argc, char **argv);
    ~QCoreApplication() override;

    static QCoreApplication *instance();

    // The command line as passed to the constructor, decoded as UTF-8.
    static QStringList arguments();
    static QString applicationFilePath();
    static QString applicationDirPath();
    static qint64 applicationPid();
    static QStringList libraryPaths();
    static void setLibraryPaths(const QStringList &paths);
    static void addLibraryPath(const QString &path);
    static void removeLibraryPath(const QString &path);
    // Defaults to the file name of argv[0].
    static void setApplicationName(const QString &name);
    static QString applicationName();
    static void setOrganizationName(const QString &orgName);
    static QString organizationName();
    static void setOrganizationDomain(const QString &orgDomain);
    static QString organizationDomain();
    static void setApplicationVersion(const QString &version);
    static QString applicationVersion();

    static int exec();
    static void exit(int returnCode = 0);
    static void setQuitLockEnabled(bool enabled);
    static bool isQuitLockEnabled();
    static void processEvents(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents);
    static void processEvents(QEventLoop::ProcessEventsFlags flags, int ms);
    static void processEvents(QEventLoop::ProcessEventsFlags flags, QDeadlineTimer deadline);
    static void sendPostedEvents(QObject *receiver = nullptr, int eventType = 0);

    // Takes ownership of `event`.
    static void postEvent(QObject *receiver, QEvent *event);
    static bool sendEvent(QObject *receiver, QEvent *event);
    // eventType 0 removes every pending event for `receiver`.
    static void removePostedEvents(QObject *receiver, int eventType = 0);

    virtual bool notify(QObject *receiver, QEvent *event);
    static bool installTranslator(QTranslator *translationFile);
    static bool removeTranslator(QTranslator *translationFile);
    static QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1);

public slots:
    static void quit();

signals:
    /*!
        This signal is emitted by exec() after the event loop has stopped,
        just before exec() returns. Use it to release resources.

        \sa quit()
    */
    void aboutToQuit() { emitSignal(&QCoreApplication::aboutToQuit); }
};

#define qApp QCoreApplication::instance()

#define Q_DECLARE_TR_FUNCTIONS(context)                                                                                \
public:                                                                                                                \
    static QString tr(const char *sourceText, const char *disambiguation = nullptr, int n = -1) {                      \
        return QCoreApplication::translate(#context, sourceText, disambiguation, n);                                   \
    }                                                                                                                  \
                                                                                                                       \
private:
