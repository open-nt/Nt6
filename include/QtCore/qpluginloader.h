// MIT License - see LICENSE
#pragma once

#include <QtCore/qjsonobject.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qplugin.h>
#include <QtCore/qstring.h>

class QPluginLoader : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName)
    Q_PROPERTY(QLibrary::LoadHints loadHints READ loadHints WRITE setLoadHints)
public:
    explicit QPluginLoader(QObject *parent = nullptr);
    explicit QPluginLoader(const QString &fileName, QObject *parent = nullptr);
    ~QPluginLoader() override;

    QObject *instance();
    QJsonObject metaData() const;

    static QObjectList staticInstances();
    static QList<QStaticPlugin> staticPlugins();

    bool load();
    bool unload();
    bool isLoaded() const;

    void setFileName(const QString &fileName);
    QString fileName() const;

    QString errorString() const;

    void setLoadHints(QLibrary::LoadHints loadHints);
    QLibrary::LoadHints loadHints() const;

private:
    bool loadPlugin();

    QLibrary m_library;
    QString m_fileName;
    QString m_error;
    mutable QJsonObject m_metaData;
    mutable bool m_haveMetaData = false;
    bool m_loaded = false;
};
