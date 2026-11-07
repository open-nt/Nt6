// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbuserror.h>

class QDBusMessagePrivate;

class QDBusMessage {
public:
    enum MessageType { InvalidMessage, MethodCallMessage, ReplyMessage, ErrorMessage, SignalMessage };

    QDBusMessage();
    QDBusMessage(const QDBusMessage &other);
    QDBusMessage &operator=(const QDBusMessage &other);
    ~QDBusMessage();

    static QDBusMessage createSignal(const QString &path, const QString &interface, const QString &name);
    static QDBusMessage createTargetedSignal(const QString &service, const QString &path, const QString &interface,
                                             const QString &name);
    static QDBusMessage createMethodCall(const QString &destination, const QString &path, const QString &interface,
                                         const QString &method);
    static QDBusMessage createError(const QString &name, const QString &message);
    static QDBusMessage createError(const QDBusError &error);
    static QDBusMessage createError(QDBusError::ErrorType type, const QString &message);

    QDBusMessage createReply(const QList<QVariant> &arguments = QList<QVariant>()) const;
    QDBusMessage createReply(const QVariant &argument) const;
    QDBusMessage createErrorReply(const QString &name, const QString &message) const;
    QDBusMessage createErrorReply(const QDBusError &error) const;
    QDBusMessage createErrorReply(QDBusError::ErrorType type, const QString &message) const;

    QString service() const;
    QString path() const;
    QString interface() const;
    QString member() const;
    QString errorName() const;
    QString errorMessage() const;
    MessageType type() const;
    QString signature() const;

    bool isReplyRequired() const;
    void setDelayedReply(bool enable) const;
    bool isDelayedReply() const;
    void setAutoStartService(bool enable);
    bool autoStartService() const;
    void setInteractiveAuthorizationAllowed(bool enable);
    bool isInteractiveAuthorizationAllowed() const;

    void setArguments(const QList<QVariant> &arguments);
    QList<QVariant> arguments() const;
    QDBusMessage &operator<<(const QVariant &argument);

private:
    friend class QDBusMessagePrivate;
    QSharedDataPointer<QDBusMessagePrivate> d_ptr;
};

Q_DECLARE_METATYPE(QDBusMessage)
