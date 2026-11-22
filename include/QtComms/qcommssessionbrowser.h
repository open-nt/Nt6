// MIT License - see LICENSE
#pragma once

#include "qcommssession.h"

#include <QtCore/qobject.h>

#include <memory>

namespace QtComms {

class SessionBrowserPrivate;

class SessionBrowser : public QObject {
    Q_OBJECT

public:
    explicit SessionBrowser(QObject *parent = nullptr);
    ~SessionBrowser() override;

    void setConfiguration(const SessionConfiguration &configuration);
    SessionConfiguration configuration() const;

    bool start();
    void stop();
    bool isActive() const;
    void refresh();
    QList<SessionInfo> sessions() const;
    SessionInfo find(const QString &nameOrId) const;
    QString errorString() const;

Q_SIGNALS:
    void sessionFound(const QtComms::SessionInfo &session);
    void sessionUpdated(const QtComms::SessionInfo &session);
    void sessionLost(quint32 id);

private:
    std::unique_ptr<SessionBrowserPrivate> d;
};

} // namespace QtComms
