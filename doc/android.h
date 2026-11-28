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

    The port is at an early stage. It was built with NDK r25 and r30. Widgets, OpenGL ES, Vulkan and multi-touch
    applications run on the Android 15 emulator for \c x86_64, where Vulkan is in software. On an \c arm64-v8a
    phone with Android 14 (a Motorola moto g04s), Widgets, OpenGL ES and Vulkan applications, the on-screen
    keyboard, the clipboard and QJniObject work; multi-touch and rotation have not been tried there.

    \section1 What is built

    Nt Core, Network, Sql, D-Bus, Xml, Concurrent, Test, Qml, FM, Gpio, Sensors, SerialBus, Protobuf, Mqtt, Device,
    WebSockets, HttpServer, Positioning, Twin, Comms and Twin Network, and Nt GUI with Widgets, Svg, Svg Widgets,
    Charts, OpenGL, OpenGL Widgets, Multimedia and Multimedia Widgets. Several of these have only been compiled for
    Android, not run on it; Android has no D-Bus for Nt D-Bus to talk to. Left out are Serial Port, Gamepad, Gltf and
    Barcode (they depend on Linux devices or have not been tried) and the Wayland compositor.

    Nt Multimedia has the cameras (see \l{Cameras and position}) and no audio devices yet. Nt Positioning has the
    position source of the system, \c android.

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
    network access. Zstandard and JPEG are left out; Vulkan is built with the headers of the NDK. The NDK has neither OpenSSL nor SQLite,
    so Nt Network is built without TLS (no QSslSocket or \c https:) and Nt SQL without the \c QSQLITE driver.

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
    the version, a key store and what the application adds to the APK (see below); \c MANUAL_FINALIZATION leaves the packaging to a call of \c qt_finalize_target().

    \section1 Java sources, permissions and services

    An application that needs more of Android than a window gives \c nt_android_add_apk() what to add, and leaves
    the packaging to it with \c MANUAL_FINALIZATION:

    \code
    qt_add_executable(app MANUAL_FINALIZATION main.cpp)
    nt_android_add_apk(app
        PACKAGE com.example.app
        ACTIVITY com.example.app.AppActivity
        PERMISSIONS ACCESS_FINE_LOCATION android.permission.CAMERA
        JAVA_SOURCES android/AppActivity.java android/AppService.java
        MANIFEST_APPLICATION android/application.xml)
    \endcode

    \c PERMISSIONS become \c{<uses-permission>} entries (a name without a dot is taken from
    \c{android.permission}), next to the network permissions every Nt application has. \c JAVA_SOURCES are
    compiled together with \c org.nt.NtActivity into the APK's \c classes.dex. \c ACTIVITY is the activity the
    launcher starts; it must extend \c org.nt.NtActivity, which keeps the window, input and keyboard of Nt, and call
    \c super.onCreate() first. \c MANIFEST_APPLICATION names a file of XML that is put inside \c{<application>},
    after the activity: services, receivers, providers and meta-data. Declaring a permission does not grant a
    dangerous one such as location or the camera: the user still has to allow it, in the system settings or when the
    application asks with QCoreApplication::requestPermission() (QLocationPermission, QCameraPermission and the
    other permission classes), which shows the system's prompt.

    \section1 Prebuilt libraries and assets

    \c EXTRA_LIBS names prebuilt shared libraries that the application links to, which go into the APK beside its
    own library, as Qt's \c QT_ANDROID_EXTRA_LIBS target property does (it is read too). \c ASSETS names a
    directory whose files become the APK's assets; without it the \c assets directory of the target's
    \c QT_ANDROID_PACKAGE_SOURCE_DIR is taken, as in Qt:

    \code
    nt_android_add_apk(app
        EXTRA_LIBS ${TFLITE_ROOT}/lib/arm64-v8a/libtensorflowlite_c.so
        ASSETS ${CMAKE_CURRENT_BINARY_DIR}/assets)
    \endcode

    The application reads an asset with QFile as \c{assets:/<path>}; QFileInfo and QDir see it too (a directory
    lists its files, not its subdirectories). Assets are stored uncompressed, so QFile::map() gives the address of
    the asset inside the APK and a large file, such as a neural network model, costs no memory of its own.

    When \c qt_add_executable() makes the APK by itself (without \c MANUAL_FINALIZATION), it does so at the end of
    the directory, so it sees properties set after the call.

    \section1 Cameras and position

    QMediaDevices::videoInputs() lists the cameras of the NDK's Camera2 API (\c ACameraManager), the back
    camera first. A QCamera in a QMediaCaptureSession with a QVideoSink delivers NV12 (or NV21 or YUV420P) frames,
    which carry the rotation that shows them upright as the display was turned when the camera started
    (QVideoFrame::rotation()), and are mirrored for a front camera; QVideoFrame::toImage() applies both. The
    formats are the YUV sizes of the camera, with the highest frame rate each allows. The application needs the
    camera permission first:

    \code
    QCameraPermission camera;
    qApp->requestPermission(camera, this, [this](const QPermission &p) {
        if (p.status() == Qt::PermissionStatus::Granted)
            m_camera->start();
    });
    \endcode

    QGeoPositionInfoSource::createDefaultSource() makes the \c android source, which takes fixes from the GPS and
    network providers of the \c LocationManager, as preferredPositioningMethods() asks, after QLocationPermission
    has been granted. It needs \c org.nt.NtActivity (a JDK when the APK is made).

    \section1 Launcher icon

    \c ICON names a PNG, best 512 pixels square, that becomes the application's launcher icon:

    \code
    nt_android_add_apk(app PACKAGE com.example.app ICON icon.png)
    \endcode

    \section1 Calling Java

    QJniObject, QJniEnvironment and QNativeInterface::QAndroidApplication of Nt Core call the Java APIs of Android,
    as in Qt. Code that uses views, toasts or dialogs has to run on the UI thread, with
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread(); \c main() runs on a thread of its own. As in
    Qt, \c Q_OS_ANDROID is defined for Android code (with \c Q_OS_LINUX and \c Q_OS_UNIX, see
    \c{<QtCore/qsystemdetection.h>}), so the Android parts of a Qt application are compiled in.

    \section1 The android platform

    The application thread runs \c main(); the activity's UI thread passes input, the native window and the
    life cycle to it. The screen is the window of the activity. A top-level window fills it, with the one shown last on
    top; a dialog over such a window keeps its size (280 to 560 wide), is centred, and what is below it is dimmed. The scale factor is the density of the screen over 160, and the logical resolution is 120
    dots per inch. Touch screens deliver QTouchEvent with every finger, and the first finger is the mouse
    (a press, moves and a release with the left button) for the widgets that do not take touch events. A finger goes
    to the topmost window that it is in; beside a dialog that is not modal (such as a standard bottom sheet of the
    Material style) it goes to the window below, and beside a modal one to that dialog, or it closes a modal sheet.
    The window touched becomes the active one. Keys from a
    keyboard type text, and the Back key closes the window on top.

    \section1 Differences from Qt for Android

    \list
    \li Widgets are painted by the raster engine. OpenGL ES works, through the EGL library of the system, in
        QOpenGLWindow and QOpenGLWidget. Vulkan presents through \c{VK_KHR_android_surface}, loaded at run time from
        the system, so QVulkanWindow works; like an OpenGL window it draws into the whole surface. While the
        application is in the background there is no native window: QVulkanWindow releases its swapchain, device and
        surface and makes them again, but a program that uses QVulkanInstance::surfaceForWindow() in a QWindow of
        its own is only told with an expose event (isExposed() is false) and must release its swapchain then.
    \li The Java part is one class, \c org.nt.NtActivity, a NativeActivity with a view for the keyboard to type into. It is
        compiled with \c javac and \c d8 when the APK is made; without a JDK the application is a plain NativeActivity
        and has no on-screen keyboard, and QCoreApplication::requestPermission() answers
        \c Qt::PermissionStatus::Denied, as it cannot ask. An
        application's own Java is compiled with \c javac against \c android.jar alone: there is no Gradle, so
        AndroidX and other libraries cannot be used (see \l{Java sources, permissions and services}).
    \li Dialogs have no title bar. The Material style shows a dialog's window title inside it; the other styles do not show
        it.
    \li A dialog is shown inside the window, no wider than 560 or than the screen less 24 at each side. One that asks to be
        wider narrows itself first, and its labels that wrap their text are given the height they need at that width, as
        layouts have no height for a width.
    \li The on-screen keyboard appears when a widget that takes text gets the focus (QInputMethod::show()), and the
        window shrinks to leave it free. A window whose contents cannot be that short (QWidget::minimumSizeHint()) is not
        squeezed: it keeps the height it needs and slides up until the field with the focus is above the keyboard, as
        Android's own panning does (Android 11 and later; before that it shrinks). A field in a QScrollArea is scrolled
        into view. With \c org.nt.NtActivity it shows the keys for what the widget says it takes
        with its input method hints: digits or numbers (as a spin box does), a telephone number, an e-mail address, a
        URL, a date or a time, or a password. It types plain text: no composing text, suggestions or input method
        events, and the keys it sends are those a hardware keyboard would send.
    \li The clipboard is that of the system, for plain text; QClipboard::changed() is not emitted when another
        application copies.
    \li A widget that sets Qt::WA_AcceptTouchEvents and accepts QEvent::TouchBegin gets the fingers as QTouchEvent;
        otherwise the first finger is the mouse. There are no gestures (QGesture, QPinchGesture). A window is as large as the content area that the
        activity reports, which leaves out the system bars (QScreen::availableGeometry()); an OpenGL window draws into
        the whole surface, behind the bars.
    \li Windows cannot be moved or resized by the user, and there are no title bars: a dialog has to say what it is
        in its content. Dialogs such as QMessageBox are windows of
        their own that cover it.
    \li The activity is not recreated when the device turns, but the window follows the new size of the screen.
    \li QSharedMemory is not built (\c QT_NO_SHAREDMEMORY is defined), because Android has no POSIX shared memory.
    \li QThread::terminate() only warns, and QThread::setTerminationEnabled() does nothing, because Android has
        no \c pthread_cancel().
    \li Messages go to logcat under the tag \c Nt, as well as to \c stderr.
    \endlist

    \sa {Nt on Zephyr}
*/
