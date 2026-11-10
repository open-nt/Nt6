// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreapplication.h>

#include <cstddef>
#include <mutex>
#include <tuple>
#include <utility>

/*!
    \class QApplicationStatic
    \brief The QApplicationStatic class holds a global object that lives as long as the application object.
    \since 6.3

    Programs use it through the \c Q_APPLICATION_STATIC macro and do not name the class. The macro
    defines a variable that behaves like a pointer to an object of the given type; the arguments
    after the name, if any, are passed to its constructor. The object is created the first time it
    is used and destroyed with the QCoreApplication, which makes it the right kind of global for a
    QObject or anything else that needs the application:

    \code
    Q_APPLICATION_STATIC(QTimer, heartbeat)

    void startHeartbeat() { heartbeat->start(1000); }
    \endcode

    If the program creates another application object later, the next use creates a new object.

    \differences
    An object that is first used when there is no application object is created all the same, and
    destroyed with the next application object; Qt requires the application object to exist. The
    macro defines a \c static variable in the file that uses it, so declare each object once, in a
    \c .cpp file.
    \enddifferences

    \sa QGlobalStatic, QCoreApplication
*/
template <typename Holder> class QApplicationStatic {
public:
    /*!
        The type of the object that the variable stands for.
    */
    using Type = typename Holder::Type;

    /*!
        Returns \c true if the object exists now: it has been used and not been destroyed since.
    */
    bool exists() const noexcept {
        const std::lock_guard<std::mutex> lock(state().mutex);
        return state().pointer != nullptr;
    }
    /*!
        Returns a pointer to the object, creating it if needed.
    */
    operator Type *() { return instance(); }
    /*!
        Returns a pointer to the object, creating it if needed.
    */
    Type *operator()() { return instance(); }
    /*!
        Returns a pointer to the object, creating it if needed.
    */
    Type *operator->() { return instance(); }
    /*!
        Returns a reference to the object, creating it if needed.
    */
    Type &operator*() { return *instance(); }
    /*!
        Destroys the object now, if it exists. The next use creates a new one.
    */
    void reset() {
        qRemovePostRoutine(&destroy);
        destroy();
    }

private:
    struct State {
        std::mutex mutex;
        Type *pointer = nullptr;
    };
    static State &state() {
        static State s;
        return s;
    }
    static void destroy() {
        Type *old = nullptr;
        {
            const std::lock_guard<std::mutex> lock(state().mutex);
            old = state().pointer;
            state().pointer = nullptr;
        }
        delete old;
    }
    static Type *instance() {
        const std::lock_guard<std::mutex> lock(state().mutex);
        if (!state().pointer) {
            state().pointer = Holder::create();
            qAddPostRoutine(&destroy);
        }
        return state().pointer;
    }
};

namespace QtPrivate {

struct ApplicationStaticEnd {};

template <typename T, typename Tuple, std::size_t... I>
T *newApplicationStatic(Tuple &&arguments, std::index_sequence<I...>) {
    return new T(std::get<I>(std::forward<Tuple>(arguments))...);
}
// Constructs a T from the arguments but the last, which is the marker that the macro appends.
template <typename T, typename... Args> T *newApplicationStatic(Args &&...arguments) {
    return newApplicationStatic<T>(std::forward_as_tuple(std::forward<Args>(arguments)...),
                                   std::make_index_sequence<sizeof...(Args) - 1>());
}

} // namespace QtPrivate

// The name and the arguments of the constructor share the "..." so that it is never empty, which C++17 does not
// allow; the helpers take them apart again.
#define NT_QAS_FIRST_(FIRST, ...) FIRST
#define NT_QAS_REST_(FIRST, ...) __VA_ARGS__
#define NT_QAS_HOLDER_(NAME) Q_QAS_##NAME
#define NT_QAS_HOLDER(NAME) NT_QAS_HOLDER_(NAME)
#define NT_QAS_DEFINE_(TYPE, NAME, ...)                                                                                \
    namespace {                                                                                                        \
    struct NT_QAS_HOLDER(NAME) {                                                                                       \
        using Type = TYPE;                                                                                             \
        static Type *create() { return QtPrivate::newApplicationStatic<Type>(__VA_ARGS__); }                           \
    };                                                                                                                 \
    }                                                                                                                  \
    [[maybe_unused]] static QApplicationStatic<NT_QAS_HOLDER(NAME)> NAME;
#define NT_QAS_EXPAND_(...) __VA_ARGS__
#define Q_APPLICATION_STATIC(TYPE, ...)                                                                                \
    NT_QAS_EXPAND_(NT_QAS_DEFINE_(TYPE, __VA_ARGS__, QtPrivate::ApplicationStaticEnd()))
