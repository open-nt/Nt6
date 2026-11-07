// MIT License - see LICENSE
#include <QSignalSpy>
#include <QTest>

/*!
    \class QSignalSpyArgument
    \brief The QSignalSpyArgument class holds one argument of a signal recorded by QSignalSpy.

    It stands in for QVariant, which Nt does not have yet. Code like
    \c{spy.at(0).at(0).toInt()} and \c{spy.at(0).at(0).value<MyType>()} reads
    the same as with Qt.

    \differences
    This class does not exist in Qt, where the arguments are \c QVariant
    objects. Write \c{auto arguments = spy.takeFirst();} rather than naming
    \c{QList<QVariant>}. Only numbers and enums convert to one another.
    \enddifferences

    \sa QSignalSpy
*/

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
    The arguments are QSignalSpyArgument objects, not \c QVariant. There is no
    constructor taking a \c SIGNAL() string or a \c QMetaMethod, no
    \c signal() function, and no check that the arguments are registered meta
    types: every signal can be spied on, as long as its arguments can be copied.
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
