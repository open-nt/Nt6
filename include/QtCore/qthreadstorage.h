// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace QtPrivate {

std::size_t allocateThreadStorageId();
void *threadStorageGet(std::size_t id);
// Takes ownership of `data`; destroys the value that the calling thread stored before.
void threadStorageSet(std::size_t id, void *data, void (*destroy)(void *));

} // namespace QtPrivate

template <typename T> class QThreadStorage {
public:
    QThreadStorage() : m_id(QtPrivate::allocateThreadStorageId()) {}
    ~QThreadStorage() { QtPrivate::threadStorageSet(m_id, nullptr, nullptr); }
    Q_DISABLE_COPY(QThreadStorage)

    /*!
        Returns \c true if the calling thread has data in this storage; otherwise returns
        \c false.

        \sa localData(), setLocalData()
    */
    bool hasLocalData() const { return QtPrivate::threadStorageGet(m_id) != nullptr; }

    /*!
        Returns a reference to the data that the calling thread stores here. If the thread has
        none yet, a default-constructed value is stored first.

        \sa hasLocalData(), setLocalData()
    */
    T &localData() {
        void *p = QtPrivate::threadStorageGet(m_id);
        if (!p) {
            p = new T();
            QtPrivate::threadStorageSet(m_id, p, &destroy);
        }
        return *static_cast<T *>(p);
    }

    /*!
        \overload
        Returns a copy of the data that the calling thread stores here, or a default-constructed
        value if it has none. Nothing is stored.
    */
    T localData() const {
        const void *p = QtPrivate::threadStorageGet(m_id);
        return p ? *static_cast<const T *>(p) : T();
    }

    /*!
        Stores \a data as the calling thread's value. Data the thread stored before is destroyed.
        If \c T is a pointer type, the storage owns the object it points to and deletes it, and
        passing \c nullptr clears the data. The data is destroyed when the thread exits.

        \sa localData(), hasLocalData()
    */
    void setLocalData(T data) {
        if constexpr (std::is_pointer_v<T>) {
            if (data == nullptr) {
                QtPrivate::threadStorageSet(m_id, nullptr, nullptr);
                return;
            }
        }
        QtPrivate::threadStorageSet(m_id, new T(std::move(data)), &destroy);
    }

private:
    static void destroy(void *p) {
        auto *value = static_cast<T *>(p);
        if constexpr (std::is_pointer_v<T>)
            delete *value;
        delete value;
    }

    std::size_t m_id;
};
