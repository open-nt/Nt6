// MIT License - see LICENSE
#pragma once

#include <QtDBus/qdbusconnection.h>
#include <QtDBus/qdbuscontext.h>
#include <QtDBus/qdbusmessage.h>

class QObject;

struct QDBusContextPrivate {
    QDBusConnection connection;
    QDBusMessage message;

    // Makes `context` the context of `object`, if it is a QDBusContext, and returns the previous.
    static QDBusContextPrivate *set(QObject *object, QDBusContextPrivate *context);
};
