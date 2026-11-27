// MIT License - see LICENSE
#include <QtCore/qpermissions.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>

#include <utility>

#include "qpermissions_p.h"

/*!
    \class QPermission
    \since 6.5
    \brief The QPermission class holds a permission that the application may need, such as access to the camera
    or to the location of the device.

    A QPermission is made from one of the permission classes, QCameraPermission, QMicrophonePermission,
    QBluetoothPermission, QLocationPermission, QContactsPermission or QCalendarPermission, and passed to
    QCoreApplication::checkPermission() and QCoreApplication::requestPermission():

    \code
    QCameraPermission camera;
    switch (qApp->checkPermission(camera)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(camera, this, &Recorder::start);
        return;
    case Qt::PermissionStatus::Denied:
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
    \endcode

    \differences
    The platforms that Nt supports have no permission prompts, so every valid permission is granted. The
    permission is held in a shared, immutable copy rather than in a QVariant.
    \enddifferences

    \sa QCoreApplication::checkPermission(), QCoreApplication::requestPermission()
*/

/*!
    \fn QPermission::QPermission()
    Constructs an invalid permission, of no type. Checking it returns \c Qt::PermissionStatus::Denied.
*/

/*!
    \fn template <typename T, std::enable_if_t<QtPrivate::IsPermission<T>::value, bool>> QPermission::QPermission(const T &permission)
    Constructs a permission that holds a copy of \a permission, which is one of the permission classes, such as
    QCameraPermission.

    \sa value(), type()
*/

/*!
    \fn Qt::PermissionStatus QPermission::status() const
    Returns the status of the permission. It is \c Qt::PermissionStatus::Undetermined, except in the permission
    that QCoreApplication::requestPermission() passes to its callback, which holds the answer.
*/

/*!
    \fn QMetaType QPermission::type() const
    Returns the type of the permission class that the permission holds, such as \c QCameraPermission, or an
    invalid QMetaType if it holds none.

    \sa value()
*/

/*!
    \fn template <typename T, std::enable_if_t<QtPrivate::IsPermission<T>::value, bool>> std::optional<T> QPermission::value() const
    Returns a copy of the permission of the class \c T that this permission holds, or \c std::nullopt if it
    holds another class or none.

    \code
    if (auto location = permission.value<QLocationPermission>())
        qDebug() << location->accuracy();
    \endcode

    \sa type()
*/

// Writes the type and the status of the permission.
QDebug operator<<(QDebug debug, const QPermission &permission) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "QPermission(" << (permission.m_type.isValid() ? permission.m_type.name() : "invalid") << ", ";
    switch (permission.m_status) {
    case Qt::PermissionStatus::Undetermined:
        debug << "Undetermined";
        break;
    case Qt::PermissionStatus::Granted:
        debug << "Granted";
        break;
    case Qt::PermissionStatus::Denied:
        debug << "Denied";
        break;
    }
    debug << ')';
    return debug;
}

/*!
    \class QLocationPermission
    \since 6.5
    \brief The QLocationPermission class describes access to the location of the device.

    Set how precise the location must be with setAccuracy(), and whether the application needs it in the
    background with setAvailability(). Like every permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \enum QLocationPermission::Accuracy
    This enum says how precise the location must be.

    \value Approximate A location to within a few kilometers.
    \value Precise The most precise location that the device can tell.
*/

/*!
    \enum QLocationPermission::Availability
    This enum says when the application uses the location.

    \value WhenInUse Only while the application is in use.
    \value Always Also while the application runs in the background.
*/

/*!
    \fn void QLocationPermission::setAccuracy(QLocationPermission::Accuracy accuracy)
    Sets the accuracy that the application needs to \a accuracy. The default is \c Approximate.

    \sa accuracy()
*/

/*!
    \fn QLocationPermission::Accuracy QLocationPermission::accuracy() const
    Returns the accuracy that the application needs.

    \sa setAccuracy()
*/

/*!
    \fn void QLocationPermission::setAvailability(QLocationPermission::Availability availability)
    Sets when the application uses the location to \a availability. The default is \c WhenInUse.

    \sa availability()
*/

/*!
    \fn QLocationPermission::Availability QLocationPermission::availability() const
    Returns when the application uses the location.

    \sa setAvailability()
*/

