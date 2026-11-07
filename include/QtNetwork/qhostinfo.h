// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qhostaddress.h>

#include <functional>
#include <type_traits>
#include <utility>

class QHostInfo {
public:
    enum HostInfoError { NoError, HostNotFound, UnknownError };

    explicit QHostInfo(int lookupId = -1);

    QString hostName() const;
    void setHostName(const QString &name);
    QList<QHostAddress> addresses() const;
    void setAddresses(const QList<QHostAddress> &addresses);
    HostInfoError error() const;
    void setError(HostInfoError error);
    QString errorString() const;
    void setErrorString(const QString &errorString);
    int lookupId() const;
    void setLookupId(int id);

    static QHostInfo fromName(const QString &name);
    static void abortHostLookup(int lookupId);
    static QString localHostName();
    static QString localDomainName();

    /*!
        Looks up \a name on a background thread and calls \a functor with the
        result from the event loop. Returns the lookup's ID, which
        abortHostLookup() accepts.

        \sa fromName()
    */
    template <typename Func>
    static int lookupHost(const QString &name, Func &&functor) {
        return startLookup(name, nullptr, std::function<void(const QHostInfo &)>(std::forward<Func>(functor)));
    }

    /*!
        \overload

        Looks up \a name and calls \a functor, unless \a context is destroyed
        first.
    */
    template <typename Func>
    static int lookupHost(const QString &name, const QObject *context, Func &&functor) {
        return startLookup(name, context, std::function<void(const QHostInfo &)>(std::forward<Func>(functor)));
    }

private:
    static int startLookup(const QString &name, const QObject *context, std::function<void(const QHostInfo &)> callback);

    QString m_name;
    QList<QHostAddress> m_addresses;
    HostInfoError m_error = NoError;
    QString m_errorString;
    int m_id;
};
