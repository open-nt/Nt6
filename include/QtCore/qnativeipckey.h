// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qstring.h>

#include <utility>

class QNativeIpcKey {
public:
    /*!
        \enum QNativeIpcKey::Type
        \since 6.6
        This enum names the kinds of objects that a key can name.

        \value SystemV System V IPC objects (\c shmget(), \c semget()). Nt does not use them.
        \value PosixRealtime POSIX real-time IPC objects (\c shm_open(), \c sem_open()), which Nt uses.
        \value Windows Windows objects. Nt does not use them.
    */
    enum class Type : quint16 { SystemV = 0x51, PosixRealtime = 0x100, Windows };

    /*!
        The type of key that QSharedMemory and QSystemSemaphore use on this system: \c Type::PosixRealtime.
    */
    static constexpr Type DefaultTypeForOs = Type::PosixRealtime;
    static Type legacyDefaultTypeForOs() noexcept;

    /*!
        Constructs an empty key of the type DefaultTypeForOs.

        \sa isEmpty()
    */
    QNativeIpcKey() noexcept = default;
    /*!
        Constructs an empty key of the type \a type.
    */
    explicit QNativeIpcKey(Type type) noexcept : m_type(type) {}
    /*!
        Constructs a key of the type \a type with the native key \a k.
    */
    explicit QNativeIpcKey(const QString &k, Type type = DefaultTypeForOs) : m_key(k), m_type(type) {}

    /*!
        Exchanges this key with \a other.
    */
    void swap(QNativeIpcKey &other) noexcept {
        m_key.swap(other.m_key);
        std::swap(m_type, other.m_type);
        std::swap(m_valid, other.m_valid);
    }
    /*!
        Returns \c true if the native key is empty; otherwise returns \c false.
    */
    bool isEmpty() const noexcept { return m_key.isEmpty(); }
    /*!
        Returns \c true if the key has a type that the system knows; an empty key can be valid.
    */
    bool isValid() const noexcept {
        return m_valid && (m_type == Type::SystemV || m_type == Type::PosixRealtime || m_type == Type::Windows);
    }
    /*!
        Returns the type of the key.

        \sa setType()
    */
    Type type() const noexcept { return m_type; }
    /*!
        Sets the type of the key to \a type.
    */
    void setType(Type type) { m_type = type; }
    /*!
        Returns the native key: for \c Type::PosixRealtime, the name of the object, such as \c{/myapp-shm}.

        \sa setNativeKey()
    */
    QString nativeKey() const noexcept { return m_key; }
    /*!
        Sets the native key to \a newKey.
    */
    void setNativeKey(const QString &newKey) { m_key = newKey; }
    QString toString() const;
    static QNativeIpcKey fromString(const QString &string);

    /*!
        Returns \c true if \a a and \a b have the same type and native key.
    */
    friend bool operator==(const QNativeIpcKey &a, const QNativeIpcKey &b) noexcept {
        return a.m_type == b.m_type && a.m_key == b.m_key && a.m_valid == b.m_valid;
    }
    /*!
        Returns \c true if \a a and \a b differ in type or native key.
    */
    friend bool operator!=(const QNativeIpcKey &a, const QNativeIpcKey &b) noexcept { return !(a == b); }
    /*!
        Returns a hash of \a key, starting from \a seed.
    */
    friend size_t qHash(const QNativeIpcKey &key, size_t seed = 0) noexcept {
        return qHashMulti(seed, key.m_key, static_cast<quint16>(key.m_type));
    }

private:
    QString m_key;
    Type m_type = DefaultTypeForOs;
    bool m_valid = true; // false for the result of fromString() on text that is not a key
};
