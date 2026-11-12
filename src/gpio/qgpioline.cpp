// MIT License - see LICENSE
#include "qgpiobackend_p.h"

#include <QtCore/qlogging.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>

#include <cerrno>

/*!
    \module NtGpio
    \title Nt GPIO C++ Classes
    \brief Classes for reading and driving general-purpose I/O lines.
    \cmaketarget Nt::Gpio

    Nt GPIO reads switches and drives LEDs, enables and resets through the GPIO lines of the
    board, on Linux and on Zephyr. Qt has no such module; the class follows the conventions of
    the other Nt classes so that it fits into an Nt or Qt program.

    See \l{Using Nt GPIO} for a walk through, and \c{examples/gpio/ptt.cpp} for a complete
    program.
*/

/*!
    \page nt-gpio.html
    \title Using Nt GPIO
    \brief Naming a GPIO line, reading a push-to-talk switch, driving an LED, and getting permission on Linux.

    Link \c Nt::Gpio (or \c Nt6::Gpio from an installed package) and include \c{<QGpioLine>}.
    On Zephyr, also set \c CONFIG_NT_GPIO in \c prj.conf.

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::Gpio)
    \endcode

    Nt GPIO is not part of Qt. A program that must also build with Qt keeps its use of
    QGpioLine behind an interface of its own.

    \section1 Naming a Line

    A QGpioLine is made from the name of one line:

    \table
    \header
        \li Name
        \li Platform
        \li Meaning
    \row
        \li \c PTT
        \li Linux
        \li The line with this name, from \c gpio-line-names in the devicetree. Every chip is
            searched.
    \row
        \li \c gpiochip0:17
        \li Linux
        \li Line 17 of \c /dev/gpiochip0. A full path such as \c /dev/gpiochip0:17 also works.
    \row
        \li \c gpio@401b8000:17
        \li Zephyr
        \li Pin 17 of the GPIO device with this name, the name that \c device_get_binding()
            takes: the devicetree node name, or its \c label.
    \endtable

    \c gpioinfo, from the libgpiod tools, lists the chips and lines of a Linux machine.

    \section1 Reading a Switch

    A new line is an input. Set whether it is active low, its bias and its debounce interval
    before open(). For a push-to-talk switch that pulls the line to ground:

    \snippet gpio/ptt.cpp input

    value() is then the logical value, \c true while the switch is held, and valueChanged() is
    emitted each time it changes:

    \snippet gpio/ptt.cpp connect

    The line is watched for edges where the hardware can, and polled every pollInterval()
    milliseconds where it cannot; isEventDriven() tells which. With a debounce interval, a
    change is reported once the line has been steady for that long, so a switch reports one
    press however much its contacts bounce.

    \section1 Driving a Line

    An output line is opened with the value set by setValue() beforehand, \c false unless
    changed, and changed with setValue():

    \snippet gpio/ptt.cpp output

    \section1 Handling Errors

    open() returns \c false and sets error() and errorString() when the line cannot be had.
    \c BusyError means that another program, or a kernel driver, holds the line. A
    \c ResourceError while the line is open means it went away, for example because a USB GPIO
    adapter was unplugged; the line is closed, and must be opened again:

    \snippet gpio/ptt.cpp errors

    \section1 Permission on Linux

    \c /dev/gpiochip* belongs to \c root, or to the group \c gpio on some distributions. To let a
    user in the group \c plugdev open them, add a udev rule such as this as
    \c /etc/udev/rules.d/60-gpio.rules:

    \code
    SUBSYSTEM=="gpio", KERNEL=="gpiochip*", GROUP="plugdev", MODE="0660"
    \endcode

    \section1 Zephyr

    The line is configured with \c gpio_pin_configure() and watched with an interrupt on both
    edges. The interrupt only wakes the thread that owns the QGpioLine, so its slots run in that
    thread as usual. A pin whose controller cannot interrupt is polled. Names of lines from the
    devicetree are not looked up; use the device name and pin number.
*/

