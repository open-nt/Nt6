// MIT License - see LICENSE
#include <QtQml/qjsengine.h>

#include "qjsbuiltins_p.h"
#include "qjsengine_p.h"

#include <QtCore/qalgorithms.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qelapsedtimer.h>

using QtJSPrivate::EngineAccess;
using QtJSPrivate::Runtime;
using QtJSPrivate::Value;

/*!
    \module NtQml
    \title Nt Qml C++ Classes
    \brief A JavaScript engine for scripting applications, and the QML engine.
    \cmaketarget Nt::Qml

    Nt Qml runs JavaScript and QML. QJSEngine evaluates programs, QJSValue holds what they make, and
    QObjects take part in scripts with their properties, slots and signals; see \l{Using Nt Qml}.
    QQmlEngine and QQmlComponent make objects from QML documents, with bindings that keep their properties
    up to date; see \l{Using Nt QML}.

    Link with \c Nt::Qml and include \c{<QJSEngine>}, \c{<QQmlEngine>} or \c{<QtQml>}.
*/

/*!
    \class QJSEngine
    \brief The QJSEngine class runs JavaScript programs.

    An engine has a global object, and evaluate() runs programs in it:

    \snippet qml/scripting.cpp evaluate

    Values go both ways as QJSValue. A QObject passed to newQObject() is seen by scripts with its
    properties, its slots and Q_INVOKABLE methods, and its signals, which scripts connect to functions:

    \snippet qml/scripting.cpp qobject

    A script that throws makes evaluate() return the error; QJSValue::isError() tells. A C++ method that
    scripts call can throw a JavaScript error with throwError().

    The language is ECMAScript 2017 without classes, generators, async functions, destructuring, symbols,
    Proxy and typed arrays; see \l{Using Nt Qml} for the details.

    Source code is untrusted input: nesting, recursion and the size of arrays and strings are limited, so
    a script can fail but not crash the application. setInterrupted() stops a script that runs too long.

    \differences
    There is no \c importModule() (it returns an error), \c newSymbol(), \c newQMetaObject(),
    \c registerModule() or \c QJSManagedValue. JavaScript is interpreted, not compiled. Garbage is
    collected when no script runs. toScriptValue() and fromScriptValue() go through QVariant.
    \enddifferences

    \sa QJSValue, {Using Nt Qml}
*/

/*!
    \enum QJSEngine::ObjectOwnership

    Who deletes a QObject that scripts can see.

    \value CppOwnership C++ code does; the engine never does.
    \value JavaScriptOwnership The engine deletes it when no script can reach it any more, unless it has a
           parent.
*/

/*!
    \enum QJSEngine::Extension

    Optional parts of the global object.

    \value TranslationExtension \c qsTr(), \c qsTranslate() and the \c QT_TR_NOOP() family. Nt returns the
           text untranslated.
    \value ConsoleExtension \c console.log() and its siblings, and \c print().
    \value GarbageCollectionExtension \c gc().
    \value AllExtensions All of them.
*/

/*!
    \typedef QJSEngine::Extensions

    A QFlags of Extension values.
*/

/*!
    Constructs an engine with an empty global object, apart from the standard library.
*/
QJSEngine::QJSEngine() : QJSEngine(nullptr) {}

/*!
    Constructs an engine with the given \a parent.
*/
QJSEngine::QJSEngine(QObject *parent) : QObject(parent), m_runtime(std::make_unique<Runtime>(this)) {
    m_runtime->connectionContext = this;
    m_runtime->reportError = [this](const Value &error) {
        Runtime *runtime = m_runtime.get();
        QString file, line;
        if (error.isObject()) {
            file = runtime->toString(runtime->get(error, QStringLiteral("fileName")));
            line = runtime->toString(runtime->get(error, QStringLiteral("lineNumber")));
        }
        QString message;
        try {
            message = runtime->toString(error);
        } catch (const QtJSPrivate::Throw &) {
            message = QStringLiteral("(an error that cannot be shown)");
        }
        qWarning().noquote() << QStringLiteral("%1:%2: %3").arg(file, line, message);
    };
}

