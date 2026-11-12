// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QQmlParserStatus {
public:
    virtual ~QQmlParserStatus();
    virtual void classBegin() = 0;
    virtual void componentComplete() = 0;
};

#define QQmlParserStatus_iid "org.qt-project.Qt.QQmlParserStatus"
