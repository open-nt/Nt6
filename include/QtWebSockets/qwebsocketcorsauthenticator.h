// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

class QWebSocketCorsAuthenticator {
public:
    explicit QWebSocketCorsAuthenticator(const QString &origin);
    QWebSocketCorsAuthenticator(const QWebSocketCorsAuthenticator &other);
    QWebSocketCorsAuthenticator(QWebSocketCorsAuthenticator &&other) noexcept;
    QWebSocketCorsAuthenticator &operator=(const QWebSocketCorsAuthenticator &other);
    QWebSocketCorsAuthenticator &operator=(QWebSocketCorsAuthenticator &&other) noexcept;
    ~QWebSocketCorsAuthenticator();

    void swap(QWebSocketCorsAuthenticator &other) noexcept;
    QString origin() const;
    void setAllowed(bool allowed);
    bool allowed() const;

private:
    QString m_origin;
    bool m_allowed = true;
};
