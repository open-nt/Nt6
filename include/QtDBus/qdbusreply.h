// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusextratypes.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbuspendingcall.h>

void qDBusReplyFill(const QDBusMessage &reply, QDBusError &error, QVariant &data);

template <typename T> class QDBusReply {
public:
    /*!
        Constructs a reply from the message \a reply.
    */
    QDBusReply(const QDBusMessage &reply) { *this = reply; }
    /*!
        Constructs a reply from \a call, waiting for the call to finish.
    */
    QDBusReply(const QDBusPendingCall &call) { *this = call; }
    /*!
        Constructs a reply that holds the error \a error.
    */
    QDBusReply(const QDBusError &error = QDBusError()) : m_error(error) {}

    /*!
        Extracts the value from \a reply and returns a reference to this object.
    */
    QDBusReply &operator=(const QDBusMessage &reply) {
        QVariant data{QMetaType::fromType<T>()};
        qDBusReplyFill(reply, m_error, data);
        if (m_error.isValid())
            m_data = T();
        else
            m_data = data.value<T>();
        return *this;
    }
    /*!
        Waits for \a call and extracts its value. Returns a reference to this object.
    */
    QDBusReply &operator=(const QDBusPendingCall &call) {
        QDBusPendingCall other(call);
        other.waitForFinished();
        return *this = other.reply();
    }
    /*!
        Makes this object hold \a error. Returns a reference to this object.
    */
    QDBusReply &operator=(const QDBusError &error) {
        m_error = error;
        m_data = T();
        return *this;
    }

    /*!
        Returns \c true if the reply holds a value rather than an error.
    */
    bool isValid() const { return !m_error.isValid(); }
    /*!
        Returns the error, which is invalid if the call succeeded.
    */
    const QDBusError &error() const { return m_error; }
    /*!
        Returns the value, or a default-constructed one if the call failed.
    */
    T value() const { return m_data; }
    /*!
        Returns the value, or a default-constructed one if the call failed.
    */
    operator T() const { return m_data; }

private:
    QDBusError m_error;
    T m_data{};
};

// The value of a QVariant reply arrives wrapped in a D-Bus variant.
template <> inline QDBusReply<QVariant> &QDBusReply<QVariant>::operator=(const QDBusMessage &reply) {
    QVariant data{QMetaType::fromType<QDBusVariant>()};
    qDBusReplyFill(reply, m_error, data);
    m_data = m_error.isValid() ? QVariant() : data.value<QDBusVariant>().variant();
    return *this;
}

// A reply to a call that returns nothing.
template <> class QDBusReply<void> {
public:
    QDBusReply(const QDBusMessage &reply) : m_error(reply) {}
    QDBusReply(const QDBusPendingCall &call) { *this = call; }
    QDBusReply(const QDBusError &error = QDBusError()) : m_error(error) {}

    QDBusReply &operator=(const QDBusMessage &reply) {
        m_error = QDBusError(reply);
        return *this;
    }
    QDBusReply &operator=(const QDBusPendingCall &call) {
        QDBusPendingCall other(call);
        other.waitForFinished();
        return *this = other.reply();
    }
    QDBusReply &operator=(const QDBusError &error) {
        m_error = error;
        return *this;
    }

    bool isValid() const { return !m_error.isValid(); }
    const QDBusError &error() const { return m_error; }

private:
    QDBusError m_error;
};
