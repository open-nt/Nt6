// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

class QCoreApplication : public QObject {
    Q_OBJECT
public:
    QCoreApplication(int &argc, char **argv);
    ~QCoreApplication() override;

    static QCoreApplication *instance();

    // The command line as passed to the constructor, decoded as UTF-8.
    static QStringList arguments();
    // Defaults to the file name of argv[0].
    static void setApplicationName(const QString &name);
    static QString applicationName();
    static void setApplicationVersion(const QString &version);
    static QString applicationVersion();

    static int exec();
    static void exit(int returnCode = 0);
    static void processEvents(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents);
    static void sendPostedEvents(QObject *receiver = nullptr, int eventType = 0);

    // Takes ownership of `event`.
    static void postEvent(QObject *receiver, QEvent *event);
    static bool sendEvent(QObject *receiver, QEvent *event);
    // eventType 0 removes every pending event for `receiver`.
    static void removePostedEvents(QObject *receiver, int eventType = 0);

    virtual bool notify(QObject *receiver, QEvent *event);

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
