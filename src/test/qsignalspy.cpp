// MIT License - see LICENSE
#include <QSignalSpy>
#include <QTest>

/*!
    \class QSignalSpy
    \brief The QSignalSpy class records the emissions of a signal.

    A QSignalSpy is a list with one item for each emission, and each item is
    the list of that emission's arguments:

    \code
    QSignalSpy spy(&counter, &Counter::valueChanged);
    counter.setValue(5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 5);
    \endcode

    \differences
    The arguments of a signal given as a member function pointer are stored with
    \c QVariant::fromValue(), so every such signal can be spied on as long as its
    arguments can be copied; they need not be registered meta types. A signal given
    by name or QMetaMethod is found through the meta-object that ntmoc made, and its
    arguments are stored by their meta types. signal() is empty for a spy made from
    a member function pointer.
    \enddifferences

    \sa QTest::qWaitFor()
*/

/*!
    Returns \c true if the spy is listening to a signal.
*/
bool QSignalSpy::isValid() const noexcept { return m_valid; }

/*!
    Processes events until the signal is emitted or \a timeout milliseconds
    have passed. Returns \c true if it was emitted in that time.
*/
bool QSignalSpy::wait(int timeout) {
    const qsizetype before = size();
    return QTest::qWaitFor([&] { return size() > before; }, timeout);
}

/*!
    \overload

    Waits for the signal for at most \a timeout.
*/
bool QSignalSpy::wait(std::chrono::milliseconds timeout) { return wait(static_cast<int>(timeout.count())); }

void QSignalSpy::warnNullObject() { qWarning("QSignalSpy: cannot spy on a null object"); }

/*!
    \since 6.0

    Constructs a spy that records each emission of the signal \a signature of \a object, given as
    \c SIGNAL() writes it, such as \c{SIGNAL(valueChanged(int))}, or without the macro. The spy is invalid if
    \a object has no such signal.

    \sa signal()
*/
QSignalSpy::QSignalSpy(const QObject *object, const char *signature) {
    if (!object) {
        warnNullObject();
        return;
    }
    if (!signature) {
        qWarning("QSignalSpy: no signal given");
        return;
    }
    const char *name = signature[0] == '2' ? signature + 1 : signature; // the code that SIGNAL() puts in front
    const QByteArray normalized = QMetaObject::normalizedSignature(name);
    const int index = object->metaObject()->indexOfSignal(normalized.constData());
    if (index < 0) {
        qWarning("QSignalSpy: no such signal: '%s'", normalized.constData());
        return;
    }
    listen(object, object->metaObject()->method(index));
}

/*!
    \since 6.0

    Constructs a spy that records each emission of the signal \a method of \a object.
*/
QSignalSpy::QSignalSpy(const QObject *object, const QMetaMethod &method) {
    if (!object) {
        warnNullObject();
        return;
    }
    if (!method.isValid() || method.methodType() != QMetaMethod::Signal) {
        qWarning("QSignalSpy: not a signal");
        return;
    }
    listen(object, method);
}

/*!
    \fn QByteArray QSignalSpy::signal() const
    \since 6.0

    Returns the normalized signature of the signal that the spy was given by name or meta-method, or an empty array
    for one given as a member function pointer.
*/

void QSignalSpy::listen(const QObject *object, const QMetaMethod &method) {
    m_signature = method.methodSignature();
    std::vector<QMetaType> types;
    for (int i = 0; i < method.parameterCount(); ++i)
        types.push_back(method.parameterMetaType(i));
    m_valid =
        static_cast<bool>(QtPrivate::Access::connectFunction(object, method, this, [this, types](void *const *argv) {
            QList<QVariant> arguments;
            for (std::size_t i = 0; i < types.size(); ++i)
                arguments.append(types[i].isValid() ? QVariant(types[i], argv[i]) : QVariant());
            append(arguments);
        }));
}