/*!
    Destroys the engine. QJSValues of the engine become undefined, and objects that scripts own are deleted.
*/
QJSEngine::~QJSEngine() {
    Runtime *runtime = m_runtime.get();
    for (QJSValuePrivate *h : std::as_const(runtime->handles)) {
        h->rt = nullptr;
        h->value = Value();
    }
    runtime->handles.clear();
    for (const Runtime::ScriptConnection &c : runtime->connections)
        QObject::disconnect(c.connection);
    runtime->connections.clear();
    QList<QObject *> owned;
    for (auto it = runtime->scriptOwned.cbegin(); it != runtime->scriptOwned.cend(); ++it)
        if (it.value() && !it.key()->parent() && runtime->wrappers.contains(it.key()))
            owned.append(it.key());
    runtime->scriptOwned.clear();
    runtime->wrappers.clear();
    qDeleteAll(owned);
}

/*!
    Returns the global object, where scripts find the standard library and the names they declare.
*/
QJSValue QJSEngine::globalObject() const {
    return EngineAccess::make(m_runtime.get(), Value::object(m_runtime->global));
}

/*!
    Runs \a program, which comes from \a fileName at \a lineNumber, and returns the value of its last
    statement. If the program throws, returns the error, and if \a exceptionStackTrace is not \c nullptr,
    stores the functions that were running, innermost first, as \c{name@file:line}.

    \snippet qml/scripting.cpp evaluate
*/
QJSValue QJSEngine::evaluate(const QString &program, const QString &fileName, int lineNumber,
                             QStringList *exceptionStackTrace) {
    Runtime *runtime = m_runtime.get();
    if (exceptionStackTrace)
        exceptionStackTrace->clear();
    try {
        const Value v = runtime->evaluate(program, fileName, lineNumber);
        const QJSValue r = EngineAccess::make(runtime, v);
        runtime->maybeCollect();
        return r;
    } catch (const QtJSPrivate::Throw &t) {
        if (exceptionStackTrace && t.value.isObject()) {
            const Value stack = runtime->getFromObject(t.value.asObject(), QStringLiteral("stack"), t.value);
            if (stack.isString())
                *exceptionStackTrace = stack.asString().split(u'\n', Qt::SkipEmptyParts);
        }
        const QJSValue r = EngineAccess::make(runtime, t.value);
        runtime->maybeCollect();
        return r;
    }
}

/*!
    Returns an error: modules are not supported. \a fileName is not used.
*/
QJSValue QJSEngine::importModule(const QString &fileName) {
    (void)fileName;
    return newErrorObject(QJSValue::GenericError, QStringLiteral("modules are not supported"));
}

/*!
    Returns a new, empty object.
*/
QJSValue QJSEngine::newObject() { return EngineAccess::make(m_runtime.get(), Value::object(m_runtime->newObject())); }

/*!
    Returns a new array of \a length undefined elements.
*/
QJSValue QJSEngine::newArray(uint length) {
    return EngineAccess::make(
        m_runtime.get(), Value::object(m_runtime->newArray(QtJSPrivate::Values(std::min<uint>(length, 10000000u)))));
}

/*!
    Returns \a object as scripts see it: its properties, slots, invokable methods and signals. The same
    object always gives the same value. If \a object has no parent and its ownership was not set with
    setObjectOwnership(), the engine owns it and deletes it when no script can reach it.
*/
QJSValue QJSEngine::newQObject(QObject *object) {
    Runtime *runtime = m_runtime.get();
    if (!object)
        return QJSValue(QJSValue::NullValue);
    const QVariant explicitOwnership = object->property("_q_ntJSOwnership");
    const bool scriptOwned =
        explicitOwnership.isValid() ? explicitOwnership.toInt() == JavaScriptOwnership : !object->parent();
    const QJSValue v = EngineAccess::make(runtime, runtime->wrapQObject(object));
    if (scriptOwned)
        runtime->scriptOwned.insert(object, true);
    return v;
}

/*!
    Returns a new error of the given \a errorType with \a message.
*/
QJSValue QJSEngine::newErrorObject(QJSValue::ErrorType errorType, const QString &message) {
    const int index = errorType == QJSValue::NoError || errorType == QJSValue::GenericError ? 0 : errorType - 1;
    return EngineAccess::make(m_runtime.get(),
                              Value::object(m_runtime->newError(static_cast<QtJSPrivate::ErrorType>(index), message)));
}

/*!
    \fn template <typename T> QJSValue QJSEngine::toScriptValue(const T &value)

    Returns \a value as a script value, converted through QVariant.
*/

/*!
    \fn template <typename T> T QJSEngine::fromScriptValue(const QJSValue &value)

    Returns \a value converted to \c T through QVariant.
*/

QJSValue QJSEngine::fromVariant(const QVariant &value) {
    return EngineAccess::make(m_runtime.get(), m_runtime->fromVariant(value));
}

