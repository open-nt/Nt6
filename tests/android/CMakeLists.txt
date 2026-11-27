# A Qt 6 style project that is built for Android against an installed Nt, to check the whole path: the CMake
# package, moc, resources, the shared library, the Java activity and the APK. CI does it (.github/workflows/android.yml):
#
#   cmake -S tests/android -B build-app -G Ninja -DANDROID_NDK=$ANDROID_NDK \
#     -DCMAKE_TOOLCHAIN_FILE=<prefix>/lib/cmake/Nt6/nt-android.toolchain.cmake
#   cmake --build build-app
cmake_minimum_required(VERSION 3.18)
project(nt_android_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Network)
qt_standard_project_setup()
# Both spellings of the found variables are set, as by Qt's packages.
foreach(_module Widgets OpenGLWidgets Network)
    if(NOT Qt6_${_module}_FOUND OR NOT Qt6${_module}_FOUND)
        message(FATAL_ERROR "Qt6_${_module}_FOUND and Qt6${_module}_FOUND should both be set")
    endif()
endforeach()

qt_add_executable(nt_android_smoke main.cpp counter.h)
target_link_libraries(nt_android_smoke PRIVATE Qt6::Widgets Qt6::OpenGLWidgets Qt6::Network)

# QJniObject and friends, checked on a device (see jni.cpp).
qt_add_executable(nt_android_jni jni.cpp)
target_link_libraries(nt_android_jni PRIVATE Qt6::Gui)

# An application that adds its own activity (a subclass of NtActivity), a service, Java sources and permissions to the
# APK (nt_android_add_apk() options), as an application with Android integration of its own does.
qt_add_executable(nt_android_app MANUAL_FINALIZATION app/app.cpp)
target_link_libraries(nt_android_app PRIVATE Qt6::Widgets)
if(ANDROID)
    nt_android_add_apk(nt_android_app
        PACKAGE org.nt.smoke
        LABEL "Nt app"
        ACTIVITY org.nt.smoke.SmokeActivity
        PERMISSIONS ACCESS_FINE_LOCATION android.permission.CAMERA
        JAVA_SOURCES app/SmokeActivity.java app/SmokeService.java
        MANIFEST_APPLICATION app/application.xml)
endif()
