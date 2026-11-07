// MIT License - see LICENSE
#include <QtCore/qcoreevent.h>

#include <atomic>
#include <mutex>
#include <set>

QEvent::~QEvent() = default;

QTimerEvent::~QTimerEvent() = default;

int QEvent::registerEventType(int hint) {
    static std::mutex m;
    static std::set<int> used;
    static int next = QEvent::MaxUser;

    std::lock_guard<std::mutex> lk(m);
    if (hint >= QEvent::User && hint <= QEvent::MaxUser && used.insert(hint).second)
        return hint;
    while (next >= QEvent::User && used.count(next))
        --next;
    if (next < QEvent::User)
        return -1;
    used.insert(next);
    return next--;
}
