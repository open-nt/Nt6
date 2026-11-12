// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtQml/qjsengine.h>
#include <QtQml/qqmlerror.h>

#include <memory>

class QQmlContext;
class QQmlComponent;

class QQmlEngine : public QJSEngine {
    Q_OBJECT
    Q_PROPERTY(QString offlineStoragePath READ offlineStoragePath WRITE setOfflineStoragePath)
public:
    explicit QQmlEngine(QObject *parent = nullptr);
    ~QQmlEngine() override;

    QQmlContext *rootContext() const;

    void clearComponentCache();
    void trimComponentCache();

    QStringList importPathList() const;
    void setImportPathList(const QStringList &paths);
    void addImportPath(const QString &dir);

    QUrl baseUrl() const;
    void setBaseUrl(const QUrl &url);
    QUrl interceptUrl(const QUrl &url, int type) const;

    bool outputWarningsToStandardError() const;
    void setOutputWarningsToStandardError(bool enabled);

    QString offlineStoragePath() const;
    void setOfflineStoragePath(const QString &dir);

    template <typename T> T singletonInstance(const QString &moduleName, const QString &typeName) {
        return qobject_cast<T>(singleton(moduleName, typeName));
    }

    static QQmlContext *contextForObject(const QObject *object);
    static void setContextForObject(QObject *object, QQmlContext *context);

public slots:
    void retranslate();

signals:
    /*!
        This signal is emitted when QML code calls \c{Qt.quit()}.
    */
    void quit() { emitSignal(&QQmlEngine::quit); }
    /*!
        This signal is emitted when QML code calls \c{Qt.exit()} with \a retCode.
    */
    void exit(int retCode) { emitSignal(&QQmlEngine::exit, retCode); }
    /*!
        This signal is emitted with \a warnings when QML code makes errors that are not fatal.
    */
    void warnings(const QList<QQmlError> &warnings) { emitSignal(&QQmlEngine::warnings, warnings); }

private:
    friend class QtQmlPrivate::EngineData;
    friend class QtQmlPrivate::Creator;
    QObject *singleton(const QString &moduleName, const QString &typeName);

    std::unique_ptr<QtQmlPrivate::EngineData> m_data;
};
