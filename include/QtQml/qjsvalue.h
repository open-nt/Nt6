// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtQml/qtqmlglobal.h>

#include <memory>

class QDateTime;
class QJSEngine;
class QJSValue;
class QObject;
struct QJSValuePrivate;

using QJSValueList = QList<QJSValue>;

class QJSValue {
public:
    enum SpecialValue { NullValue, UndefinedValue };
    enum ErrorType { NoError, GenericError, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError };
    enum ObjectConversionBehavior { ConvertJSObjects, RetainJSObjects };

    QJSValue(SpecialValue value = UndefinedValue);
    QJSValue(bool value);
    QJSValue(int value);
    QJSValue(uint value);
    QJSValue(double value);
    QJSValue(const QString &value);
    QJSValue(QLatin1String value);
    QJSValue(const char *str);
    QJSValue(const QJSValue &other);
    QJSValue(QJSValue &&other) noexcept;
    QJSValue &operator=(const QJSValue &other);
    QJSValue &operator=(QJSValue &&other) noexcept;
    ~QJSValue();

    void swap(QJSValue &other) noexcept { d.swap(other.d); }

    bool isBool() const;
    bool isNumber() const;
    bool isNull() const;
    bool isString() const;
    bool isUndefined() const;
    bool isVariant() const;
    bool isQObject() const;
    bool isObject() const;
    bool isDate() const;
    bool isRegExp() const;
    bool isArray() const;
    bool isError() const;
    bool isUrl() const;
    bool isCallable() const;

    QString toString() const;
    double toNumber() const;
    qint32 toInt() const;
    quint32 toUInt() const;
    bool toBool() const;
    QVariant toVariant() const;
    QVariant toVariant(ObjectConversionBehavior behavior) const;
    QObject *toQObject() const;
    QDateTime toDateTime() const;

    bool equals(const QJSValue &other) const;
    bool strictlyEquals(const QJSValue &other) const;

    QJSValue prototype() const;
    void setPrototype(const QJSValue &prototype);

    QJSValue property(const QString &name) const;
    void setProperty(const QString &name, const QJSValue &value);
    bool hasProperty(const QString &name) const;
    bool hasOwnProperty(const QString &name) const;
    QJSValue property(quint32 arrayIndex) const;
    void setProperty(quint32 arrayIndex, const QJSValue &value);
    bool deleteProperty(const QString &name);

    QJSValue call(const QJSValueList &args = QJSValueList()) const;
    QJSValue callWithInstance(const QJSValue &instance, const QJSValueList &args = QJSValueList()) const;
    QJSValue callAsConstructor(const QJSValueList &args = QJSValueList()) const;

    ErrorType errorType() const;

private:
    friend class QtJSPrivate::EngineAccess;
    explicit QJSValue(std::shared_ptr<QJSValuePrivate> data);
    std::shared_ptr<QJSValuePrivate> d;
};

Q_DECLARE_METATYPE(QJSValue)
