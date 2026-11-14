// MIT License - see LICENSE
#pragma once

#include "echointerface.h"

#include <QtCore/qfactoryinterface.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

namespace NtTest {

// A plugin that the tests link statically, built with QT_STATICPLUGIN, in a namespace and without a file.
class StaticEchoPlugin : public QObject, public EchoInterface, public QFactoryInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.nt.test.EchoInterface")
public:
    QString echo(const QString &message) const override { return QStringLiteral("static:") + message; }
    QStringList keys() const override { return {QStringLiteral("static")}; }
};

} // namespace NtTest
