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
    There is no constructor taking a \c SIGNAL() string or a \c QMetaMethod and
    no \c signal() function. Arguments are stored with
    \c QVariant::fromValue(), so every signal can be spied on as long as its
    arguments can be copied; they need not be registered meta types.
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
