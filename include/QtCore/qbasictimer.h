// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

// A timer without signals: `object` receives QTimerEvent.
class QBasicTimer {
public:
    QBasicTimer() noexcept = default;
    ~QBasicTimer() { stop(); }
    QBasicTimer(const QBasicTimer &) = delete;
    QBasicTimer &operator=(const QBasicTimer &) = delete;

    bool isActive() const noexcept { return m_id != 0; }
    int timerId() const noexcept { return m_id; }
    void start(int msec, QObject *object) {
        stop();
        m_object = object;
        m_id = object ? object->startTimer(msec) : 0;
    }
    void start(std::chrono::milliseconds interval, QObject *object) {
        start(static_cast<int>(interval.count()), object);
    }
    void stop() {
        if (m_id != 0 && m_object)
            m_object->killTimer(m_id);
        m_id = 0;
        m_object = nullptr;
    }

private:
    QPointer<QObject> m_object;
    int m_id = 0;
};
