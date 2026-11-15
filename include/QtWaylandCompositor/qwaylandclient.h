// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

struct wl_client;

class QWaylandCompositor;

namespace QtWaylandPrivate {
class Connection;
}

class QWaylandClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWaylandCompositor *compositor READ compositor CONSTANT)
public:
    enum TextInputProtocol { NoProtocol = 0, QtTextInputMethodV1 = 1, TextInputV2 = 2, TextInputV4 = 4 };
    Q_DECLARE_FLAGS(TextInputProtocols, TextInputProtocol)

    ~QWaylandClient() override;

    static QWaylandClient *fromWlClient(QWaylandCompositor *compositor, wl_client *wlClient);

    /*!
        Returns the compositor the client is connected to.
    */
    QWaylandCompositor *compositor() const { return m_compositor; }
    wl_client *client() const;

    qint64 processId() const;
    qint64 userId() const;
    qint64 groupId() const;

    /*!
        Returns the text-input protocols the client uses; always NoProtocol in Nt.
    */
    TextInputProtocols textInputProtocols() const { return NoProtocol; }

    Q_INVOKABLE void kill(int signal = 15);

public slots:
    void close();

private:
    friend class QWaylandCompositor;
    QWaylandClient(QWaylandCompositor *compositor, QtWaylandPrivate::Connection *connection);

    QWaylandCompositor *m_compositor;
    QtWaylandPrivate::Connection *m_connection;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QWaylandClient::TextInputProtocols)
