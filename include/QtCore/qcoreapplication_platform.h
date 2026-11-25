// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#ifdef __ANDROID__
#include <QtCore/qfuture.h>
#include <QtCore/qvariant.h>

#include <jni.h>

#include <functional>

namespace QNativeInterface {

// What an application can ask of Android, as in Qt.
struct QAndroidApplication {
    // The activity, which is the Context of the application; a global reference that stays valid.
    static jobject context();
    static bool isActivityContext();
    // The API level of the device.
    static int sdkVersion();
    // Runs \a runnable on the UI thread of Android, which is where views, toasts and dialogs must be used. The future
    // is ready when it has run; waiting for it on the UI thread itself would never end, so there it runs at once.
    static QFuture<QVariant> runOnAndroidMainThread(const std::function<QVariant()> &runnable);
    static QFuture<void> runOnAndroidMainThread(const std::function<void()> &runnable);
};

} // namespace QNativeInterface
#endif // __ANDROID__
