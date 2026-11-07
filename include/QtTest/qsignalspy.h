// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <chrono>
#include <memory>
#include <type_traits>
#include <typeinfo>

class QSignalSpyArgument {
public:
    /*!
        Constructs an invalid argument.

        \sa isValid()
    */
    QSignalSpyArgument() = default;

    /*!
        Constructs an argument holding a copy of \a value.
    */
    template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, QSignalSpyArgument>>>
    explicit QSignalSpyArgument(const T &value) : m_type(&typeid(T)), m_value(std::make_shared<const T>(value)) {
        if constexpr (std::is_enum_v<T>) {
            m_kind = Integer;
            m_integer = static_cast<qlonglong>(value);
        } else if constexpr (std::is_integral_v<T>) {
            m_kind = Integer;
            m_integer = static_cast<qlonglong>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            m_kind = Real;
            m_real = static_cast<double>(value);
        }
    }

    /*!
        Returns \c true if this argument holds a value.
    */
    bool isValid() const noexcept { return m_type != nullptr; }

    /*!
        Returns \c true if value<T>() can return the argument: it holds a
        \c T, or both it and \c T are numbers or enums.

        \sa value()
    */
    template <typename T> bool canConvert() const {
        if (m_type && *m_type == typeid(T))
            return true;
        return m_kind != None && (std::is_arithmetic_v<T> || std::is_enum_v<T>);
    }

    /*!
        Returns the argument as a \c T, or a default-constructed \c T if
        canConvert<T>() is \c false.

        \sa canConvert()
    */
    template <typename T> T value() const {
        if (m_type && *m_type == typeid(T))
            return *static_cast<const T *>(m_value.get());
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            if (m_kind == Integer)
                return static_cast<T>(m_integer);
            if (m_kind == Real)
                return static_cast<T>(m_real);
        }
        return T();
    }

    /*!
        Returns the argument as an \c int, or \c 0 if it is not a number.
    */
    int toInt() const { return value<int>(); }
    /*!
        Returns the argument as a \c qlonglong, or \c 0 if it is not a number.
    */
    qlonglong toLongLong() const { return value<qlonglong>(); }
    /*!
        Returns the argument as a \c double, or \c 0.0 if it is not a number.
    */
    double toDouble() const { return value<double>(); }
    /*!
        Returns \c true if the argument is a number other than zero.
    */
    bool toBool() const { return m_kind == Integer ? m_integer != 0 : (m_kind == Real && m_real != 0.0); }
    /*!
        Returns the argument if it is a QString; otherwise returns a null string.
    */
    QString toString() const { return value<QString>(); }
    /*!
        Returns the argument if it is a QByteArray; otherwise returns a null array.
    */
    QByteArray toByteArray() const { return value<QByteArray>(); }

private:
    enum Kind { None, Integer, Real };

    const std::type_info *m_type = nullptr;
    std::shared_ptr<const void> m_value;
    Kind m_kind = None;
    qlonglong m_integer = 0;
    double m_real = 0.0;
};

class QSignalSpy : public QObject, public QList<QList<QSignalSpyArgument>> {
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
        append(QList<QSignalSpyArgument>{QSignalSpyArgument(args)...});
    }
    static void warnNullObject();

    bool m_valid = false;
};
