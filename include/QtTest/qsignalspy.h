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
        Constructs a spy that records each emission of \a signal from
        \a object. Use isValid() to check that it is listening.

        \a signal is a pointer to the signal, as passed to QObject::connect().
    */
    template <typename Object, typename SignalClass, typename... Args>
    QSignalSpy(const Object *object, void (SignalClass::*signal)(Args...)) {
        if (!object) {
            warnNullObject();
            return;
        }
        m_valid = static_cast<bool>(QObject::connect(
            object, signal, this, [this](const std::decay_t<Args> &...args) { record(args...); },
            Qt::DirectConnection));
    }

    bool isValid() const noexcept;
    bool wait(int timeout = 5000);
    bool wait(std::chrono::milliseconds timeout);

private:
    template <typename... Args> void record(const Args &...args) {
        append(QList<QVariant>{QVariant::fromValue(args)...});
    }
    static void warnNullObject();

    bool m_valid = false;
};
