// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlerror.h>
#include <QtQml/qtqmlglobal.h>

#include <memory>

class QQmlContext;
class QQmlEngine;

class QQmlComponent : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl url READ url CONSTANT)
public:
    enum CompilationMode { PreferSynchronous, Asynchronous };
    Q_ENUM(CompilationMode)
    enum Status { Null, Ready, Loading, Error };
    Q_ENUM(Status)

    explicit QQmlComponent(QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *engine, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *engine, const QString &fileName, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *engine, const QString &fileName, CompilationMode mode, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *engine, const QUrl &url, QObject *parent = nullptr);
    QQmlComponent(QQmlEngine *engine, const QUrl &url, CompilationMode mode, QObject *parent = nullptr);
    ~QQmlComponent() override;

    Status status() const;
    bool isNull() const;
    bool isReady() const;
    bool isError() const;
    bool isLoading() const;
    QList<QQmlError> errors() const;
    QString errorString() const;
    qreal progress() const;
    QUrl url() const;

    virtual QObject *create(QQmlContext *context = nullptr);
    QObject *createWithInitialProperties(const QVariantMap &initialProperties, QQmlContext *context = nullptr);
    void setInitialProperties(QObject *component, const QVariantMap &properties);
    virtual QObject *beginCreate(QQmlContext *context);
    virtual void completeCreate();

    QQmlContext *creationContext() const;
    QQmlEngine *engine() const;

    Q_INVOKABLE QObject *createObject(QObject *parent = nullptr, const QVariantMap &properties = QVariantMap());

public slots:
    void loadUrl(const QUrl &url);
    void loadUrl(const QUrl &url, CompilationMode mode);
    void loadFromModule(const QString &uri, const QString &typeName, CompilationMode mode = PreferSynchronous);
    void setData(const QByteArray &data, const QUrl &baseUrl);

signals:
    /*!
        This signal is emitted when the status of the component changes to \a status.
    */
    void statusChanged(QQmlComponent::Status status) { emitSignal(&QQmlComponent::statusChanged, status); }
    /*!
        This signal is emitted when the loading progress changes to \a progress.
    */
    void progressChanged(qreal progress) { emitSignal(&QQmlComponent::progressChanged, progress); }

private:
    friend class QtQmlPrivate::Creator;
    friend class QtQmlPrivate::EngineData;
    void setStatus(Status status);

    QQmlEngine *m_engine = nullptr;
    QUrl m_url;
    Status m_status = Null;
    QList<QQmlError> m_errors;
    std::shared_ptr<const QtJSPrivate::ProgramData> m_program;
    const QtJSPrivate::QmlObjectNode *m_root = nullptr; // set for components declared in a document
    QPointer<QQmlContext> m_creationContext;
    QVariantMap m_initialProperties;
    std::unique_ptr<QtQmlPrivate::Creator> m_pending;
};
