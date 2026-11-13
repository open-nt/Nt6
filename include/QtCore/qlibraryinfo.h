// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qversionnumber.h>

class QLibraryInfo {
public:
    QLibraryInfo() = delete;

    /*!
        \enum QLibraryInfo::LibraryPath
        \since 6.0
        This enum names the directories of the Nt installation that path() returns.

        \value PrefixPath The installation prefix, such as \c{/usr}.
        \value DocumentationPath The documentation.
        \value HeadersPath The headers.
        \value LibrariesPath The libraries.
        \value LibraryExecutablesPath Programs that the libraries run.
        \value BinariesPath The tools, such as \c ntmoc.
        \value PluginsPath Plugins.
        \value QmlImportsPath QML modules.
        \value Qml2ImportsPath Same as \c QmlImportsPath.
        \value ArchDataPath Data that depends on the processor.
        \value DataPath Other data.
        \value TranslationsPath Translations.
        \value ExamplesPath Examples.
        \value TestsPath Installed tests.
        \value SettingsPath The system-wide settings.
    */
    enum LibraryPath {
        PrefixPath = 0,
        DocumentationPath,
        HeadersPath,
        LibrariesPath,
        LibraryExecutablesPath,
        BinariesPath,
        PluginsPath,
        QmlImportsPath,
        Qml2ImportsPath = QmlImportsPath,
        ArchDataPath,
        DataPath,
        TranslationsPath,
        ExamplesPath,
        TestsPath,
        SettingsPath = 100
    };

    static QString path(LibraryPath p);
    static QStringList paths(LibraryPath p);
    static QStringList platformPluginArguments(const QString &platformName);

    static QVersionNumber version() noexcept;
    static const char *build() noexcept;
    static bool isDebugBuild() noexcept;
    static bool isSharedBuild() noexcept;
};
