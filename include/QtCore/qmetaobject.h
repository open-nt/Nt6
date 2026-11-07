// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qvariant.h>

#include <type_traits>

/*!
    \class QMetaMethod
    \brief The QMetaMethod class provides meta-data about a member function.

    A QMetaMethod describes a signal, a slot, an invokable method or a
    constructor that \c ntmoc found in a class. Get one from
    QMetaObject::method() or QMetaObject::indexOfMethod(), or for a signal
    from fromSignal().

    \code
    const QMetaObject *mo = &QTimer::staticMetaObject;
    QMetaMethod m = mo->method(mo->indexOfSlot("start(int)"));
    qDebug() << m.methodSignature() << m.parameterCount();
    m.invoke(&timer, Q_ARG(int, 100));
    \endcode

    \differences
    There is no \c attributes(), and \c invokeOnGadget() is not available.
    Argument types are compared by name, as in Qt, so the types of a queued
    call must be registered with the meta-type system. A
    \c Qt::BlockingQueuedConnection call to an object in the calling thread
    runs the method directly, where Qt reports a deadlock.
    \enddifferences

    \sa QMetaObject, QMetaProperty, QMetaEnum
*/
class QMetaMethod {
public:
    /*!
        \enum QMetaMethod::Access

        This enum describes the access level of a method.

        \value Private The method is private.
        \value Protected The method is protected.
        \value Public The method is public.
    */
    enum Access { Private, Protected, Public };

    /*!
        \enum QMetaMethod::MethodType

        This enum describes the kind of a method.

        \value Method A function that is declared with \c Q_INVOKABLE.
        \value Signal A signal.
        \value Slot A slot.
        \value Constructor A constructor that is declared with \c Q_INVOKABLE.
    */
    enum MethodType { Method, Signal, Slot, Constructor };

    /*!
        Constructs an invalid method.

        \sa isValid()
    */
    QMetaMethod() = default;

    QByteArray methodSignature() const;
    QByteArray name() const;
    const char *typeName() const;
    int returnType() const;
    QMetaType returnMetaType() const;
    int parameterCount() const;
    int parameterType(int param) const;
    QMetaType parameterMetaType(int param) const;
    void getParameterTypes(int *types) const;
    QList<QByteArray> parameterTypes() const;
    QList<QByteArray> parameterNames() const;
    const char *tag() const;
    Access access() const;
    MethodType methodType() const;
    int methodIndex() const;
    int relativeMethodIndex() const;
    int revision() const;
    const QMetaObject *enclosingMetaObject() const;
    /*!
        Returns \c true if this object describes a method; otherwise returns \c false.
    */
    bool isValid() const { return mobj != nullptr; }

    bool invoke(QObject *object, Qt::ConnectionType connectionType, QGenericReturnArgument returnValue,
                QGenericArgument val0 = QGenericArgument(nullptr), QGenericArgument val1 = QGenericArgument(),
                QGenericArgument val2 = QGenericArgument(), QGenericArgument val3 = QGenericArgument(),
                QGenericArgument val4 = QGenericArgument(), QGenericArgument val5 = QGenericArgument(),
                QGenericArgument val6 = QGenericArgument(), QGenericArgument val7 = QGenericArgument(),
                QGenericArgument val8 = QGenericArgument(), QGenericArgument val9 = QGenericArgument()) const;
    /*!
        \overload
        Invokes the method on \a object with \c Qt::AutoConnection, storing the result in
        \a returnValue.
    */
    bool invoke(QObject *object, QGenericReturnArgument returnValue, QGenericArgument val0 = QGenericArgument(nullptr),
                QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                QGenericArgument val9 = QGenericArgument()) const {
        return invoke(object, Qt::AutoConnection, returnValue, val0, val1, val2, val3, val4, val5, val6, val7, val8,
                      val9);
    }
    /*!
        \overload
        Invokes the method on \a object with the connection type \a connectionType, discarding
        the result.
    */
    bool invoke(QObject *object, Qt::ConnectionType connectionType, QGenericArgument val0 = QGenericArgument(nullptr),
                QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                QGenericArgument val9 = QGenericArgument()) const {
        return invoke(object, connectionType, QGenericReturnArgument(), val0, val1, val2, val3, val4, val5, val6, val7,
                      val8, val9);
    }
    /*!
        \overload
        Invokes the method on \a object with \c Qt::AutoConnection, discarding the result.
    */
    bool invoke(QObject *object, QGenericArgument val0 = QGenericArgument(nullptr),
                QGenericArgument val1 = QGenericArgument(), QGenericArgument val2 = QGenericArgument(),
                QGenericArgument val3 = QGenericArgument(), QGenericArgument val4 = QGenericArgument(),
                QGenericArgument val5 = QGenericArgument(), QGenericArgument val6 = QGenericArgument(),
                QGenericArgument val7 = QGenericArgument(), QGenericArgument val8 = QGenericArgument(),
                QGenericArgument val9 = QGenericArgument()) const {
        return invoke(object, Qt::AutoConnection, QGenericReturnArgument(), val0, val1, val2, val3, val4, val5, val6,
                      val7, val8, val9);
    }

