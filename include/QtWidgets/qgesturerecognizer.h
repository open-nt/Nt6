// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qnamespace.h>

class QEvent;
class QGesture;
class QObject;

class QGestureRecognizer {
public:
    enum ResultFlag {
        Ignore = 0x0001,
        MayBeGesture = 0x0002,
        TriggerGesture = 0x0004,
        FinishGesture = 0x0008,
        CancelGesture = 0x0010,
        ResultState_Mask = 0x00ff,
        ConsumeEventHint = 0x0100,
        ResultHint_Mask = 0xff00
    };
    Q_DECLARE_FLAGS(Result, ResultFlag)

    QGestureRecognizer();
    virtual ~QGestureRecognizer();

    virtual QGesture *create(QObject *target);
    virtual Result recognize(QGesture *state, QObject *watched, QEvent *event) = 0;
    virtual void reset(QGesture *state);

    static Qt::GestureType registerRecognizer(QGestureRecognizer *recognizer);
    static void unregisterRecognizer(Qt::GestureType type);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGestureRecognizer::Result)
