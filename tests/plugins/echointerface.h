// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

// The interface of the test plugins.
class EchoInterface {
public:
    virtual ~EchoInterface() = default;
    virtual QString echo(const QString &message) const = 0;
};

#define EchoInterface_iid "org.nt.test.EchoInterface"
Q_DECLARE_INTERFACE(EchoInterface, EchoInterface_iid)