    QObject *newInstance(QGenericArgument val0 = QGenericArgument(nullptr), QGenericArgument val1 = QGenericArgument(),
                         QGenericArgument val2 = QGenericArgument(), QGenericArgument val3 = QGenericArgument(),
                         QGenericArgument val4 = QGenericArgument(), QGenericArgument val5 = QGenericArgument(),
                         QGenericArgument val6 = QGenericArgument(), QGenericArgument val7 = QGenericArgument(),
                         QGenericArgument val8 = QGenericArgument(), QGenericArgument val9 = QGenericArgument()) const;

    /*!
        Returns the meta-method of the signal \a signal, a pointer to a signal of a class that
        \c ntmoc processed. Returns an invalid method if the pointer does not name a signal.

        \sa QMetaObject::indexOfSignal()
    */
    template <typename PointerToMemberFunction> static QMetaMethod fromSignal(PointerToMemberFunction signal) {
        using Class = typename QtPrivate::MemberClass<PointerToMemberFunction>::type;
        return fromSignalImpl(&Class::staticMetaObject, QtPrivate::SignalId::of(signal));
    }

    /*!
        Returns \c true if \a a and \a b describe the same method.
    */
    friend bool operator==(const QMetaMethod &a, const QMetaMethod &b) {
        return a.mobj == b.mobj && a.index == b.index && a.ctor == b.ctor;
    }
    /*!
        Returns \c true if \a a and \a b describe different methods.
    */
    friend bool operator!=(const QMetaMethod &a, const QMetaMethod &b) { return !(a == b); }

private:
    friend struct QMetaObject;
    friend class QMetaProperty;
    friend class QObject;

    static QMetaMethod fromSignalImpl(const QMetaObject *mo, const QtPrivate::SignalId &id);
    const QtPrivate::MetaMethodData &data() const;

    const QMetaObject *mobj = nullptr;
    int index = -1; // local to mobj
    bool ctor = false;
};

/*!
    \class QMetaEnum
    \brief The QMetaEnum class provides meta-data about an enumerator.

    Use it to convert between the values of an enumeration declared with
    \c Q_ENUM() or \c Q_FLAG() and their names:

    \code
    QMetaEnum e = QMetaEnum::fromType<Shape::Kind>();
    e.valueToKey(Shape::Circle);   // "Circle"
    e.keyToValue("Square");        // Shape::Square
    \endcode

    \differences
    Values are \c int; there is no \c value64() or \c is64Bit(). There is no
    \c QMetaEnum::fromType() for an enumeration that is not declared with
    \c Q_ENUM(), \c Q_FLAG(), \c Q_ENUM_NS() or \c Q_FLAG_NS().
    \enddifferences

    \sa QMetaObject::enumerator(), QMetaProperty::enumerator()
*/
class QMetaEnum {
public:
    /*!
        Constructs an invalid enumerator.

        \sa isValid()
    */
    QMetaEnum() = default;

    const char *name() const;
    const char *enumName() const;
    const char *scope() const;
    bool isFlag() const;
    bool isScoped() const;
    int keyCount() const;
    const char *key(int i) const;
    int value(int i) const;
    int keyToValue(const char *key, bool *ok = nullptr) const;
    const char *valueToKey(int value) const;
    int keysToValue(const char *keys, bool *ok = nullptr) const;
    QByteArray valueToKeys(int value) const;
    const QMetaObject *enclosingMetaObject() const;
    /*!
        Returns \c true if this object describes an enumerator; otherwise returns \c false.
    */
    bool isValid() const { return mobj != nullptr; }

