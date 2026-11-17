// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

#include <chrono>
#include <type_traits>

class QSignalSpy : public QObject, public QList<QList<QVariant>> {
public:
    /*!
        Constructs a spy that records each emission of \a signalFunction from
        \a object. Use isValid() to check that it is listening.

        \a signalFunction is a pointer to the signal, as passed to QObject::connect().
    */
    template <typename Object, typename SignalClass, typename... Args>
    QSignalSpy(const Object *object, void (SignalClass::*signalFunction)(Args...)) {
        if (!object) {
            warnNullObject();
            return;
        }
        m_valid = static_cast<bool>(QObject::connect(
            object, signalFunction, this, [this](const std::decay_t<Args> &...args) { record(args...); },
            Qt::DirectConnection));
    }

    QSignalSpy(const QObject *object, const char *signature);
    QSignalSpy(const QObject *object, const QMetaMethod &method);

    bool isValid() const noexcept;
    QByteArray signal() const { return m_signature; }
    bool wait(int timeout = 5000);
    bool wait(std::chrono::milliseconds timeout);

private:
    template <typename... Args> void record(const Args &...args) {
        append(QList<QVariant>{QVariant::fromValue(args)...});
    }
    static void warnNullObject();
    void listen(const QObject *object, const QMetaMethod &method);

    bool m_valid = false;
    QByteArray m_signature; // of a signal given by name or meta-method
};
