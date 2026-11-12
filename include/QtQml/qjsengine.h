// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qtqmlglobal.h>

#include <memory>

class QJSEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)
public:
    enum ObjectOwnership { CppOwnership, JavaScriptOwnership };
    Q_ENUM(ObjectOwnership)

    enum Extension {
        TranslationExtension = 0x1,
        ConsoleExtension = 0x2,
        GarbageCollectionExtension = 0x4,
        AllExtensions = 0x7
    };
    Q_ENUM(Extension)
    Q_DECLARE_FLAGS(Extensions, Extension)

    QJSEngine();
    explicit QJSEngine(QObject *parent);
    ~QJSEngine() override;

    QJSValue globalObject() const;
    QJSValue evaluate(const QString &program, const QString &fileName = QString(), int lineNumber = 1,
                      QStringList *exceptionStackTrace = nullptr);
    QJSValue importModule(const QString &fileName);

    QJSValue newObject();
    QJSValue newArray(uint length = 0);
    QJSValue newQObject(QObject *object);
    QJSValue newErrorObject(QJSValue::ErrorType errorType, const QString &message = QString());

    template <typename T> QJSValue toScriptValue(const T &value) { return fromVariant(QVariant::fromValue(value)); }
    template <typename T> T fromScriptValue(const QJSValue &value) {
        return toVariant(value, QMetaType::fromType<T>()).template value<T>();
    }

    void collectGarbage();

    static void setObjectOwnership(QObject *object, ObjectOwnership ownership);
    static ObjectOwnership objectOwnership(QObject *object);

    void installExtensions(Extensions extensions, const QJSValue &object = QJSValue());

    void setInterrupted(bool interrupted);
    bool isInterrupted() const;

    void throwError(const QString &message);
    void throwError(QJSValue::ErrorType errorType, const QString &message = QString());
    void throwError(const QJSValue &error);
    bool hasError() const;
    QJSValue catchError();

    QString uiLanguage() const;
    void setUiLanguage(const QString &language);

signals:
    /*!
        This signal is emitted when the language of the user interface changes.

        \sa setUiLanguage()
    */
    void uiLanguageChanged() { emitSignal(&QJSEngine::uiLanguageChanged); }

private:
    friend class QtJSPrivate::EngineAccess;
    QJSValue fromVariant(const QVariant &value);
    QVariant toVariant(const QJSValue &value, QMetaType type);

    std::unique_ptr<QtJSPrivate::Runtime> m_runtime;
    QString m_uiLanguage;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QJSEngine::Extensions)
