// MIT License - see LICENSE
#pragma once

#include <jni.h>

namespace QtAndroidPrivate {

// Called by the entry point of the activity, on the UI thread, before main() runs: gives Nt Core the Java VM, the
// activity (a global reference that lives as long as the process) and the API level.
void initJni(JavaVM *vm, JNIEnv *env, jobject activity, int sdkVersion);

} // namespace QtAndroidPrivate
