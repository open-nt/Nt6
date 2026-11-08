// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

class QHttpServerWebSocketUpgradeResponse {
public:
    enum class ResponseType { Accept, Deny, PassToNext };

    QHttpServerWebSocketUpgradeResponse(const QHttpServerWebSocketUpgradeResponse &other);
    QHttpServerWebSocketUpgradeResponse(QHttpServerWebSocketUpgradeResponse &&other) noexcept;
    QHttpServerWebSocketUpgradeResponse &operator=(const QHttpServerWebSocketUpgradeResponse &other);
    QHttpServerWebSocketUpgradeResponse &operator=(QHttpServerWebSocketUpgradeResponse &&other) noexcept;
    ~QHttpServerWebSocketUpgradeResponse();

    void swap(QHttpServerWebSocketUpgradeResponse &other) noexcept;
    ResponseType type() const;
    int denyStatus() const;
    QByteArray denyMessage() const;

    static QHttpServerWebSocketUpgradeResponse accept();
    static QHttpServerWebSocketUpgradeResponse deny();
    static QHttpServerWebSocketUpgradeResponse deny(int status, QByteArray message);
    static QHttpServerWebSocketUpgradeResponse passToNext();

private:
    QHttpServerWebSocketUpgradeResponse(ResponseType type, int status, QByteArray message);

    ResponseType m_type;
    int m_status;
    QByteArray m_message;
};
