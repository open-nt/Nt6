// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QEvent {
public:
    enum Type {
        None = 0,
        Timer = 1,
        MetaCall = 43,
        SockAct = 50,
        DeferredDelete = 52,
        User = 1000,
        MaxUser = 65535
    };

    explicit QEvent(Type type) : t(type) {}
    virtual ~QEvent();
    Q_DISABLE_COPY(QEvent)

    Type type() const { return t; }
    bool isAccepted() const { return accepted; }
    void setAccepted(bool a) { accepted = a; }
    void accept() { accepted = true; }
    void ignore() { accepted = false; }

    static int registerEventType(int hint = -1);

private:
    Type t;
    bool accepted = true;
};

class QTimerEvent : public QEvent {
public:
    explicit QTimerEvent(int timerId) : QEvent(Timer), id(timerId) {}
    ~QTimerEvent() override;
    int timerId() const { return id; }

private:
    int id;
};
