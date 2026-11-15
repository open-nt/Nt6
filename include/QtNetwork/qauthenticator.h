// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

class QAuthenticator {
public:
    QAuthenticator() = default;
    QAuthenticator(const QAuthenticator &other) = default;
    QAuthenticator &operator=(const QAuthenticator &other) = default;
    ~QAuthenticator() = default;

    bool operator==(const QAuthenticator &other) const;
    bool operator!=(const QAuthenticator &other) const { return !(*this == other); }

    QString user() const { return m_user; }
    void setUser(const QString &user) { m_user = user; }
    QString password() const { return m_password; }
    void setPassword(const QString &password) { m_password = password; }
    QString realm() const { return m_realm; }
    void setRealm(const QString &realm) { m_realm = realm; }

    QVariant option(const QString &opt) const { return m_options.value(opt); }
    QVariantHash options() const { return m_options; }
    void setOption(const QString &opt, const QVariant &value) { m_options.insert(opt, value); }

    bool isNull() const;

private:
    QString m_user;
    QString m_password;
    QString m_realm;
    QVariantHash m_options;
};
