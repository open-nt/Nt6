// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtQml/qtqmlglobal.h>

class QQmlEngine;

class QQmlContext : public QObject {
    Q_OBJECT
public:
    /*!
        \class QQmlContext::PropertyPair
        \brief A name and a value, for setContextProperties().

        \differences
        None.
        \enddifferences
    */
    struct PropertyPair {
        /*!
            The name of the property.
        */
        QString name;
        /*!
            The value of the property.
        */
        QVariant value;
    };

    explicit QQmlContext(QQmlEngine *engine, QObject *parent = nullptr);
    explicit QQmlContext(QQmlContext *parentContext, QObject *parent = nullptr);
    ~QQmlContext() override;

    bool isValid() const;
    QQmlEngine *engine() const;
    QQmlContext *parentContext() const;

    QObject *contextObject() const;
    void setContextObject(QObject *object);

    QVariant contextProperty(const QString &name) const;
    void setContextProperty(const QString &name, QObject *value);
    void setContextProperty(const QString &name, const QVariant &value);
    void setContextProperties(const QList<PropertyPair> &properties);

    QString nameForObject(const QObject *object) const;
    QObject *objectForName(const QString &name) const;

    QUrl resolvedUrl(const QUrl &url) const;
    void setBaseUrl(const QUrl &baseUrl);
    QUrl baseUrl() const;

private:
    friend class QtQmlPrivate::Creator;
    friend class QtQmlPrivate::EngineData;

    QQmlEngine *m_engine = nullptr;
    QQmlContext *m_parent = nullptr;
    QPointer<QObject> m_parentGuard; // m_parent while it exists
    QPointer<QObject> m_contextObject;
    QHash<QString, QVariant> m_properties;
    QHash<QString, QPointer<QObject>> m_ids;
    QUrl m_baseUrl;
    void *m_scope = nullptr; // the scope object of the context in the JavaScript runtime
};
