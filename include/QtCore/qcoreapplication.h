// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qobject.h>

class QCoreApplication : public QObject {
    Q_OBJECT
public:
    QCoreApplication(int &argc, char **argv);
    ~QCoreApplication() override;

    static QCoreApplication *instance();

    static int exec();
    static void exit(int returnCode = 0);
    static void processEvents(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents);

    // Takes ownership of `event`.
    static void postEvent(QObject *receiver, QEvent *event);
    static bool sendEvent(QObject *receiver, QEvent *event);
    // eventType 0 removes every pending event for `receiver`.
    static void removePostedEvents(QObject *receiver, int eventType = 0);

    virtual bool notify(QObject *receiver, QEvent *event);

public slots:
    static void quit();

signals:
    void aboutToQuit() { emitSignal(&QCoreApplication::aboutToQuit); }
};

#define qApp QCoreApplication::instance()
