// MIT License - see LICENSE
#pragma once

#include <QPermission>
#include <QStringList>

#include <functional>

class QObject;

namespace QtPrivate {

// The Android permissions (android.permission.*) that \a permission stands for on a device of API level
// \a sdkVersion; the permission is granted when all of them are. Empty for an invalid permission. Compiled on every
// platform so that the mapping can be tested off Android.
QStringList androidPermissionNames(const QPermission &permission, int sdkVersion);

#ifdef __ANDROID__
// The status of \a permission on the device: Granted when Android has granted every one of its permissions, Denied
// when the user refused them since the application started, Undetermined otherwise.
Qt::PermissionStatus androidCheckPermission(const QPermission &permission);

// Asks Android for \a permission and calls \a answer with the result, in the thread of \a target (or of the
// application object when \a target is null), unless \a target is destroyed first.
void androidRequestPermission(const QPermission &permission, QObject *target,
                              std::function<void(Qt::PermissionStatus)> answer);
#endif

} // namespace QtPrivate
