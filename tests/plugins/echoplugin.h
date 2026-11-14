// MIT License - see LICENSE
#pragma once

#include "echointerface.h"

#include <QtCore/qobject.h>

// A plugin that the tests load as a shared module. Its IID is a macro, and its metadata comes from a file.
class EchoPlugin : public QObject, public EchoInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EchoInterface_iid FILE "echoplugin.json")
public:
    QString echo(const QString &message) const override { return QStringLiteral("echo:") + message; }
};
