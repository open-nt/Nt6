// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtQml/qqmlengine.h>

class QQmlApplicationEngine : public QQmlEngine {
    Q_OBJECT
public:
    explicit QQmlApplicationEngine(QObject *parent = nullptr);
    explicit QQmlApplicationEngine(const QUrl &url, QObject *parent = nullptr);
    explicit QQmlApplicationEngine(const QString &filePath, QObject *parent = nullptr);
    QQmlApplicationEngine(const QString &uri, const QString &typeName, QObject *parent = nullptr);
    ~QQmlApplicationEngine() override;

    QList<QObject *> rootObjects() const;

    void setInitialProperties(const QVariantMap &initialProperties);

public slots:
    void load(const QUrl &url);
    void load(const QString &filePath);
    void loadFromModule(const QString &uri, const QString &typeName);
    void setExtraFileSelectors(const QStringList &extraFileSelectors);
    void loadData(const QByteArray &data, const QUrl &url = QUrl());

signals:
    /*!
        This signal is emitted when the root object \a object of \a url has been created, or with a null
        \a object if it could not be.
    */
    void objectCreated(QObject *object, const QUrl &url) {
        emitSignal(&QQmlApplicationEngine::objectCreated, object, url);
    }
    /*!
        This signal is emitted when the root object of \a url could not be created.
    */
    void objectCreationFailed(const QUrl &url) { emitSignal(&QQmlApplicationEngine::objectCreationFailed, url); }

private:
    void loadComponent(class QQmlComponent *component, const QUrl &url);

    QList<QPointer<QObject>> m_roots;
    QVariantMap m_initialProperties;
    QStringList m_selectors;
};
