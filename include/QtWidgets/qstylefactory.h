// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QStyle;

class QStyleFactory {
public:
    static QStringList keys();
    static QStyle *create(const QString &key);
};