namespace QtPrivate {

void setGpioFailure(GpioFailure &failure, int code, const QString &what) {
    switch (code) {
    case ENOENT:
    case ENODEV:
    case ENXIO:
        failure.error = QGpioLine::DeviceNotFoundError;
        break;
    case EACCES:
    case EPERM:
        failure.error = QGpioLine::PermissionError;
        break;
    case EBUSY:
        failure.error = QGpioLine::BusyError;
        break;
    case ENOTSUP:
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
    case EOPNOTSUPP:
#endif
        failure.error = QGpioLine::UnsupportedOperationError;
        break;
    default:
        failure.error = QGpioLine::UnknownError;
        break;
    }
    failure.message = what + QStringLiteral(": ") + qt_error_string(code);
}

namespace {
GpioBackendFactory g_factory = nullptr;
} // namespace

void setGpioBackendFactory(GpioBackendFactory factory) { g_factory = factory; }

} // namespace QtPrivate

using QtPrivate::GpioFailure;

class QGpioLinePrivate {
public:
    explicit QGpioLinePrivate(QGpioLine *owner) : q(owner) {}

    void setError(QGpioLine::GpioError e, const QString &message) {
        error = e;
        errorString = message;
        if (e != QGpioLine::NoError)
            emit q->errorOccurred(e);
    }
    void fail(const GpioFailure &failure) { setError(failure.error, failure.message); }
    // A failure while the line is open: the line is closed and the failure reported as a
    // ResourceError, unless it says something more precise.
    void lost(GpioFailure failure) {
        if (failure.error == QGpioLine::UnknownError || failure.error == QGpioLine::DeviceNotFoundError)
            failure.error = QGpioLine::ResourceError;
        q->close();
        fail(failure);
    }

    void commit(bool level) {
        if (level == value)
            return;
        value = level;
        emit q->valueChanged(level);
    }

    // Takes in a level the input line was seen at.
    void sample(bool level) {
        if (debounce <= 0) {
            lastLevel = level;
            commit(level);
        } else if (level != lastLevel) {
            lastLevel = level;
            debounceTimer.start(debounce);
        }
    }

    void onEvents() {
        std::vector<bool> levels;
        GpioFailure failure;
        if (!backend->readEvents(levels, failure)) {
            lost(failure);
            return;
        }
        for (const bool level : levels) {
            sample(level);
            if (!backend) // a slot closed the line
                return;
        }
    }

    void poll() {
        bool level = false;
        GpioFailure failure;
        if (!backend->readValue(level, failure)) {
            lost(failure);
            return;
        }
        sample(level);
    }

    void settle() {
        bool level = false;
        GpioFailure failure;
        if (!backend->readValue(level, failure)) {
            lost(failure);
            return;
        }
        lastLevel = level;
        commit(level);
    }

    QGpioLine *q;
    QtPrivate::GpioConfig config;
    int debounce = 0;
    int pollInterval = 10;
    bool value = false;
    bool lastLevel = false;
    std::unique_ptr<QtPrivate::GpioBackend> backend;
    QSocketNotifier *notifier = nullptr; // a child of q, so that close() may run from its own signal
    QTimer debounceTimer;
    QTimer pollTimer;
    QGpioLine::GpioError error = QGpioLine::NoError;
    QString errorString;
};

/*!
    \class QGpioLine
    \inmodule NtGpio
    \brief The QGpioLine class reads or drives one general-purpose I/O line.
    \since 6.0

    A QGpioLine is an input, such as a push-to-talk switch or a headset detect pin, or an
    output, such as an LED or the enable of an amplifier. Give it the name of the line, set it
    up, and call open():

    \code
    QGpioLine ptt(QStringLiteral("gpiochip0:17"));
    ptt.setActiveLow(true);
    ptt.setBias(QGpioLine::PullUp);
    ptt.setDebounceInterval(20);
    if (!ptt.open())
        qWarning() << ptt.errorString();
    QObject::connect(&ptt, &QGpioLine::valueChanged, [](bool pressed) { qDebug() << pressed; });
    \endcode

    Values are logical: on an active-low line, \c true means the line is low. An input line is
    watched from the event loop of the thread the object lives in, by edge where the hardware
    can and by polling where it cannot, and valueChanged() is emitted when the value changes.
    See \l{Using Nt GPIO} for how lines are named on each platform.

    \differences
    Qt has no GPIO class; QGpioLine is an Nt addition. It works on Linux, through the GPIO
    character device (version 2 of its interface, Linux 5.10 or later), and on Zephyr. One object
    handles one line. Direction, bias and active low are applied by open() and need the line
    to be opened again to change; there is no open drain or open source drive, and no event
    timestamps. On Zephyr, line names are not looked up, and quick edges that come closer
    together than the thread can wake are seen as the level the line settled at.
    \enddifferences

    \sa {Using Nt GPIO}
*/

