// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qurl.h>

class QObject;

class QDesktopServices {
public:
    static bool openUrl(const QUrl &url);
    static void setUrlHandler(const QString &scheme, QObject *receiver, const char *method);
    static void unsetUrlHandler(const QString &scheme);
};
