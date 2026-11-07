// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbusargument.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbuspendingcall.h>

#include <tuple>
#include <type_traits>

class QDBusPendingReplyBase : public QDBusPendingCall {
protected:
    /*!
        Constructs an object that waits for no call.
    */
    QDBusPendingReplyBase() = default;
    void assign(const QDBusPendingCall &call, const QList<QMetaType> &types);
    void assign(const QDBusMessage &message, const QList<QMetaType> &types);
    QVariant argumentAt(int index) const;
};

template <typename... Types> class QDBusPendingReply : public QDBusPendingReplyBase {
public:
    /*!
        The type of the reply's argument at the index \c Index.
    */
    template <int Index> using Select = std::tuple_element_t<static_cast<std::size_t>(Index), std::tuple<Types...>>;

    /*!
        Constructs an object that waits for no call.
    */
    QDBusPendingReply() = default;
    /*!
        Constructs a reply that follows the call \a call.
    */
    QDBusPendingReply(const QDBusPendingCall &call) { assign(call, types()); }
    /*!
        Constructs a reply that is complete and holds \a message.
    */
    QDBusPendingReply(const QDBusMessage &message) { assign(message, types()); }

    /*!
        Makes this reply follow the call \a call and returns a reference to it.
    */
    QDBusPendingReply &operator=(const QDBusPendingCall &call) {
        assign(call, types());
        return *this;
    }
    /*!
        Makes this reply hold \a message and returns a reference to it.
    */
    QDBusPendingReply &operator=(const QDBusMessage &message) {
        assign(message, types());
        return *this;
    }

    /*!
        Returns the number of arguments that the reply has.
    */
    static constexpr int count() { return static_cast<int>(sizeof...(Types)); }

    /*!
        Returns the reply's argument at \a index as a QVariant, which may hold a QDBusArgument.
        The call must be finished.
    */
    QVariant argumentAt(int index) const { return QDBusPendingReplyBase::argumentAt(index); }
    /*!
        Returns the reply's argument at \c Index as its own type. The call must be finished; the
        function waits for it if it is not.
    */
    template <int Index> Select<Index> argumentAt() const {
        const_cast<QDBusPendingReply *>(this)->waitForFinished();
        return qdbus_cast<Select<Index>>(QDBusPendingReplyBase::argumentAt(Index));
    }
    /*!
        Returns the first argument of the reply, waiting for the call if necessary.
    */
    Select<0> value() const { return argumentAt<0>(); }
    /*!
        Returns the first argument of the reply, waiting for the call if necessary.
    */
    operator Select<0>() const { return argumentAt<0>(); }

private:
    static QList<QMetaType> types() { return QList<QMetaType>{QMetaType::fromType<Types>()...}; }
};

// A reply without arguments.
template <> class QDBusPendingReply<> : public QDBusPendingReplyBase {
public:
    QDBusPendingReply() = default;
    QDBusPendingReply(const QDBusPendingCall &call) { assign(call, {}); }
    QDBusPendingReply(const QDBusMessage &message) { assign(message, {}); }
    QDBusPendingReply &operator=(const QDBusPendingCall &call) {
        assign(call, {});
        return *this;
    }
    QDBusPendingReply &operator=(const QDBusMessage &message) {
        assign(message, {});
        return *this;
    }
    static constexpr int count() { return 0; }
};