/*!
    \enum QGpioLine::Direction

    This enum describes which way a line works.

    \value Input The line is read. valueChanged() reports its changes.
    \value Output The line is driven with setValue().
*/

/*!
    \enum QGpioLine::Bias

    This enum describes the internal resistor of an input line.

    \value AsIs The bias is left as it was set before, for example by the devicetree.
    \value Disabled No pull resistor. On Zephyr this is the same as \c AsIs.
    \value PullUp The line is pulled high when nothing drives it.
    \value PullDown The line is pulled low when nothing drives it.
*/

/*!
    \enum QGpioLine::GpioError

    This enum describes the errors a QGpioLine can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The chip or the line does not exist, or the name is not valid.
    \value PermissionError The user may not open the GPIO chip.
    \value BusyError Another program or a driver holds the line.
    \value OpenError The line is already open in this object.
    \value UnsupportedOperationError The hardware cannot do what was asked, such as a bias, or
        setValue() was called on an input line.
    \value ResourceError The line went away while it was open, for example because its device
        was removed. The line has been closed.
    \value NotOpenError The operation needs the line to be open.
    \value UnknownError Any other error.
*/

/*!
    Constructs a line object with the given \a parent. Set the line with setLineName() before
    open().
*/
QGpioLine::QGpioLine(QObject *parent) : QObject(parent), d(std::make_unique<QGpioLinePrivate>(this)) {
    d->debounceTimer.setSingleShot(true);
    QObject::connect(&d->debounceTimer, &QTimer::timeout, this, [this] { d->settle(); });
    QObject::connect(&d->pollTimer, &QTimer::timeout, this, [this] { d->poll(); });
}

/*!
    Constructs a line object for the line called \a name, with the given \a parent. The line
    is not opened.

    \sa setLineName()
*/
QGpioLine::QGpioLine(const QString &name, QObject *parent) : QGpioLine(parent) { d->config.name = name; }

/*!
    Destroys the object, closing the line first.
*/
QGpioLine::~QGpioLine() { close(); }

/*!
    Sets the line this object opens to \a name, such as \c PTT or \c gpiochip0:17. It takes
    effect at the next open().

    \sa {Naming a Line}
*/
void QGpioLine::setLineName(const QString &name) { d->config.name = name; }

/*!
    Returns the name of the line.

    \sa setLineName()
*/
QString QGpioLine::lineName() const { return d->config.name; }

/*!
    Sets whether the line is an input or an output to \a direction. It takes effect at the next
    open(); a new line is an \c Input.
*/
void QGpioLine::setDirection(Direction direction) { d->config.direction = direction; }

/*!
    Returns whether the line is an input or an output.

    \sa setDirection()
*/
QGpioLine::Direction QGpioLine::direction() const { return d->config.direction; }

/*!
    Sets whether the line is active low to \a activeLow. On an active-low line, a logical
    \c true is a low level. It takes effect at the next open().

    \sa value()
*/
void QGpioLine::setActiveLow(bool activeLow) { d->config.activeLow = activeLow; }

/*!
    Returns \c true if the line is active low; otherwise returns \c false.

    \sa setActiveLow()
*/
bool QGpioLine::isActiveLow() const { return d->config.activeLow; }

/*!
    Sets the internal pull resistor of the line to \a bias. It takes effect at the next open(),
    which fails with \c UnsupportedOperationError if the hardware has no such bias.
*/
void QGpioLine::setBias(Bias bias) { d->config.bias = bias; }

/*!
    Returns the bias of the line.

    \sa setBias()
*/
QGpioLine::Bias QGpioLine::bias() const { return d->config.bias; }

/*!
    Sets the debounce interval of an input line to \a msecs milliseconds. A change of the line
    is reported once the line has kept its new value for this long. The default, 0, reports
    every change at once. It takes effect at once.

    \sa valueChanged()
*/
void QGpioLine::setDebounceInterval(int msecs) { d->debounce = msecs > 0 ? msecs : 0; }

