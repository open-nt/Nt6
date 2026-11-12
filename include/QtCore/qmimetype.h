// MIT License - see LICENSE
#pragma once

#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <cstddef>
#include <utility>

class QMimeType {
    Q_GADGET
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(bool isDefault READ isDefault CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString comment READ comment CONSTANT)
    Q_PROPERTY(QString genericIconName READ genericIconName CONSTANT)
    Q_PROPERTY(QString iconName READ iconName CONSTANT)
    Q_PROPERTY(QStringList globPatterns READ globPatterns CONSTANT)
    Q_PROPERTY(QStringList parentMimeTypes READ parentMimeTypes CONSTANT)
    Q_PROPERTY(QStringList allAncestors READ allAncestors CONSTANT)
    Q_PROPERTY(QStringList aliases READ aliases CONSTANT)
    Q_PROPERTY(QStringList suffixes READ suffixes CONSTANT)
    Q_PROPERTY(QString preferredSuffix READ preferredSuffix CONSTANT)
    Q_PROPERTY(QString filterString READ filterString CONSTANT)
public:
    QMimeType();
    QMimeType(const QMimeType &other);
    QMimeType &operator=(const QMimeType &other);
    QMimeType(QMimeType &&other) noexcept;
    QMimeType &operator=(QMimeType &&other) noexcept;
    ~QMimeType();

    /*!
        Exchanges this type with \a other.
    */
    void swap(QMimeType &other) noexcept { m_name.swap(other.m_name); }

    /*!
        Returns \c true if \a lhs and \a rhs are the same type: if they have the same name.
    */
    friend bool operator==(const QMimeType &lhs, const QMimeType &rhs) { return lhs.m_name == rhs.m_name; }
    /*!
        Returns \c true if \a lhs and \a rhs are different types.
    */
    friend bool operator!=(const QMimeType &lhs, const QMimeType &rhs) { return lhs.m_name != rhs.m_name; }

    bool isValid() const;
    bool isDefault() const;

    QString name() const;
    QString comment() const;
    QString genericIconName() const;
    QString iconName() const;
    QStringList globPatterns() const;
    QStringList parentMimeTypes() const;
    QStringList allAncestors() const;
    QStringList aliases() const;
    QStringList suffixes() const;
    QString preferredSuffix() const;

    bool inherits(const QString &mimeTypeName) const;

    QString filterString() const;

private:
    friend class QMimeDatabase;
    explicit QMimeType(const QString &name) : m_name(name) {}
    QString m_name; // empty for an invalid type
};

std::size_t qHash(const QMimeType &key, std::size_t seed = 0) noexcept;
