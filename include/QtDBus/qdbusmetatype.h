// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtDBus/qdbusargument.h>

class QDBusMetaType {
public:
    /*!
        The type of a function that writes the value at the given address to a QDBusArgument.
    */
    using MarshallFunction = void (*)(QDBusArgument &, const void *);
    /*!
        The type of a function that reads a value from a QDBusArgument to the given address.
    */
    using DemarshallFunction = void (*)(const QDBusArgument &, void *);

    static void registerMarshallOperators(QMetaType metaType, MarshallFunction marshall, DemarshallFunction demarshall);
    static bool marshall(QDBusArgument &argument, QMetaType metaType, const void *data);
    static bool demarshall(const QDBusArgument &argument, QMetaType metaType, void *data);
    static QMetaType signatureToMetaType(const char *signature);
    static const char *typeToSignature(QMetaType metaType);
};

// Registers T with the D-Bus type system and returns its meta-type. T must have
// operator<<(QDBusArgument &, const T &) and operator>>(const QDBusArgument &, T &),
// and be declared with Q_DECLARE_METATYPE().
template <typename T> QMetaType qDBusRegisterMetaType() {
    const QMetaType type = QMetaType::fromType<T>();
    (void)type.id();
    QDBusMetaType::registerMarshallOperators(
        type, [](QDBusArgument &arg, const void *t) { arg << *static_cast<const T *>(t); },
        [](const QDBusArgument &arg, void *t) { arg >> *static_cast<T *>(t); });
    return type;
}
