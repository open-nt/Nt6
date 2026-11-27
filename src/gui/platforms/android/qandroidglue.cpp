// MIT License - see LICENSE
//
// The entry point of an Nt application on Android. The application is a shared library that defines main(), as a
// Qt 6 application is; android.app.NativeActivity loads it and calls ANativeActivity_onCreate(), which starts a
// thread that runs main(). Qt's own activity is written in Java, which Nt does without.
#include "qandroidglue_p.h"

#include "android/qjni_android_p.h"

#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <jni.h>

#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>

// main() may not be named in C++ code; this declares the symbol of the application under another name.
extern "C" int ntAndroidApplicationMain(int argc, char *argv[]) __asm__("main");

namespace QtAndroid {

Glue &Glue::instance() {
    static Glue *glue = new Glue;
    return *glue;
}

void Glue::attach(ANativeActivity *activity) {
    m_activity = activity;
    if (::pipe(m_pipe) == 0) {
        for (int fd : m_pipe)
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    }
    m_config = AConfiguration_new();
    AConfiguration_fromAssetManager(m_config, activity->assetManager);
}

void Glue::post(const Event &event) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }
    const char byte = 1;
    if (::write(m_pipe[1], &byte, 1) < 0) {
        // A full pipe means the application thread has plenty to read already.
    }
}

void Glue::drainDescriptor() {
    char buffer[64];
    while (::read(m_pipe[0], buffer, sizeof buffer) > 0) {
    }
}

bool Glue::next(Event *event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_events.empty())
        return false;
    *event = m_events.front();
    m_events.pop_front();
    return true;
}

ANativeWindow *Glue::waitForWindow(int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return m_window != nullptr; });
    return m_window;
}

ANativeWindow *Glue::window() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_window;
}

void Glue::setWindow(ANativeWindow *window) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_window = window;
        m_windowGone = false;
    }
    m_changed.notify_all();
    Event e;
    e.kind = Event::WindowCreated;
    post(e);
}

// The UI thread must not return from onNativeWindowDestroyed() while the application still draws into the window.
void Glue::windowGoingAway() {
    Event e;
    e.kind = Event::WindowDestroyed;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_window = nullptr;
        m_windowGone = false;
    }
    post(e);
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_running)
        m_changed.wait_for(lock, std::chrono::seconds(2), [this] { return m_windowGone; });
}

void Glue::windowReleased() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_windowGone = true;
    }
    m_changed.notify_all();
}

int Glue::density() const {
    const int dpi = m_config ? AConfiguration_getDensity(m_config) : 0;
    return dpi > 0 && dpi < 0xfffe ? dpi : 160;
}

