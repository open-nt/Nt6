// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>
#include <new>
#include <type_traits>

namespace QtGlobalStaticPrivate {
// Defined for the lifetime of the program so a destroyed object is detectable.
inline constexpr int kUninitialized = 0;
inline constexpr int kInitialized = -1;
inline constexpr int kDestroyed = -2;
} // namespace QtGlobalStaticPrivate

/*!
    \class QGlobalStatic
    \brief The QGlobalStatic class is used to implement a global static object.

    Applications use it through the Q_GLOBAL_STATIC and
    Q_GLOBAL_STATIC_WITH_ARGS macros, not by naming the class. The macros
    define a variable that behaves like a pointer to an object of the given
    type. The object is created the first time it is used, in a thread-safe
    way, and destroyed when the program exits:

    \code
    Q_GLOBAL_STATIC(QStringList, sharedNames)

    void remember(const QString &name) { sharedNames->append(name); }
    \endcode

    Unlike a plain static variable, the object is never constructed if it is
    not used, and code that runs during the program's shutdown can find out
    that it is gone with isDestroyed(): the pointer is then \c nullptr.

    \differences
    None in behavior. The macros define a \c static variable in the file that
    uses them, so declare each global once, in a \c .cpp file.
    \enddifferences
*/
template <typename Holder> class QGlobalStatic {
public:
    /*!
        The type of the object that the variable stands for.
    */
    using Type = typename Holder::Type;

    /*!
        Returns \c true if the object has been created and not yet destroyed.

        \sa isDestroyed()
    */
    bool exists() const noexcept { return guard().load() == QtGlobalStaticPrivate::kInitialized; }
    /*!
        Returns \c true if the object has been destroyed, which happens when
        the program exits.

        \sa exists()
    */
    bool isDestroyed() const noexcept { return guard().load() == QtGlobalStaticPrivate::kDestroyed; }
    /*!
        Returns a pointer to the object, creating it if necessary, or
        \c nullptr if it has been destroyed.
    */
    Type *operator()() { return instance(); }
    /*!
        Returns a pointer to the object, creating it if necessary, or
        \c nullptr if it has been destroyed.
    */
    Type *operator->() { return instance(); }
    /*!
        Returns a reference to the object, creating it if necessary. Calling
        this after the object has been destroyed is undefined.
    */
    Type &operator*() { return *instance(); }
    /*!
        Returns a pointer to the object, creating it if necessary, or
        \c nullptr if it has been destroyed.
    */
    operator Type *() { return instance(); }

private:
    static std::atomic<int> &guard() noexcept {
        static std::atomic<int> g{QtGlobalStaticPrivate::kUninitialized};
        return g;
    }

    struct Storage {
        Storage() : pointer(Holder::create(buffer)) { guard().store(QtGlobalStaticPrivate::kInitialized); }
        ~Storage() {
            pointer->~Type();
            guard().store(QtGlobalStaticPrivate::kDestroyed);
        }
        Storage(const Storage &) = delete;
        Storage &operator=(const Storage &) = delete;

        alignas(Type) unsigned char buffer[sizeof(Type)];
        Type *pointer;
    };

    static Type *instance() {
        if (guard().load() == QtGlobalStaticPrivate::kDestroyed)
            return nullptr;
        static Storage storage;
        return storage.pointer;
    }
};

#define Q_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ARGS)                                                                    \
    namespace {                                                                                                        \
    struct Q_QGS_##NAME {                                                                                              \
        using Type = TYPE;                                                                                             \
        static Type *create(void *where) { return new (where) Type ARGS; }                                             \
    };                                                                                                                 \
    }                                                                                                                  \
    [[maybe_unused]] static QGlobalStatic<Q_QGS_##NAME> NAME;

#define Q_GLOBAL_STATIC(TYPE, NAME) Q_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ())