/*!
    \class QCalendarPermission
    \since 6.5
    \brief The QCalendarPermission class describes access to the user's calendar.

    Set whether the application only reads the calendar or changes it too with setAccessMode(). Like every
    permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \enum QCalendarPermission::AccessMode
    This enum says what the application does with the calendar.

    \value ReadOnly It only reads it.
    \value ReadWrite It reads and changes it.
*/

/*!
    \fn void QCalendarPermission::setAccessMode(QCalendarPermission::AccessMode mode)
    Sets the access that the application needs to \a mode. The default is \c ReadOnly.

    \sa accessMode()
*/

/*!
    \fn QCalendarPermission::AccessMode QCalendarPermission::accessMode() const
    Returns the access that the application needs.

    \sa setAccessMode()
*/

/*!
    \class QContactsPermission
    \since 6.5
    \brief The QContactsPermission class describes access to the user's contacts.

    Set whether the application only reads the contacts or changes them too with setAccessMode(). Like every
    permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \enum QContactsPermission::AccessMode
    This enum says what the application does with the contacts.

    \value ReadOnly It only reads them.
    \value ReadWrite It reads and changes them.
*/

/*!
    \fn void QContactsPermission::setAccessMode(QContactsPermission::AccessMode mode)
    Sets the access that the application needs to \a mode. The default is \c ReadOnly.

    \sa accessMode()
*/

/*!
    \fn QContactsPermission::AccessMode QContactsPermission::accessMode() const
    Returns the access that the application needs.

    \sa setAccessMode()
*/

/*!
    \class QCameraPermission
    \since 6.5
    \brief The QCameraPermission class describes access to the camera.

    It has no settings. Like every permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \class QMicrophonePermission
    \since 6.5
    \brief The QMicrophonePermission class describes access to the microphone.

    It has no settings. Like every permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \class QBluetoothPermission
    \since 6.5
    \brief The QBluetoothPermission class describes access to Bluetooth devices.

    Set whether the application connects to devices, advertises itself, or both with setCommunicationModes().
    Like every permission class, it can be copied, moved and swapped.

    \differences
    The permission is always granted, since the platforms that Nt supports have no permission prompts.
    \enddifferences

    \sa QPermission
*/

/*!
    \enum QBluetoothPermission::CommunicationMode
    This enum says what the application does over Bluetooth.

    \value Access It finds and connects to other devices.
    \value Advertise It makes the device known to others.
    \value Default Both.
*/

/*!
    \typedef QBluetoothPermission::CommunicationModes
    A QFlags of CommunicationMode values.
*/

/*!
    \fn void QBluetoothPermission::setCommunicationModes(QBluetoothPermission::CommunicationModes modes)
    Sets what the application does over Bluetooth to \a modes. Empty modes stand for \c Default, which is also
    the initial value.

    \sa communicationModes()
*/

/*!
    \fn QBluetoothPermission::CommunicationModes QBluetoothPermission::communicationModes() const
    Returns what the application does over Bluetooth.

    \sa setCommunicationModes()
*/

/*!
    \since 6.5
    Returns whether the application may use the feature that \a permission describes. On Android this is
    \c Qt::PermissionStatus::Granted when the system has granted every Android permission the feature needs,
    \c Qt::PermissionStatus::Denied when the user refused them since the application started, and
    \c Qt::PermissionStatus::Undetermined otherwise, so the application should ask with requestPermission(). The
    application's manifest must declare those permissions (\c nt_android_add_apk() \c PERMISSIONS). The other
    platforms that Nt supports have no permission prompts, and there this returns \c Qt::PermissionStatus::Granted.
    An invalid, default-constructed QPermission is always \c Qt::PermissionStatus::Denied.

    \sa requestPermission()
*/
Qt::PermissionStatus QCoreApplication::checkPermission(const QPermission &permission) {
    if (!permission.type().isValid())
        return Qt::PermissionStatus::Denied;
#ifdef __ANDROID__
    return QtPrivate::androidCheckPermission(permission);
#else
    return Qt::PermissionStatus::Granted;
#endif
}