// The system bars and cut-outs, as the insets of the decor view of the activity; NativeActivity reports no more than
// the whole window. Asked on the application thread, through JNI.
ARect Glue::systemInsets(int *bottomWithoutKeyboard) {
    ARect insets = {0, 0, 0, 0};
    if (bottomWithoutKeyboard)
        *bottomWithoutKeyboard = 0;
    if (!m_activity || !m_activity->vm || !m_activity->clazz)
        return insets;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (m_activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (m_activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            return insets;
        attached = true;
    }
    const auto call = [env](jobject object, const char *name, const char *signature) -> jobject {
        if (!object)
            return nullptr;
        jclass cls = env->GetObjectClass(object);
        jmethodID method = cls ? env->GetMethodID(cls, name, signature) : nullptr;
        jobject result = method ? env->CallObjectMethod(object, method) : nullptr;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            result = nullptr;
        }
        if (cls)
            env->DeleteLocalRef(cls);
        return result;
    };
    jobject window = call(m_activity->clazz, "getWindow", "()Landroid/view/Window;");
    jobject decor = call(window, "getDecorView", "()Landroid/view/View;");
    jobject windowInsets = call(decor, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
    if (windowInsets) {
        jclass cls = env->GetObjectClass(windowInsets);
        const auto read = [&](const char *name) {
            jmethodID method = env->GetMethodID(cls, name, "()I");
            const int value = method ? env->CallIntMethod(windowInsets, method) : 0;
            if (env->ExceptionCheck())
                env->ExceptionClear();
            return value;
        };
        insets.left = read("getSystemWindowInsetLeft");
        insets.top = read("getSystemWindowInsetTop");
        insets.right = read("getSystemWindowInsetRight");
        insets.bottom = read("getSystemWindowInsetBottom");
        int barsBottom = -1; // what the system bars alone take at the bottom, if the system says
        // The keyboard (Android 11 and later).
        jmethodID getInsets = env->GetMethodID(cls, "getInsets", "(I)Landroid/graphics/Insets;");
        if (getInsets) {
            const jint barTypes = 7; // WindowInsets.Type.systemBars()
            jobject bars = env->CallObjectMethod(windowInsets, getInsets, barTypes);
            if (!env->ExceptionCheck() && bars) {
                jclass insetsClass = env->GetObjectClass(bars);
                jfieldID bottom = env->GetFieldID(insetsClass, "bottom", "I");
                if (bottom)
                    barsBottom = env->GetIntField(bars, bottom);
                env->DeleteLocalRef(insetsClass);
            }
            if (env->ExceptionCheck())
                env->ExceptionClear();
            if (bars)
                env->DeleteLocalRef(bars);
            const jint imeType = 1 << 3; // WindowInsets.Type.ime()
            jobject ime = env->CallObjectMethod(windowInsets, getInsets, imeType);
            if (!env->ExceptionCheck() && ime) {
                jclass insetsClass = env->GetObjectClass(ime);
                jfieldID bottom = env->GetFieldID(insetsClass, "bottom", "I");
                if (bottom)
                    insets.bottom = std::max(insets.bottom, env->GetIntField(ime, bottom));
                env->DeleteLocalRef(insetsClass);
            }
            if (env->ExceptionCheck())
                env->ExceptionClear();
            if (ime)
                env->DeleteLocalRef(ime);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        env->DeleteLocalRef(cls);
        if (bottomWithoutKeyboard)
            *bottomWithoutKeyboard = barsBottom >= 0 ? barsBottom : insets.bottom;
    }
    for (jobject local : {windowInsets, decor, window})
        if (local)
            env->DeleteLocalRef(local);
    if (attached)
        m_activity->vm->DetachCurrentThread();
    return insets;
}

namespace {

// The JNI environment of the calling thread, attached for as long as the object lives.
class JniScope {
public:
    explicit JniScope(ANativeActivity *activity) : m_vm(activity ? activity->vm : nullptr) {
        if (!m_vm)
            return;
        if (m_vm->GetEnv(reinterpret_cast<void **>(&m_env), JNI_VERSION_1_6) != JNI_OK) {
            if (m_vm->AttachCurrentThread(&m_env, nullptr) == JNI_OK)
                m_attached = true;
            else
                m_env = nullptr;
        }
    }
    ~JniScope() {
        if (m_attached)
            m_vm->DetachCurrentThread();
    }
    JniScope(const JniScope &) = delete;
    JniScope &operator=(const JniScope &) = delete;
    JNIEnv *env() const { return m_env; }
    // A Java exception is dropped: the caller sees a null result.
    bool failed() const {
        if (!m_env->ExceptionCheck())
            return false;
        m_env->ExceptionClear();
        return true;
    }

private:
    JavaVM *m_vm;
    JNIEnv *m_env = nullptr;
    bool m_attached = false;
};

} // namespace

std::string Glue::clipboardText() {
    JniScope jni(m_activity);
    JNIEnv *env = jni.env();
    if (!env || !m_activity->clazz)
        return std::string();
    std::string text;
    jclass activityClass = env->GetObjectClass(m_activity->clazz);
    jmethodID getService =
        env->GetMethodID(activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = env->NewStringUTF("clipboard");
    jobject manager = getService ? env->CallObjectMethod(m_activity->clazz, getService, name) : nullptr;
    if (!jni.failed() && manager) {
        jclass managerClass = env->GetObjectClass(manager);
        jmethodID getClip = env->GetMethodID(managerClass, "getPrimaryClip", "()Landroid/content/ClipData;");
        jobject clip = getClip ? env->CallObjectMethod(manager, getClip) : nullptr;
        if (!jni.failed() && clip) {
            jclass clipClass = env->GetObjectClass(clip);
            jmethodID count = env->GetMethodID(clipClass, "getItemCount", "()I");
            jmethodID itemAt = env->GetMethodID(clipClass, "getItemAt", "(I)Landroid/content/ClipData$Item;");
            if (count && itemAt && env->CallIntMethod(clip, count) > 0 && !jni.failed()) {
                jobject item = env->CallObjectMethod(clip, itemAt, 0);
                if (!jni.failed() && item) {
                    jclass itemClass = env->GetObjectClass(item);
                    jmethodID coerce = env->GetMethodID(itemClass, "coerceToText",
                                                        "(Landroid/content/Context;)Ljava/lang/CharSequence;");
                    jobject sequence = coerce ? env->CallObjectMethod(item, coerce, m_activity->clazz) : nullptr;
                    if (!jni.failed() && sequence) {
                        jclass objectClass = env->GetObjectClass(sequence);
                        jmethodID toString = env->GetMethodID(objectClass, "toString", "()Ljava/lang/String;");
                        auto *string =
                            toString ? static_cast<jstring>(env->CallObjectMethod(sequence, toString)) : nullptr;
                        if (!jni.failed() && string) {
                            const char *chars = env->GetStringUTFChars(string, nullptr);
                            if (chars) {
                                text = chars;
                                env->ReleaseStringUTFChars(string, chars);
                            }
                            env->DeleteLocalRef(string);
                        }
                        env->DeleteLocalRef(objectClass);
                        env->DeleteLocalRef(sequence);
                    }
                    env->DeleteLocalRef(itemClass);
                    env->DeleteLocalRef(item);
                }
            }
            env->DeleteLocalRef(clipClass);
            env->DeleteLocalRef(clip);
        }
        env->DeleteLocalRef(managerClass);
        env->DeleteLocalRef(manager);
    }
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(activityClass);
    return text;
}

void Glue::setClipboardText(const std::string &text) {
    JniScope jni(m_activity);
    JNIEnv *env = jni.env();
    if (!env || !m_activity->clazz)
        return;
    jclass activityClass = env->GetObjectClass(m_activity->clazz);
    jmethodID getService =
        env->GetMethodID(activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = env->NewStringUTF("clipboard");
    jobject manager = getService ? env->CallObjectMethod(m_activity->clazz, getService, name) : nullptr;
    jclass clipClass = env->FindClass("android/content/ClipData");
    if (!jni.failed() && manager && clipClass) {
        jmethodID newPlainText = env->GetStaticMethodID(
            clipClass, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
        jstring label = env->NewStringUTF("Nt");
        jstring value = env->NewStringUTF(text.c_str());
        jobject clip = newPlainText ? env->CallStaticObjectMethod(clipClass, newPlainText, label, value) : nullptr;
        if (!jni.failed() && clip) {
            jclass managerClass = env->GetObjectClass(manager);
            jmethodID setClip = env->GetMethodID(managerClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");
            if (setClip) {
                env->CallVoidMethod(manager, setClip, clip);
                jni.failed();
            }
            env->DeleteLocalRef(managerClass);
            env->DeleteLocalRef(clip);
        }
        env->DeleteLocalRef(label);
        env->DeleteLocalRef(value);
    }
    if (clipClass)
        env->DeleteLocalRef(clipClass);
    if (manager)
        env->DeleteLocalRef(manager);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(activityClass);
}

namespace {
void nativeText(JNIEnv *env, jclass, jstring text);
void nativeKey(JNIEnv *, jclass, jint action, jint keyCode, jint metaState);
void nativeInsetsChanged(JNIEnv *, jclass);
} // namespace

// The activity of the package is org.nt.NtActivity when the APK was made with a JDK; its native methods are bound here,
// not by name, so that the application library need not export them.
void Glue::registerNtActivity() {
    JniScope jni(m_activity);
    JNIEnv *env = jni.env();
    if (!env || !m_activity->clazz)
        return;
    jclass cls = env->GetObjectClass(m_activity->clazz);
    const JNINativeMethod methods[] = {{"nativeText", "(Ljava/lang/String;)V", reinterpret_cast<void *>(&nativeText)},
                                       {"nativeKey", "(III)V", reinterpret_cast<void *>(&nativeKey)},
                                       {"nativeInsetsChanged", "()V", reinterpret_cast<void *>(&nativeInsetsChanged)}};
    m_ntActivity = env->RegisterNatives(cls, methods, 3) == JNI_OK;
    jni.failed();
    env->DeleteLocalRef(cls);
}

void Glue::setSoftInputHints(int hints) {
    if (!m_activity || !m_ntActivity)
        return; // the plain NativeActivity has no say in what its keyboard shows
    JniScope jni(m_activity);
    JNIEnv *env = jni.env();
    if (!env)
        return;
    jclass cls = env->GetObjectClass(m_activity->clazz);
    jmethodID set = env->GetMethodID(cls, "setNtInputHints", "(I)V");
    if (set)
        env->CallVoidMethod(m_activity->clazz, set, static_cast<jint>(hints));
    jni.failed();
    env->DeleteLocalRef(cls);
}

void Glue::setSoftInputVisible(bool visible) {
    if (!m_activity)
        return;
    if (m_ntActivity) {
        JniScope jni(m_activity);
        JNIEnv *env = jni.env();
        if (!env)
            return;
        jclass cls = env->GetObjectClass(m_activity->clazz);
        jmethodID show = env->GetMethodID(cls, "showNtInput", "(Z)V");
        if (show)
            env->CallVoidMethod(m_activity->clazz, show, static_cast<jboolean>(visible));
        jni.failed();
        env->DeleteLocalRef(cls);
        return;
    }
    if (visible)
        ANativeActivity_showSoftInput(m_activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
    else
        ANativeActivity_hideSoftInput(m_activity, ANATIVEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS);
}

ARect Glue::contentRect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_content;
}

void Glue::setContentRect(const ARect &rect) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_content = rect;
    }
    Event e;
    e.kind = Event::WindowResized;
    post(e);
}

bool Glue::nightMode() const {
    return m_config && AConfiguration_getUiModeNight(m_config) == ACONFIGURATION_UI_MODE_NIGHT_YES;
}

void Glue::configurationChanged() {
    if (m_config)
        AConfiguration_fromAssetManager(m_config, m_activity->assetManager);
    Event e;
    e.kind = Event::ConfigChanged;
    post(e);
}

void Glue::finish() {
    if (m_activity)
        ANativeActivity_finish(m_activity);
}

namespace {

void nativeText(JNIEnv *env, jclass, jstring text) {
    const char *chars = env->GetStringUTFChars(text, nullptr);
    if (!chars)
        return;
    Event e;
    e.kind = Event::Text;
    e.text = chars;
    env->ReleaseStringUTFChars(text, chars);
    Glue::instance().post(e);
}

void nativeKey(JNIEnv *, jclass, jint action, jint keyCode, jint metaState) {
    Event e;
    e.kind = Event::Key;
    e.action = action;
    e.keyCode = keyCode;
    e.meta = metaState;
    Glue::instance().post(e);
}

void nativeInsetsChanged(JNIEnv *, jclass) {
    Event e;
    e.kind = Event::WindowResized;
    Glue::instance().post(e);
}

int inputCallback(int, int, void *data) {
    auto *queue = static_cast<AInputQueue *>(data);
    AInputEvent *event = nullptr;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(queue, event))
            continue;
        bool handled = false;
        const int32_t type = AInputEvent_getType(event);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            const int32_t action = AMotionEvent_getAction(event);
            const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
            const size_t index =
                (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            Event e;
            e.kind = Event::Touch;
            e.action = masked;
            const size_t count = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < count; ++i) {
                Event::TouchPoint p;
                p.id = AMotionEvent_getPointerId(event, i);
                p.x = AMotionEvent_getX(event, i);
                p.y = AMotionEvent_getY(event, i);
                const bool changed = i == index;
                if (masked == AMOTION_EVENT_ACTION_MOVE)
                    p.state = 2;
                else if (changed &&
                         (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN))
                    p.state = 1;
                else if (changed && (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP))
                    p.state = 3;
                e.points.push_back(p);
            }
            Glue::instance().post(e);
            handled = true;
        } else if (type == AINPUT_EVENT_TYPE_KEY) {
            Event e;
            e.kind = Event::Key;
            e.action = AKeyEvent_getAction(event);
            e.keyCode = AKeyEvent_getKeyCode(event);
            e.meta = AKeyEvent_getMetaState(event);
            Glue::instance().post(e);
            handled = true;
        }
        AInputQueue_finishEvent(queue, event, handled ? 1 : 0);
    }
    return 1;
}

void *applicationThread(void *) {
    char name[] = "app";
    char *argv[] = {name, nullptr};
    Glue &glue = Glue::instance();
    const int result = ntAndroidApplicationMain(1, argv);
    __android_log_print(ANDROID_LOG_INFO, "Nt", "main() returned %d", result);
    glue.setApplicationRunning(false);
    glue.finish();
    return nullptr;
}

void onDestroy(ANativeActivity *activity) {
    Event e;
    e.kind = Event::Close;
    Glue::instance().post(e);
    // The process is not reused: static objects of the application are not meant to see a second main().
    (void)activity;
    for (int i = 0; i < 100 && Glue::instance().applicationRunning(); ++i)
        ::usleep(20000);
    std::_Exit(0);
}

void onWindowCreated(ANativeActivity *, ANativeWindow *window) { Glue::instance().setWindow(window); }
void onWindowDestroyed(ANativeActivity *, ANativeWindow *) { Glue::instance().windowGoingAway(); }
void onWindowResized(ANativeActivity *, ANativeWindow *) {
    Event e;
    e.kind = Event::WindowResized;
    Glue::instance().post(e);
}
void onContentRectChanged(ANativeActivity *, const ARect *rect) { Glue::instance().setContentRect(*rect); }
void onInputQueueCreated(ANativeActivity *, AInputQueue *queue) {
    AInputQueue_attachLooper(queue, ALooper_forThread(), ALOOPER_POLL_CALLBACK, inputCallback, queue);
}
void onInputQueueDestroyed(ANativeActivity *, AInputQueue *queue) { AInputQueue_detachLooper(queue); }
void onConfigurationChanged(ANativeActivity *) { Glue::instance().configurationChanged(); }
void onPause(ANativeActivity *) {
    Event e;
    e.kind = Event::Pause;
    Glue::instance().post(e);
}
void onResume(ANativeActivity *) {
    Event e;
    e.kind = Event::Resume;
    Glue::instance().post(e);
}
void *onSaveInstanceState(ANativeActivity *, size_t *size) {
    *size = 0;
    return nullptr;
}

} // namespace
} // namespace QtAndroid

extern "C" __attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity *activity, void *,
                                                                                size_t) {
    using namespace QtAndroid;
    Glue &glue = Glue::instance();
    glue.attach(activity);
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onNativeWindowCreated = onWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onWindowDestroyed;
    activity->callbacks->onNativeWindowResized = onWindowResized;
    activity->callbacks->onContentRectChanged = onContentRectChanged;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    // The application's files live in the private directory of the package.
    if (activity->internalDataPath) {
        ::setenv("HOME", activity->internalDataPath, 1);
        if (::chdir(activity->internalDataPath) != 0) {
            // Stay where the process started.
        }
    }
    // Nt Core gets the Java VM and the activity, for QJniObject and QNativeInterface::QAndroidApplication.
    QtAndroidPrivate::initJni(activity->vm, activity->env, activity->clazz, activity->sdkVersion);
    glue.registerNtActivity();
    glue.setApplicationRunning(true);
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, applicationThread, nullptr);
    pthread_attr_destroy(&attr);
}
