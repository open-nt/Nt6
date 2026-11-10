// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <memory>

class QEventLoop;
class QThread;

namespace QtPrivate {
struct LockerCounter;
} // namespace QtPrivate

class QEventLoopLocker {
public:
    QEventLoopLocker();
    explicit QEventLoopLocker(QEventLoop *loop);
    explicit QEventLoopLocker(QThread *thread);
    ~QEventLoopLocker();
    Q_DISABLE_COPY(QEventLoopLocker)

private:
    std::shared_ptr<QtPrivate::LockerCounter> m_counter;
};
