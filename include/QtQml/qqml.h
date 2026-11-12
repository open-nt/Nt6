// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtQml/qqmllist.h>
#include <QtQml/qqmlparserstatus.h>

#include <functional>

class QJSEngine;
class QQmlContext;
class QQmlEngine;

// Declarative registration in Qt needs its build tools; Nt registers types with qmlRegisterType() instead,
// and keeps these macros so that the class declarations compile.
#define QML_ELEMENT
#define QML_NAMED_ELEMENT(NAME)
#define QML_ANONYMOUS
#define QML_UNCREATABLE(REASON)
#define QML_SINGLETON
#define QML_VALUE_TYPE(NAME)
#define QML_ATTACHED(ATTACHED_TYPE)
#define QML_ADDED_IN_VERSION(MAJOR, MINOR)
#define QML_REMOVED_IN_VERSION(MAJOR, MINOR)
#define QML_EXTENDED(EXTENDED_TYPE)
#define QML_FOREIGN(FOREIGN_TYPE)
#define QML_INTERFACE
#define QML_IMPLEMENTS_INTERFACES(INTERFACES)

namespace QtQmlPrivate {
int registerType(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QMetaObject *mo,
                 std::function<QObject *()> create, const QString &noCreationReason);
int registerSingleton(const char *uri, int versionMajor, int versionMinor, const char *qmlName, const QMetaObject *mo,
                      std::function<QObject *(QQmlEngine *, QJSEngine *)> create, QObject *instance);
} // namespace QtQmlPrivate

template <typename T> int qmlRegisterType(const char *uri, int versionMajor, int versionMinor, const char *qmlName) {
    return QtQmlPrivate::registerType(
        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject, [] { return static_cast<QObject *>(new T); },
        QString());
}

template <typename T> int qmlRegisterAnonymousType(const char *uri, int versionMajor) {
    return QtQmlPrivate::registerType(uri, versionMajor, 0, nullptr, &T::staticMetaObject, nullptr,
                                      QStringLiteral("anonymous type"));
}

template <typename T>
int qmlRegisterUncreatableType(const char *uri, int versionMajor, int versionMinor, const char *qmlName,
                               const QString &reason) {
    return QtQmlPrivate::registerType(uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject, nullptr,
                                      reason.isEmpty() ? QStringLiteral("cannot be created") : reason);
}

template <typename T>
int qmlRegisterSingletonType(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                             std::function<QObject *(QQmlEngine *, QJSEngine *)> callback) {
    return QtQmlPrivate::registerSingleton(uri, versionMajor, versionMinor, typeName, &T::staticMetaObject,
                                           std::move(callback), nullptr);
}

int qmlRegisterSingletonInstance(const char *uri, int versionMajor, int versionMinor, const char *typeName,
                                 QObject *cppObject);
int qmlRegisterType(const QUrl &url, const char *uri, int versionMajor, int versionMinor, const char *qmlName);
int qmlRegisterSingletonType(const QUrl &url, const char *uri, int versionMajor, int versionMinor, const char *qmlName);
void qmlRegisterModule(const char *uri, int versionMajor, int versionMinor);

QQmlEngine *qmlEngine(const QObject *object);
QQmlContext *qmlContext(const QObject *object);
