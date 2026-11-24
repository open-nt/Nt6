// MIT License - see LICENSE
//
// Page about the Android port for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-android.html
    \title Nt on Android
    \brief Building Qt 6 applications for Android with the NDK.

    A project written for Qt 6 builds for Android against Nt, with its \c CMakeLists.txt unchanged: it finds
    \c Qt6 and links \c Qt6::Widgets, uses \c qt_add_executable(), and has a plain \c main(). The result is an
    APK that installs with \c{adb install}. Nt has an \c android platform for Nt GUI, which draws with the raster
    engine into the window of an \c{android.app.NativeActivity}, so no Java is involved.

    The port is at an early stage. It was built with NDK r25 for \c arm64-v8a and \c x86_64, and a Widgets
    application runs on the Android 15 emulator for \c x86_64; it has not been run on a device yet. OpenGL ES, in QOpenGLWindow and QOpenGLWidget, runs on the emulator too.

    \section1 What is built

    Nt Core, Network, Sql, Xml, Concurrent, Test, Qml, FM, Gpio, WebSockets, HttpServer, Positioning, Twin, Comms
    and Twin Network, and Nt GUI with Widgets, Svg, Svg Widgets, Charts, OpenGL and OpenGL Widgets. Left out are
    Serial Port, Gamepad, Gltf and Barcode (they depend on
    Linux devices or have not been tried), Multimedia and the Wayland compositor.

    \section1 Building Nt for Android

    A cross build has no tools of its own: \c ntmoc and \c ntrcc run on the build machine. Build Nt for the build
    machine first, or let CMake do it (see \c cmake/NtHostTools.cmake), then build and install Nt with the NDK:

    \code
    cmake -S Nt -B build-nt-android -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
        -DNT_HOST_TOOLS_DIR=<directory with ntmoc and ntrcc> \
        -DCMAKE_INSTALL_PREFIX=$HOME/nt-android
    cmake --build build-nt-android
    cmake --install build-nt-android
    \endcode

    Use \c android-26 or newer as the platform. FreeType is downloaded and built with Nt for text (the system fonts
    in \c /system/fonts are used); \c NT_FREETYPE_SOURCE_DIR names a source tree to use instead, for a build without
    network access. Vulkan, Zstandard and JPEG are left out.

    \section1 Building an application

    The installation holds \c Qt6Config.cmake, which answers \c{find_package(Qt6)} with Nt, and
    \c nt-android.toolchain.cmake, which loads the toolchain file of the NDK, finds Nt and takes the ABI and
    platform of the Nt build. A Qt project needs nothing else:

    \code
    cmake_minimum_required(VERSION 3.18)
    project(app LANGUAGES CXX)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    qt_standard_project_setup()
    qt_add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Qt6::Widgets)
    \endcode

    \code
    export ANDROID_SDK_ROOT=$HOME/Android/Sdk
    cmake -S . -B build -G Ninja -DANDROID_NDK=$ANDROID_SDK_ROOT/ndk/<version> \
        -DCMAKE_TOOLCHAIN_FILE=$HOME/nt-android/lib/cmake/Nt6/nt-android.toolchain.cmake
    cmake --build build
    adb install build/app.apk
    \endcode

    On Android \c qt_add_executable() makes a shared library that defines \c main(), and the target \c app_apk
    packages it with a manifest into \c app.apk, signed with the debug key of the SDK. The Android SDK (build tools
    and a platform) is found from \c ANDROID_SDK_ROOT. \c nt_android_add_apk() takes the package name, the label,
    the version and a key store; \c MANUAL_FINALIZATION leaves the packaging to a call of \c qt_finalize_target().

    \section1 The android platform

    The application thread runs \c main(); the activity's UI thread passes input, the native window and the
    life cycle to it. The screen is the window of the activity, and every top-level window fills it, with the one
    shown last on top. The scale factor is the density of the screen over 160, and the logical resolution is 120
    dots per inch. The first finger is the mouse (a press, moves and a release with the left button), keys from a
    keyboard type text, and the Back key closes the window on top.

    \section1 Differences from Qt for Android

    \list
    \li Widgets are painted by the raster engine. OpenGL ES works, through the EGL library of the system, in
        QOpenGLWindow and QOpenGLWidget; Vulkan is not available on Android yet, so QVulkanWindow does not draw.
    \li The Java part is one class, \c org.nt.NtActivity, a NativeActivity with a view for the keyboard to type into. It is
        compiled with \c javac and \c d8 when the APK is made; without a JDK the application is a plain NativeActivity
        and has no on-screen keyboard. There is no QJniObject, no Android permissions API and no assets: \c{assets:}
        paths do not exist, and resources are the compiled-in kind.
    \li The on-screen keyboard appears when a widget that takes text gets the focus (QInputMethod::show()), and the
        window shrinks to leave it free. It types plain text: no composing text, suggestions or input method
        events, and the keys it sends are those a hardware keyboard would send.
    \li The clipboard is that of the system, for plain text; QClipboard::changed() is not emitted when another
        application copies.
    \li Touch is the mouse. There are no QTouchEvent or gestures. A window is as large as the content area that the
        activity reports, which leaves out the system bars (QScreen::availableGeometry()); an OpenGL window draws into
        the whole surface, behind the bars.
    \li Only the first window is shown at a time; QMessageBox and other dialogs work because they are windows of
        their own that cover it.
    \li The activity is not recreated when the device turns, but the window follows the new size of the screen.
    \li QSharedMemory is not built (\c QT_NO_SHAREDMEMORY is defined), because Android has no POSIX shared memory.
    \li QThread::terminate() only warns, and QThread::setTerminationEnabled() does nothing, because Android has
        no \c pthread_cancel().
    \li Messages go to logcat under the tag \c Nt, as well as to \c stderr.
    \endlist

    \sa {Nt on Zephyr}
*/
