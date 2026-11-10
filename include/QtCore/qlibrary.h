// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QLibrary : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName)
    Q_PROPERTY(LoadHints loadHints READ loadHints WRITE setLoadHints)
public:
    enum LoadHint {
        ResolveAllSymbolsHint = 0x01,
        ExportExternalSymbolsHint = 0x02,
        LoadArchiveMemberHint = 0x04,
        PreventUnloadHint = 0x08,
        DeepBindHint = 0x10,
    };
    Q_DECLARE_FLAGS(LoadHints, LoadHint)
    Q_ENUM(LoadHint)
    Q_FLAG(LoadHints)

    explicit QLibrary(QObject *parent = nullptr);
    explicit QLibrary(const QString &fileName, QObject *parent = nullptr);
    explicit QLibrary(const QString &fileName, int verNum, QObject *parent = nullptr);
    explicit QLibrary(const QString &fileName, const QString &version, QObject *parent = nullptr);
    ~QLibrary() override;

    QFunctionPointer resolve(const char *symbol);
    static QFunctionPointer resolve(const QString &fileName, const char *symbol);
    static QFunctionPointer resolve(const QString &fileName, int verNum, const char *symbol);
    static QFunctionPointer resolve(const QString &fileName, const QString &version, const char *symbol);

    bool load();
    bool unload();
    bool isLoaded() const;

    static bool isLibrary(const QString &fileName);

    void setFileName(const QString &fileName);
    QString fileName() const;

    void setFileNameAndVersion(const QString &fileName, int verNum);
    void setFileNameAndVersion(const QString &fileName, const QString &version);
    QString errorString() const;

    void setLoadHints(LoadHints hints);
    LoadHints loadHints() const;

private:
    QString m_fileName;
    QString m_version;
    QString m_loadedPath;
    QString m_error;
    LoadHints m_hints;
    bool m_loaded = false;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QLibrary::LoadHints)
