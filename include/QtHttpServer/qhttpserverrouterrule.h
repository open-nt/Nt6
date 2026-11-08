// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtHttpServer/qhttpserverrequest.h>

#include <memory>

class QHttpServerRouterRule {
public:
    QHttpServerRouterRule(const QHttpServerRouterRule &) = delete;
    QHttpServerRouterRule &operator=(const QHttpServerRouterRule &) = delete;
    ~QHttpServerRouterRule();

    const QObject *contextObject() const;
    QString pathPattern() const;
    QHttpServerRequest::Methods methods() const;

private:
    friend class QHttpServer;
    struct Private;
    explicit QHttpServerRouterRule(std::unique_ptr<Private> p);
    std::unique_ptr<Private> d;
};
