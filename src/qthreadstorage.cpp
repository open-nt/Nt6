// MIT License - see LICENSE
#include <QtCore/qthreadstorage.h>

#include "threaddata_p.h"

#include <atomic>

/*!
    \class QThreadStorage
    \brief The QThreadStorage class provides per-thread data storage.

    A QThreadStorage object keeps one value for each thread that uses it, so a function that
    is called from several threads can have its own cache in each. A thread's value is
    destroyed when the thread exits, or earlier if the QThreadStorage is destroyed in that
    thread or the value is replaced.

    \code
    QThreadStorage<QString *> prefix;

    void log(const QString &text)
    {
        if (!prefix.hasLocalData())
            prefix.setLocalData(new QString(makePrefix()));
        write(*prefix.localData() + text);
    }
    \endcode

    \differences
    Destroying a QThreadStorage destroys the value of the calling thread only. The values of
    other threads stay until those threads exit. There is no \c isDestroyed().
    \enddifferences

    \sa QThread, QThreadPool
*/

/*!
    \fn QThreadStorage::QThreadStorage()

    Constructs an empty storage. No thread has data in it.
*/

/*!
    \fn QThreadStorage::~QThreadStorage()

    Destroys the storage and the calling thread's value in it.
*/

namespace QtPrivate {

std::size_t allocateThreadStorageId() {
    static std::atomic<std::size_t> next{1};
    return next.fetch_add(1);
}

void *threadStorageGet(std::size_t id) {
    auto &storage = ThreadData::current()->storage;
    const auto it = storage.find(id);
    return it == storage.end() ? nullptr : it->second.data;
}

void threadStorageSet(std::size_t id, void *data, void (*destroy)(void *)) {
    auto &storage = ThreadData::current()->storage;
    const auto it = storage.find(id);
    StorageSlot old{nullptr, nullptr};
    if (it != storage.end()) {
        old = it->second;
        storage.erase(it);
    }
    if (data)
        storage[id] = {data, destroy};
    // The old value's destructor may use this storage again, so run it last.
    if (old.data)
        old.destroy(old.data);
}

} // namespace QtPrivate
