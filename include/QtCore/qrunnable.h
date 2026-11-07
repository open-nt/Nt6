// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <functional>

class QRunnable {
public:
    QRunnable() = default;
    virtual ~QRunnable();
    Q_DISABLE_COPY(QRunnable)

    virtual void run() = 0;

    static QRunnable *create(std::function<void()> functionToRun);

    bool autoDelete() const { return m_autoDelete; }
    void setAutoDelete(bool autoDelete) { m_autoDelete = autoDelete; }

private:
    bool m_autoDelete = true;
};
