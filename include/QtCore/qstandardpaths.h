// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QStandardPaths {
public:
    enum StandardLocation {
        DesktopLocation,
        DocumentsLocation,
        FontsLocation,
        ApplicationsLocation,
        MusicLocation,
        MoviesLocation,
        PicturesLocation,
        TempLocation,
        HomeLocation,
        AppLocalDataLocation,
        CacheLocation,
        GenericDataLocation,
        RuntimeLocation,
        ConfigLocation,
        DownloadLocation,
        GenericCacheLocation,
        GenericConfigLocation,
        AppDataLocation,
        AppConfigLocation,
        PublicShareLocation,
        TemplatesLocation,
        StateLocation,
        GenericStateLocation
    };
    enum LocateOption { LocateFile = 0x0, LocateDirectory = 0x1 };
    Q_DECLARE_FLAGS(LocateOptions, LocateOption)

    static QString writableLocation(StandardLocation type);
    static QStringList standardLocations(StandardLocation type);
    static QString locate(StandardLocation type, const QString &fileName, LocateOptions options = LocateFile);
    static QStringList locateAll(StandardLocation type, const QString &fileName, LocateOptions options = LocateFile);
    static QString displayName(StandardLocation type);
    static QString findExecutable(const QString &executableName, const QStringList &paths = QStringList());
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QStandardPaths::LocateOptions)