QVariant QJSEngine::toVariant(const QJSValue &value, QMetaType type) {
    Runtime *runtime = m_runtime.get();
    try {
        return runtime->toVariant(EngineAccess::value(runtime, value), type);
    } catch (const QtJSPrivate::Throw &) {
        return QVariant();
    }
}

/*!
    Frees the objects that no script and no QJSValue can reach. It does nothing while a script runs.
*/
void QJSEngine::collectGarbage() { m_runtime->collect(); }

/*!
    Sets who deletes \a object to \a ownership, for the engines that see it.
*/
void QJSEngine::setObjectOwnership(QObject *object, ObjectOwnership ownership) {
    if (object)
        object->setProperty("_q_ntJSOwnership", static_cast<int>(ownership));
}

/*!
    Returns who deletes \a object: the ownership set with setObjectOwnership(), or JavaScriptOwnership for
    an object without a parent and CppOwnership for one with a parent.
*/
QJSEngine::ObjectOwnership QJSEngine::objectOwnership(QObject *object) {
    if (!object)
        return CppOwnership;
    const QVariant v = object->property("_q_ntJSOwnership");
    if (v.isValid())
        return static_cast<ObjectOwnership>(v.toInt());
    return object->parent() ? CppOwnership : JavaScriptOwnership;
}

/*!
    Adds the \a extensions to \a object, or to the global object if \a object is undefined.
*/
void QJSEngine::installExtensions(Extensions extensions, const QJSValue &object) {
    Runtime *runtime = m_runtime.get();
    const Value target = object.isUndefined() ? Value::object(runtime->global) : EngineAccess::value(runtime, object);
    if (!target.isObject())
        return;
    QtJSPrivate::Object *o = target.asObject();
    using QtJSPrivate::addMethod;
    using QtJSPrivate::Values;
    if (extensions & ConsoleExtension) {
        QtJSPrivate::Object *console = runtime->newObject();
        o->setOwn(QStringLiteral("console"), Value::object(console), false);
        const auto join = [](Runtime &rt, const Values &args) {
            QStringList parts;
            for (const Value &v : args) {
                if (v.isObject() && !v.asFunction() && v.asObject()->cls != QtJSPrivate::Object::Class::Error &&
                    v.asObject()->cls != QtJSPrivate::Object::Class::Date) {
                    // Objects and arrays are shown as JSON, which says more than "[object Object]".
                    QString json;
                    try {
                        json = QtJSPrivate::jsonStringify(rt, v, QString());
                    } catch (const QtJSPrivate::Throw &) {
                        json.clear(); // cyclic or failing toJSON(): fall back to the string form
                    }
                    if (!json.isEmpty()) {
                        parts.append(json);
                        continue;
                    }
                }
                parts.append(rt.toString(v));
            }
            return parts.join(u' ');
        };
        const auto level = [&](const char *name, QtMsgType type) {
            addMethod(*runtime, console, QLatin1String(name), 0,
                      [join, type](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                          const QString text = join(rt, args);
                          switch (type) {
                          case QtDebugMsg:
                              qDebug().noquote() << text;
                              break;
                          case QtInfoMsg:
                              qInfo().noquote() << text;
                              break;
                          case QtWarningMsg:
                              qWarning().noquote() << text;
                              break;
                          default:
                              qCritical().noquote() << text;
                              break;
                          }
                          return Value();
                      });
        };
        level("log", QtDebugMsg);
        level("debug", QtDebugMsg);
        level("info", QtInfoMsg);
        level("warn", QtWarningMsg);
        level("error", QtCriticalMsg);
        addMethod(*runtime, console, QStringLiteral("assert"), 1,
                  [join](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                      if (!args.empty() && rt.toBoolean(args[0]))
                          return Value();
                      const Values rest(args.size() > 1 ? args.begin() + 1 : args.end(), args.end());
                      qCritical().noquote() << QStringLiteral("Assertion failed: ") + join(rt, rest);
                      return Value();
                  });
        auto timers = std::make_shared<QHash<QString, QElapsedTimer>>();
        auto counts = std::make_shared<QHash<QString, int>>();
        addMethod(*runtime, console, QStringLiteral("time"), 1,
                  [timers](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                      const QString label = args.empty() ? QStringLiteral("default") : rt.toString(args[0]);
                      (*timers)[label].start();
                      return Value();
                  });
        addMethod(*runtime, console, QStringLiteral("timeEnd"), 1,
                  [timers](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                      const QString label = args.empty() ? QStringLiteral("default") : rt.toString(args[0]);
                      if (timers->contains(label))
                          qDebug().noquote()
                              << QStringLiteral("%1: %2ms").arg(label).arg(timers->take(label).elapsed());
                      return Value();
                  });
        addMethod(*runtime, console, QStringLiteral("count"), 1,
                  [counts](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                      const QString label = args.empty() ? QStringLiteral("default") : rt.toString(args[0]);
                      qDebug().noquote() << QStringLiteral("%1: %2").arg(label).arg(++(*counts)[label]);
                      return Value();
                  });
        addMethod(*runtime, console, QStringLiteral("trace"), 0,
                  [](Runtime &rt, const Value &, const Values &, QtJSPrivate::Object *) {
                      qDebug().noquote() << rt.stack.join(u'\n');
                      return Value();
                  });
        o->setOwn(QStringLiteral("print"),
                  runtime->getFromObject(console, QStringLiteral("log"), Value::object(console)), false);
    }
    if (extensions & GarbageCollectionExtension) {
        addMethod(*runtime, o, QStringLiteral("gc"), 0,
                  [](Runtime &, const Value &, const Values &, QtJSPrivate::Object *) {
                      // Collection waits until no script runs.
                      return Value();
                  });
    }
    if (extensions & TranslationExtension) {
        const auto identity = [](std::size_t textIndex) {
            return [textIndex](Runtime &rt, const Value &, const Values &args, QtJSPrivate::Object *) {
                return Value::string(textIndex < args.size() ? rt.toString(args[textIndex]) : QString());
            };
        };
        addMethod(*runtime, o, QStringLiteral("qsTr"), 1, identity(0));
        addMethod(*runtime, o, QStringLiteral("qsTrId"), 1, identity(0));
        addMethod(*runtime, o, QStringLiteral("qsTranslate"), 2, identity(1));
        addMethod(*runtime, o, QStringLiteral("QT_TR_NOOP"), 1, identity(0));
        addMethod(*runtime, o, QStringLiteral("QT_TRID_NOOP"), 1, identity(0));
        addMethod(*runtime, o, QStringLiteral("QT_TRANSLATE_NOOP"), 2, identity(1));
    }
}