/*!
    \fn template <typename Functor> void QCoreApplication::requestPermission(const QPermission &permission, const typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context, Functor &&func)
    \since 6.5
    Asks the user for \a permission and calls \a func with a copy of it whose QPermission::status() holds the
    answer. \a func is a function, a functor or a member function of \a context, and is called from the event
    loop of the thread that \a context lives in; it is not called if \a context is destroyed first.

    \code
    qApp->requestPermission(QMicrophonePermission(), this, [this](const QPermission &permission) {
        if (permission.status() == Qt::PermissionStatus::Granted)
            startRecording();
    });
    \endcode

    On Android the system asks the user, unless the permission is already granted, and \a func is called with the
    answer; a location permission that is wanted \c Always is asked for in two steps from Android 11 on, as the
    system requires (first while in use, then all the time). An application whose activity is a plain NativeActivity
    (no JDK when it was packaged) cannot ask, and gets \c Qt::PermissionStatus::Denied. Elsewhere there are no
    permission prompts: the answer is the one of checkPermission(), and it comes at once from the event loop.

    \sa checkPermission()
*/

/*!
    \fn template <typename Functor> void QCoreApplication::requestPermission(const QPermission &permission, Functor &&func)
    \since 6.5
    \overload
    Asks the user for \a permission and calls \a func with a copy of it whose QPermission::status() holds the
    answer, from the event loop of the thread of the application object.
*/

void QCoreApplication::requestPermissionImpl(const QPermission &permission, const QObject *context,
                                             std::function<void(const QPermission &)> callback) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): the context only chooses the thread
    QObject *target = context ? const_cast<QObject *>(context) : this;
#ifdef __ANDROID__
    if (permission.type().isValid()) {
        QtPrivate::androidRequestPermission(permission, target,
                                            [permission, callback = std::move(callback)](Qt::PermissionStatus status) {
                                                QPermission answered = permission;
                                                answered.m_status = status;
                                                callback(answered);
                                            });
        return;
    }
#endif
    QPermission answered = permission;
    answered.m_status = checkPermission(permission);
    QMetaObject::invokeMethod(
        target, [answered = std::move(answered), callback = std::move(callback)] { callback(answered); },
        Qt::QueuedConnection);
}

namespace QtPrivate {

QStringList androidPermissionNames(const QPermission &permission, int sdkVersion) {
    QStringList names;
    const auto add = [&names](const char *name) {
        names.append(QStringLiteral("android.permission.") + QLatin1String(name));
    };
    if (permission.value<QCameraPermission>()) {
        add("CAMERA");
    } else if (permission.value<QMicrophonePermission>()) {
        add("RECORD_AUDIO");
    } else if (const auto location = permission.value<QLocationPermission>()) {
        // Precise location needs the approximate one too: from Android 12 the user may grant only that.
        if (location->accuracy() == QLocationPermission::Precise)
            add("ACCESS_FINE_LOCATION");
        add("ACCESS_COARSE_LOCATION");
        if (location->availability() == QLocationPermission::Always && sdkVersion >= 29)
            add("ACCESS_BACKGROUND_LOCATION");
    } else if (const auto contacts = permission.value<QContactsPermission>()) {
        add("READ_CONTACTS");
        if (contacts->accessMode() == QContactsPermission::ReadWrite)
            add("WRITE_CONTACTS");
    } else if (const auto calendar = permission.value<QCalendarPermission>()) {
        add("READ_CALENDAR");
        if (calendar->accessMode() == QCalendarPermission::ReadWrite)
            add("WRITE_CALENDAR");
    } else if (const auto bluetooth = permission.value<QBluetoothPermission>()) {
        if (sdkVersion >= 31) {
            if (bluetooth->communicationModes().testFlag(QBluetoothPermission::Access)) {
                add("BLUETOOTH_SCAN");
                add("BLUETOOTH_CONNECT");
            }
            if (bluetooth->communicationModes().testFlag(QBluetoothPermission::Advertise))
                add("BLUETOOTH_ADVERTISE");
        } else {
            // Before Android 12 scanning for devices needs the location permission.
            add("BLUETOOTH");
            add("BLUETOOTH_ADMIN");
            add("ACCESS_FINE_LOCATION");
        }
    }
    return names;
}

} // namespace QtPrivate

#include "moc_qpermissions.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
