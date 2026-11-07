// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QDBusConnection;
class QDBusMessage;

class QDBusVirtualObject : public QObject {
    Q_OBJECT
public:
    explicit QDBusVirtualObject(QObject *parent = nullptr);
    ~QDBusVirtualObject() override;

    /*!
        Returns the introspection XML for \a path: the \c interface elements only, without the
        enclosing \c node element. It may be empty.
    */
    virtual QString introspect(const QString &path) const = 0;
    /*!
        Handles the method call \a message, which arrived on \a connection. Returns \c true if
        the call was handled, and the function is then responsible for any reply; otherwise
        returns \c false and the caller gets an error.
    */
    virtual bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) = 0;
};