    /*!
        Returns the meta-data of the enumeration \c T, which must be declared with
        \c Q_ENUM() or \c Q_FLAG() (or the \c _NS forms). Returns an invalid object otherwise.
    */
    template <typename T> static QMetaEnum fromType() {
        const QMetaObject *mo = qt_getEnumMetaObject(T());
        if (!mo)
            return QMetaEnum();
        return mo->enumerator(mo->indexOfEnumerator(qt_getEnumName(T())));
    }

private:
    friend struct QMetaObject;
    const QtPrivate::MetaEnumData &data() const;

    const QMetaObject *mobj = nullptr;
    int index = -1;
};

/*!
    \class QMetaProperty
    \brief The QMetaProperty class provides meta-data about a property.

    A property is declared with \c Q_PROPERTY(). Read and write it through a
    QVariant, with an object or, for a \c Q_GADGET, a pointer to the value:

    \code
    QMetaProperty p = obj->metaObject()->property(
        obj->metaObject()->indexOfProperty("interval"));
    p.write(obj, 500);
    int ms = p.read(obj).toInt();
    \endcode

    Enumerations declared with \c Q_ENUM() can be written from their key
    names as well as from numbers. Writing converts the variant to the type of
    the property first, and fails if it cannot be converted.

    \differences
    Bindable properties are not supported, so \c isBindable() is always
    \c false and there is no \c bindable() or \c isQProperty(). The
    \c DESIGNABLE, \c SCRIPTABLE, \c STORED and \c USER attributes are
    evaluated, but \c EDITABLE and \c isEditable() are not available. Property
    types that are not registered with QMetaType cannot be read or written.
    \enddifferences

    \sa QMetaObject::property(), QObject::property(), QMetaEnum
*/
class QMetaProperty {
public:
    /*!
        Constructs an invalid property.

        \sa isValid()
    */
    QMetaProperty() = default;

    const char *name() const;
    const char *typeName() const;
    QMetaType metaType() const;
    int typeId() const;
    int userType() const { return typeId(); }
    int propertyIndex() const;
    int relativePropertyIndex() const;
    bool isReadable() const;
    bool isWritable() const;
    bool isResettable() const;
    bool isDesignable(const QObject *obj = nullptr) const;
    bool isScriptable(const QObject *obj = nullptr) const;
    bool isStored(const QObject *obj = nullptr) const;
    bool isUser(const QObject *obj = nullptr) const;
    bool isConstant() const;
    bool isFinal() const;
    bool isRequired() const;
    bool isFlagType() const;
    bool isEnumType() const;
    bool isBindable() const;
    QMetaEnum enumerator() const;
    bool hasNotifySignal() const;
    QMetaMethod notifySignal() const;
    int notifySignalIndex() const;
    int revision() const;
    const QMetaObject *enclosingMetaObject() const;
    /*!
        Returns \c true if this object describes a property; otherwise returns \c false.
    */
    bool isValid() const { return mobj != nullptr; }

    QVariant read(const QObject *obj) const;
    bool write(QObject *obj, const QVariant &value) const;
    bool reset(QObject *obj) const;
    QVariant readOnGadget(const void *gadget) const;
    bool writeOnGadget(void *gadget, const QVariant &value) const;
    bool resetOnGadget(void *gadget) const;
    bool hasStdCppSet() const;

private:
    friend struct QMetaObject;
    const QtPrivate::MetaPropertyData &data() const;
    QVariant readImpl(QObject *object) const;
    bool writeImpl(QObject *object, const QVariant &value) const;
    bool queryFlag(unsigned shift, const QObject *obj, QMetaObject::Call call, bool defaultValue) const;

    const QMetaObject *mobj = nullptr;
    int index = -1;
};

/*!
    \class QMetaClassInfo
    \brief The QMetaClassInfo class provides additional information about a class.

    Class information is a name and value pair that a class declares with
    \c Q_CLASSINFO():

    \code
    class Document : public QObject {
        Q_OBJECT
        Q_CLASSINFO("Author", "A. Writer")
    };
    \endcode

    \differences
    None.
    \enddifferences

    \sa QMetaObject::classInfo()
*/
class QMetaClassInfo {
public:
    /*!
        Constructs an invalid object.

        \sa isValid()
    */
    QMetaClassInfo() = default;

    const char *name() const;
    const char *value() const;
    const QMetaObject *enclosingMetaObject() const;
    /*!
        Returns \c true if this object describes class information; otherwise returns \c false.
    */
    bool isValid() const { return mobj != nullptr; }

private:
    friend struct QMetaObject;
    const QtPrivate::MetaClassInfoData &data() const;

    const QMetaObject *mobj = nullptr;
    int index = -1;
};
