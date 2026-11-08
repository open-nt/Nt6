// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>

class QWebSocketHandshakeOptions {
public:
    QWebSocketHandshakeOptions();
    QWebSocketHandshakeOptions(const QWebSocketHandshakeOptions &other);
    QWebSocketHandshakeOptions(QWebSocketHandshakeOptions &&other) noexcept;
    QWebSocketHandshakeOptions &operator=(const QWebSocketHandshakeOptions &other);
    QWebSocketHandshakeOptions &operator=(QWebSocketHandshakeOptions &&other) noexcept;
    ~QWebSocketHandshakeOptions();

    void swap(QWebSocketHandshakeOptions &other) noexcept;
    QStringList subprotocols() const;
    void setSubprotocols(const QStringList &protocols);

    /*!
        Returns \c true if \a lhs and \a rhs request the same subprotocols;
        otherwise returns \c false.
    */
    friend bool operator==(const QWebSocketHandshakeOptions &lhs, const QWebSocketHandshakeOptions &rhs) {
        return lhs.m_subprotocols == rhs.m_subprotocols;
    }
    /*!
        Returns \c true if \a lhs and \a rhs request different subprotocols;
        otherwise returns \c false.
    */
    friend bool operator!=(const QWebSocketHandshakeOptions &lhs, const QWebSocketHandshakeOptions &rhs) {
        return !(lhs == rhs);
    }

private:
    QStringList m_subprotocols;
};
