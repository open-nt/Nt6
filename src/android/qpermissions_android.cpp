// MIT License - see LICENSE
//
// Android permissions for QCoreApplication::checkPermission() and requestPermission(): checked with
// Context.checkSelfPermission(), asked for with Activity.requestPermissions() through org.nt.NtActivity, whose
// onRequestPermissionsResult() hands the answer back to nativePermissionResult() below.
#include "../qpermissions_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qhash.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qpointer.h>
#include <QtCore/qset.h>

#include <jni.h>

#include <utility>

namespace {

constexpr jint kPermissionGranted = 0; // PackageManager.PERMISSION_GRANTED
constexpr const char *kActivityClass = "org/nt/NtActivity";
constexpr const char *kBackgroundLocation = "android.permission.ACCESS_BACKGROUND_LOCATION";

struct PendingRequest {
    QPointer<QObject> target;
    std::function<void(Qt::PermissionStatus)> answer;
    QStringList later; // asked for once the first names are granted (background location from Android 11)
};

struct PermissionState {
    QMutex mutex;
    QHash<int, PendingRequest> pending;
    QSet<QString> refused;        // names the user refused since the application started
    int nextRequestCode = 0x4e74; // "Nt"
    bool nativesRegistered = false;
};

PermissionState &permissionState() {
    static PermissionState state;
    return state;
}

bool isGranted(const QString &name) {
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    if (!context.isValid())
        return false;
    const QJniObject javaName = QJniObject::fromString(name);
    return context.callMethod<jint>("checkSelfPermission", "(Ljava/lang/String;)I", javaName.object<jstring>()) ==
           kPermissionGranted;
}

void deliver(const QPointer<QObject> &target, std::function<void(Qt::PermissionStatus)> answer,
             Qt::PermissionStatus status) {
    QObject *receiver = target ? target.data() : QCoreApplication::instance();
    if (!receiver)
        return;
    QMetaObject::invokeMethod(receiver, [answer = std::move(answer), status] { answer(status); }, Qt::QueuedConnection);
}

bool askActivity(const QStringList &names, int requestCode);

// Called by NtActivity.onRequestPermissionsResult() on the UI thread.
void nativePermissionResult(JNIEnv *env, jclass, jint requestCode, jobjectArray names, jintArray results) {
    PermissionState &state = permissionState();
    PendingRequest request;
    {
        QMutexLocker locker(&state.mutex);
        const auto it = state.pending.find(requestCode);
        if (it == state.pending.end())
            return;
        request = std::move(it.value());
        state.pending.erase(it);
    }
    // An empty answer means the request was interrupted (the activity went away): nothing was refused.
    const jsize count = names && results ? env->GetArrayLength(results) : 0;
    bool allGranted = count > 0;
    if (count > 0) {
        jint *values = env->GetIntArrayElements(results, nullptr);
        QMutexLocker locker(&state.mutex);
        for (jsize i = 0; i < count; ++i) {
            auto *javaName = static_cast<jstring>(env->GetObjectArrayElement(names, i));
            const QString name = QJniObject(javaName).toString();
            env->DeleteLocalRef(javaName);
            if (values[i] == kPermissionGranted) {
                state.refused.remove(name);
            } else {
                state.refused.insert(name);
                allGranted = false;
            }
        }
        env->ReleaseIntArrayElements(results, values, JNI_ABORT);
    }
    if (allGranted && !request.later.isEmpty()) {
        // The second step: background location, asked for on its own as Android 11 and later require.
        const QStringList later = std::exchange(request.later, {});
        int code = 0;
        {
            QMutexLocker locker(&state.mutex);
            code = state.nextRequestCode++;
            state.pending.insert(code, std::move(request));
        }
        if (!askActivity(later, code)) {
            QMutexLocker locker(&state.mutex);
            request = state.pending.take(code);
            locker.unlock();
            deliver(request.target, std::move(request.answer), Qt::PermissionStatus::Denied);
        }
        return;
    }
    const Qt::PermissionStatus status = allGranted  ? Qt::PermissionStatus::Granted
                                        : count > 0 ? Qt::PermissionStatus::Denied
                                                    : Qt::PermissionStatus::Undetermined;
    deliver(request.target, std::move(request.answer), status);
}

bool registerNatives() {
    PermissionState &state = permissionState();
    {
        QMutexLocker locker(&state.mutex);
        if (state.nativesRegistered)
            return true;
    }
    QJniEnvironment env;
    const JNINativeMethod methods[] = {
        {"nativePermissionResult", "(I[Ljava/lang/String;[I)V", reinterpret_cast<void *>(&nativePermissionResult)}};
    // A plain NativeActivity (packaged without a JDK) has no org.nt.NtActivity to answer.
    if (!env.findClass(kActivityClass) || !env.registerNativeMethods(kActivityClass, methods, 1))
        return false;
    QMutexLocker locker(&state.mutex);
    state.nativesRegistered = true;
    return true;
}

bool askActivity(const QStringList &names, int requestCode) {
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    QJniEnvironment env;
    jclass activityClass = env.findClass(kActivityClass);
    if (!context.isValid() || !activityClass || !env->IsInstanceOf(context.object(), activityClass))
        return false;
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(names.size()), stringClass, nullptr);
    for (qsizetype i = 0; i < names.size(); ++i) {
        const QJniObject name = QJniObject::fromString(names.at(i));
        env->SetObjectArrayElement(array, static_cast<jsize>(i), name.object());
    }
    // requestNtPermissions() runs Activity.requestPermissions() on the UI thread.
    context.callMethod<void>("requestNtPermissions", "([Ljava/lang/String;I)V", array, static_cast<jint>(requestCode));
    env->DeleteLocalRef(array);
    env->DeleteLocalRef(stringClass);
    return !env.checkAndClearExceptions();
}

} // namespace

