// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <utility>

class QDBusObjectPath {
public:
    /*!
        Constructs an empty object path, which is not a valid path on the bus.
    */
    QDBusObjectPath() = default;
    explicit QDBusObjectPath(const char *path);
    explicit QDBusObjectPath(QLatin1StringView path);
    explicit QDBusObjectPath(const QString &path);

    /*!
        Returns this path as a QString.
    */
    QString path() const { return m_path; }
    void setPath(const QString &path);
    /*!
        Swaps this path with \a other.
    */
    void swap(QDBusObjectPath &other) noexcept { m_path.swap(other.m_path); }

    /*!
        Returns \c true if \a a and \a b are the same path.
    */
    friend bool operator==(const QDBusObjectPath &a, const QDBusObjectPath &b) { return a.m_path == b.m_path; }
    /*!
        Returns \c true if \a a and \a b are different paths.
    */
    friend bool operator!=(const QDBusObjectPath &a, const QDBusObjectPath &b) { return a.m_path != b.m_path; }
    /*!
        Returns \c true if \a a sorts before \a b.
    */
    friend bool operator<(const QDBusObjectPath &a, const QDBusObjectPath &b) { return a.m_path < b.m_path; }

private:
    QString m_path;
};

// Returns the hash of path, using seed.
inline size_t qHash(const QDBusObjectPath &path, size_t seed = 0) { return qHash(path.path(), seed); }

class QDBusSignature {
public:
    /*!
        Constructs an empty signature, which is valid and describes no values.
    */
    QDBusSignature() = default;
    explicit QDBusSignature(const char *signature);
    explicit QDBusSignature(QLatin1StringView signature);
    explicit QDBusSignature(const QString &signature);

    /*!
        Returns this signature as a QString.
    */
    QString signature() const { return m_signature; }
    void setSignature(const QString &signature);
    /*!
        Swaps this signature with \a other.
    */
    void swap(QDBusSignature &other) noexcept { m_signature.swap(other.m_signature); }

    /*!
        Returns \c true if \a a and \a b are the same signature.
    */
    friend bool operator==(const QDBusSignature &a, const QDBusSignature &b) { return a.m_signature == b.m_signature; }
    /*!
        Returns \c true if \a a and \a b are different signatures.
    */
    friend bool operator!=(const QDBusSignature &a, const QDBusSignature &b) { return a.m_signature != b.m_signature; }
    /*!
        Returns \c true if \a a sorts before \a b.
    */
    friend bool operator<(const QDBusSignature &a, const QDBusSignature &b) { return a.m_signature < b.m_signature; }

private:
    QString m_signature;
};

// Returns the hash of signature, using seed.
inline size_t qHash(const QDBusSignature &signature, size_t seed = 0) { return qHash(signature.signature(), seed); }

class QDBusVariant {
public:
    /*!
        Constructs a QDBusVariant that holds an invalid QVariant.
    */
    QDBusVariant() = default;
    /*!
        Constructs a QDBusVariant that holds a copy of \a variant.
    */
    explicit QDBusVariant(const QVariant &variant) : m_variant(variant) {}

    /*!
        Returns the value in this variant.
    */
    QVariant variant() const { return m_variant; }
    /*!
        Replaces the value in this variant with \a variant.
    */
    void setVariant(const QVariant &variant) { m_variant = variant; }
    /*!
        Swaps this variant with \a other.
    */
    void swap(QDBusVariant &other) noexcept { std::swap(m_variant, other.m_variant); }

    /*!
        Returns \c true if \a a and \a b hold equal values.
    */
    friend bool operator==(const QDBusVariant &a, const QDBusVariant &b) { return a.m_variant == b.m_variant; }

private:
    QVariant m_variant;
};

Q_DECLARE_METATYPE(QDBusVariant)
Q_DECLARE_METATYPE(QDBusObjectPath)
Q_DECLARE_METATYPE(QDBusSignature)
