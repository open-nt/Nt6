// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>

class QDBusMessage;

class QDBusError {
    Q_GADGET
public:
    enum ErrorType {
        NoError = 0,
        Other = 1,
        Failed,
        NoMemory,
        ServiceUnknown,
        NoReply,
        BadAddress,
        NotSupported,
        LimitsExceeded,
        AccessDenied,
        NoServer,
        Timeout,
        NoNetwork,
        AddressInUse,
        Disconnected,
        InvalidArgs,
        UnknownMethod,
        TimedOut,
        InvalidSignature,
        UnknownInterface,
        UnknownObject,
        UnknownProperty,
        PropertyReadOnly,
        InternalError,
        InvalidService,
        InvalidObjectPath,
        InvalidInterface,
        InvalidMember,
        LastErrorType = InvalidMember
    };
    Q_ENUM(ErrorType)

    QDBusError();
    explicit QDBusError(const QDBusMessage &message);
    QDBusError(ErrorType error, const QString &message);

    ErrorType type() const;
    QString name() const;
    QString message() const;
    bool isValid() const;
    void swap(QDBusError &other) noexcept;

    static QString errorString(ErrorType error);

private:
    ErrorType m_type = NoError;
    QString m_name;
    QString m_message;
};

Q_DECLARE_METATYPE(QDBusError)
