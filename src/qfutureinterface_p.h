// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qfutureinterface.h>
#include <QtCore/qstring.h>

// What a future reports to an observer, such as a QFutureWatcher.
class QFutureCallOutEvent : public QEvent {
public:
    enum CallOutType { Started, Finished, Canceled, Suspending, Suspended, Resumed, Progress, ProgressRange, ResultsReady };

    QFutureCallOutEvent(CallOutType type, int index1 = 0, int index2 = 0, QString text = QString());
    QFutureCallOutEvent *clone() const;
    static QEvent::Type eventType();

    CallOutType callOutType;
    int index1;
    int index2;
    QString text;
};

// Implemented by an observer. The functions are called by the thread that reports, with the lock of the
// future held, so they must not call back into the future.
class QFutureCallOutInterface {
public:
    virtual ~QFutureCallOutInterface();
    virtual void callOutInterfaceEvent(const QFutureCallOutEvent &event) = 0;
};