/*!
    Stops running scripts if \a interrupted is \c true: the next statement throws an error. While it is set,
    no script runs. Call it from another thread to stop a script that runs too long.
*/
void QJSEngine::setInterrupted(bool interrupted) { m_runtime->interrupted = interrupted; }

/*!
    Returns \c true if scripts are interrupted; otherwise returns \c false.
*/
bool QJSEngine::isInterrupted() const { return m_runtime->interrupted; }

/*!
    Throws an Error with \a message in the script that called the running C++ method.
*/
void QJSEngine::throwError(const QString &message) { throwError(QJSValue::GenericError, message); }

/*!
    \overload

    Throws an error of the given \a errorType with \a message.
*/
void QJSEngine::throwError(QJSValue::ErrorType errorType, const QString &message) {
    throwError(newErrorObject(errorType, message));
}

/*!
    \overload

    Throws \a error, which may be any value.
*/
void QJSEngine::throwError(const QJSValue &error) {
    m_runtime->hasPendingError = true;
    m_runtime->pendingError = EngineAccess::value(m_runtime.get(), error);
}

/*!
    Returns \c true if throwError() was called and the error was not yet thrown or caught; otherwise returns
    \c false.
*/
bool QJSEngine::hasError() const { return m_runtime->hasPendingError; }

/*!
    Returns the error that throwError() set and forgets it, or undefined if there is none.
*/
QJSValue QJSEngine::catchError() {
    if (!m_runtime->hasPendingError)
        return QJSValue();
    m_runtime->hasPendingError = false;
    const QJSValue e = EngineAccess::make(m_runtime.get(), m_runtime->pendingError);
    m_runtime->pendingError = Value();
    return e;
}

/*!
    Returns the language of the user interface, empty by default.
*/
QString QJSEngine::uiLanguage() const { return m_uiLanguage; }

/*!
    Sets the language of the user interface to \a language. Nt keeps it for scripts and QML.
*/
void QJSEngine::setUiLanguage(const QString &language) {
    if (language == m_uiLanguage)
        return;
    m_uiLanguage = language;
    emit uiLanguageChanged();
}

#include "moc_qjsengine.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