/*!
    Returns the debounce interval in milliseconds.

    \sa setDebounceInterval()
*/
int QGpioLine::debounceInterval() const { return d->debounce; }

/*!
    Sets how often an input line that cannot report its edges is read to \a msecs
    milliseconds. The default is 10. A line that reports edges is not polled.

    \sa isEventDriven()
*/
void QGpioLine::setPollInterval(int msecs) {
    d->pollInterval = msecs > 0 ? msecs : 1;
    if (d->pollTimer.isActive())
        d->pollTimer.start(d->pollInterval);
}

/*!
    Returns the poll interval in milliseconds.

    \sa setPollInterval()
*/
int QGpioLine::pollInterval() const { return d->pollInterval; }

/*!
    Requests the line from the system and configures it. Returns \c true on success; otherwise
    returns \c false and sets error().

    An input line is read at once, so value() is valid when open() returns. An output line is
    driven to the value set with setValue() beforehand.

    \sa close(), isOpen()
*/
bool QGpioLine::open() {
    if (d->backend) {
        d->setError(OpenError, QStringLiteral("The GPIO line is already open"));
        return false;
    }
    if (d->config.name.isEmpty()) {
        d->setError(DeviceNotFoundError, QStringLiteral("No GPIO line name was set"));
        return false;
    }
    std::unique_ptr<QtPrivate::GpioBackend> backend =
        QtPrivate::g_factory ? QtPrivate::g_factory() : QtPrivate::createPlatformGpioBackend();
    GpioFailure failure;
    d->config.initialValue = d->value;
    if (!backend->open(d->config, failure)) {
        d->fail(failure);
        return false;
    }
    if (d->config.direction == Input) {
        bool level = false;
        if (!backend->readValue(level, failure)) {
            d->fail(failure);
            return false;
        }
        d->backend = std::move(backend);
        d->lastLevel = level;
        d->commit(level);
        if (!d->backend) // a slot closed the line again
            return false;
        const int fd = d->backend->eventDescriptor();
        if (fd >= 0) {
            d->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            QObject::connect(d->notifier, &QSocketNotifier::activated, this, [this] { d->onEvents(); });
        } else {
            d->pollTimer.start(d->pollInterval);
        }
    } else {
        d->backend = std::move(backend);
    }
    d->error = NoError;
    d->errorString.clear();
    return true;
}

/*!
    Releases the line. An output line keeps its last level on Zephyr; on Linux the kernel may
    set it back. Does nothing if the line is not open.

    \sa open()
*/
void QGpioLine::close() {
    d->debounceTimer.stop();
    d->pollTimer.stop();
    if (d->notifier) {
        d->notifier->setEnabled(false);
        d->notifier->deleteLater();
        d->notifier = nullptr;
    }
    d->backend.reset();
}

/*!
    Returns \c true if the line is open; otherwise returns \c false.
*/
bool QGpioLine::isOpen() const { return d->backend != nullptr; }

/*!
    Returns \c true if the open input line reports its edges; returns \c false if it is
    polled, or not an open input.

    \sa setPollInterval()
*/
bool QGpioLine::isEventDriven() const { return d->notifier != nullptr; }

/*!
    Returns the logical value of the line. For an input, this is the value last reported by
    valueChanged(), or read by open(); it is updated from the event loop. For an output, it is
    the value last set.

    \sa setValue(), setActiveLow()
*/
bool QGpioLine::value() const { return d->value; }

/*!
    Drives an output line to the logical \a value. Returns \c true on success; otherwise returns
    \c false and sets error(). On a closed output line, \a value is kept and driven by the next
    open(). An input line cannot be set.

    \sa value(), valueChanged()
*/
bool QGpioLine::setValue(bool value) {
    if (d->config.direction != Output) {
        d->setError(UnsupportedOperationError, QStringLiteral("An input GPIO line cannot be set"));
        return false;
    }
    if (d->backend) {
        GpioFailure failure;
        if (!d->backend->writeValue(value, failure)) {
            d->lost(failure);
            return false;
        }
    }
    d->commit(value);
    return true;
}

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QGpioLine::GpioError QGpioLine::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.

    \sa error()
*/
QString QGpioLine::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QGpioLine::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qgpioline.cpp"