Qt::PermissionStatus QtPrivate::androidCheckPermission(const QPermission &permission) {
    const QStringList names = androidPermissionNames(permission, QNativeInterface::QAndroidApplication::sdkVersion());
    if (names.isEmpty())
        return Qt::PermissionStatus::Denied;
    bool granted = true;
    for (const QString &name : names)
        granted = isGranted(name) && granted;
    if (granted)
        return Qt::PermissionStatus::Granted;
    PermissionState &state = permissionState();
    QMutexLocker locker(&state.mutex);
    for (const QString &name : names) {
        if (state.refused.contains(name))
            return Qt::PermissionStatus::Denied;
    }
    return Qt::PermissionStatus::Undetermined;
}

void QtPrivate::androidRequestPermission(const QPermission &permission, QObject *target,
                                         std::function<void(Qt::PermissionStatus)> answer) {
    const int sdk = QNativeInterface::QAndroidApplication::sdkVersion();
    QStringList names = androidPermissionNames(permission, sdk);
    QStringList missing;
    for (const QString &name : names) {
        if (!isGranted(name))
            missing.append(name);
    }
    if (missing.isEmpty()) {
        deliver(target, std::move(answer),
                names.isEmpty() ? Qt::PermissionStatus::Denied : Qt::PermissionStatus::Granted);
        return;
    }
    PendingRequest request{target, std::move(answer), {}};
    // From Android 11 background location may only be asked for after the foreground one is granted.
    if (sdk >= 30 && missing.contains(QLatin1String(kBackgroundLocation)) && missing.size() > 1) {
        missing.removeAll(QLatin1String(kBackgroundLocation));
        request.later.append(QLatin1String(kBackgroundLocation));
    }
    if (!registerNatives()) {
        deliver(request.target, std::move(request.answer), Qt::PermissionStatus::Denied);
        return;
    }
    PermissionState &state = permissionState();
    int code = 0;
    {
        QMutexLocker locker(&state.mutex);
        code = state.nextRequestCode++;
        state.pending.insert(code, std::move(request));
    }
    if (!askActivity(missing, code)) {
        QMutexLocker locker(&state.mutex);
        PendingRequest failed = state.pending.take(code);
        locker.unlock();
        deliver(failed.target, std::move(failed.answer), Qt::PermissionStatus::Denied);
    }
}
