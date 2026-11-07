// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtDBus/qdbuserror.h>

class QDBusConnection;
class QDBusMessage;
struct QDBusContextPrivate;

class QDBusContext {
public:
    QDBusContext();
    virtual ~QDBusContext();
    QDBusContext(const QDBusContext &) = delete;
    QDBusContext &operator=(const QDBusContext &) = delete;

    bool calledFromDBus() const;
    QDBusConnection connection() const;
    const QDBusMessage &message() const;

    bool isDelayedReply() const;
    void setDelayedReply(bool enable) const;
    void sendErrorReply(const QString &name, const QString &message = QString()) const;
    void sendErrorReply(QDBusError::ErrorType type, const QString &message = QString()) const;

private:
    friend struct QDBusContextPrivate;
    QDBusContextPrivate *d_ptr = nullptr;
};
