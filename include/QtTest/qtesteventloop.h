// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qobject.h>

#include <chrono>

class QTestEventLoop : public QObject {
public:
    explicit QTestEventLoop(QObject *parent = nullptr);
    ~QTestEventLoop() override;

    static QTestEventLoop &instance();

    void enterLoop(int seconds);
    void enterLoopMSecs(int milliseconds);
    void enterLoop(std::chrono::milliseconds timeout);
    void changeInterval(int milliseconds);
    bool timeout() const noexcept;

public slots:
    void exitLoop();

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    QEventLoop *m_loop = nullptr;
    int m_timer = 0;
    bool m_timedOut = false;
};
