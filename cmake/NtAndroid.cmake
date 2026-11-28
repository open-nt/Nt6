# MIT License - see LICENSE
#
# Building applications for Android, the way a Qt 6 project does.
#
#   qt_add_executable(<name> [WIN32|MACOSX_BUNDLE] [MANUAL_FINALIZATION] <sources>...)
#   qt_standard_project_setup()
#   qt_finalize_target(<name>)
#   nt_android_add_apk(<target> [PACKAGE <id>] [LABEL <text>] [VERSION_CODE <n>] [VERSION_NAME <text>]
#                      [KEYSTORE <file>] [ACTIVITY <class>] [PERMISSIONS <name>...]
#                      [JAVA_SOURCES <file>...] [MANIFEST_APPLICATION <file>] [ICON <png>]
#                      [EXTRA_LIBS <so>...] [ASSETS <dir>])
#
# ACTIVITY names the activity to launch instead of org.nt.NtActivity; it must extend org.nt.NtActivity, so the
# window, input and keyboard still work. JAVA_SOURCES are compiled with NtActivity (against android.jar only: no
# AndroidX or other libraries) and go into the APK's classes.dex. PERMISSIONS are added as <uses-permission> (a
# name without a dot gets "android.permission." in front). MANIFEST_APPLICATION is a file whose XML is put inside
# <application>, after the activity: services, receivers, providers and meta-data.
#
# On Android qt_add_executable() makes a shared library, which android.app.NativeActivity loads and which defines
# main(), and an <name>.apk next to it (the target <name>_apk, part of "all"), at the end of the directory unless the
# project calls nt_android_add_apk() or qt_finalize_target() for it before. The package is signed with the debug
# key of the Android SDK (~/.android/debug.keystore, created when missing), so that `adb install` takes it. Elsewhere
# it is add_executable(). The Android SDK is found from ANDROID_SDK_ROOT, ANDROID_HOME or the directory above the
# NDK. ICON is a PNG for the launcher, best 512 pixels square; without it Android shows its default icon. Except in
# a Debug build, the application's library is stripped of its debugging information in the APK (not in the build).
#
# EXTRA_LIBS are prebuilt shared libraries that the application links to, put into the APK's lib/<abi>/ beside it,
# as Qt's QT_ANDROID_EXTRA_LIBS target property does (which is read too), each under its SONAME, the name the loader
# looks for. ASSETS is a directory whose files become the
# APK's assets, stored uncompressed, which the application reads as "assets:/<path>" with QFile; without it the
# assets/ directory of the target's QT_ANDROID_PACKAGE_SOURCE_DIR is taken, as Qt does.

include(${CMAKE_CURRENT_LIST_DIR}/NtAndroidApk.cmake OPTIONAL)

set(NT_ANDROID_TARGET_SDK "" CACHE STRING "targetSdkVersion of the applications; the newest platform of the SDK when empty")

function(nt_android_add_apk target)
    cmake_parse_arguments(ARG "" "PACKAGE;LABEL;VERSION_CODE;VERSION_NAME;KEYSTORE;ACTIVITY;MANIFEST_APPLICATION;ICON;ASSETS"
        "PERMISSIONS;JAVA_SOURCES;EXTRA_LIBS" ${ARGN})
    if(NOT ANDROID)
        message(FATAL_ERROR "nt_android_add_apk() needs the Android toolchain")
    endif()
    if(NOT ARG_PACKAGE)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _id "${target}")
        set(ARG_PACKAGE "org.nt.${_id}")
    endif()
    if(NOT ARG_LABEL)
        set(ARG_LABEL "${target}")
    endif()
    if(NOT ARG_VERSION_CODE)
        set(ARG_VERSION_CODE 1)
    endif()
    if(NOT ARG_VERSION_NAME)
        set(ARG_VERSION_NAME "1.0")
    endif()

    # The SDK: ANDROID_SDK_ROOT, ANDROID_HOME, or the directory that holds ndk/.
    set(_sdk "$ENV{ANDROID_SDK_ROOT}")
    if(NOT _sdk)
        set(_sdk "$ENV{ANDROID_HOME}")
    endif()
    if(NOT _sdk AND CMAKE_ANDROID_NDK)
        get_filename_component(_sdk "${CMAKE_ANDROID_NDK}/../.." ABSOLUTE)
    endif()
    file(GLOB _build_tools LIST_DIRECTORIES true "${_sdk}/build-tools/*")
    list(SORT _build_tools COMPARE NATURAL)
    list(REVERSE _build_tools)
    list(GET _build_tools 0 _tools)
    file(GLOB _platforms LIST_DIRECTORIES true "${_sdk}/platforms/android-*")
    list(SORT _platforms COMPARE NATURAL)
    list(REVERSE _platforms)
    list(GET _platforms 0 _platform)
    if(NOT _tools OR NOT _platform)
        message(WARNING "nt_android_add_apk(${target}): no Android SDK with build-tools and a platform in '${_sdk}'; "
            "set ANDROID_SDK_ROOT. No APK will be made.")
        return()
    endif()
    string(REGEX REPLACE ".*android-" "" _platform_level "${_platform}")
    set(_target_sdk "${NT_ANDROID_TARGET_SDK}")
    if(NOT _target_sdk)
        set(_target_sdk "${_platform_level}")
    endif()
    if(ARG_KEYSTORE)
        set(_keystore "${ARG_KEYSTORE}")
    else()
        set(_keystore "$ENV{HOME}/.android/debug.keystore")
    endif()

    get_target_property(_output ${target} OUTPUT_NAME)
    if(NOT _output)
        set(_output "${target}")
    endif()
    set(_apk "${CMAKE_CURRENT_BINARY_DIR}/${target}.apk")

    # What the application adds, handed to the packaging script in a file rather than on the command line, so lists
    # and paths need no escaping.
    set(_java_sources)
    foreach(_source IN LISTS ARG_JAVA_SOURCES)
        get_filename_component(_source "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND _java_sources "${_source}")
    endforeach()
    set(_permissions)
    foreach(_permission IN LISTS ARG_PERMISSIONS)
        if(NOT _permission MATCHES "\\.")
            set(_permission "android.permission.${_permission}")
        endif()
        list(APPEND _permissions "${_permission}")
    endforeach()
    set(_manifest_application "")
    if(ARG_MANIFEST_APPLICATION)
        get_filename_component(_manifest_application "${ARG_MANIFEST_APPLICATION}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    get_target_property(_qt_extra_libs ${target} QT_ANDROID_EXTRA_LIBS)
    set(_extra_libs)
    foreach(_library IN LISTS ARG_EXTRA_LIBS _qt_extra_libs)
        if(_library)
            get_filename_component(_library "${_library}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND _extra_libs "${_library}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _extra_libs)
    set(_assets "")
    if(ARG_ASSETS)
        get_filename_component(_assets "${ARG_ASSETS}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_target_property(_package_source ${target} QT_ANDROID_PACKAGE_SOURCE_DIR)
        if(_package_source AND IS_DIRECTORY "${_package_source}/assets")
            set(_assets "${_package_source}/assets")
        endif()
    endif()
    set(_asset_files)
    if(_assets)
        file(GLOB_RECURSE _asset_files "${_assets}/*")
    endif()
    set(_app_config "${CMAKE_CURRENT_BINARY_DIR}/${target}_apk.cmake")
    file(GENERATE OUTPUT "${_app_config}" CONTENT
"set(NT_APP_ACTIVITY [==[${ARG_ACTIVITY}]==])
set(NT_APP_PERMISSIONS [==[${_permissions}]==])
set(NT_APP_JAVA_SOURCES [==[${_java_sources}]==])
set(NT_APP_MANIFEST_APPLICATION [==[${_manifest_application}]==])
set(NT_APP_EXTRA_LIBS [==[${_extra_libs}]==])
set(NT_APP_ASSETS [==[${_assets}]==])
")
    set(_icon "")
    if(ARG_ICON)
        get_filename_component(_icon "${ARG_ICON}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    add_custom_target(${target}_apk ALL
        COMMAND ${CMAKE_COMMAND}
            "-DNT_APK=${_apk}"
            "-DNT_LIBRARY=$<TARGET_FILE:${target}>"
            "-DNT_LIBRARY_NAME=${_output}"
            "-DNT_ABI=${ANDROID_ABI}"
            "-DNT_PACKAGE=${ARG_PACKAGE}"
            "-DNT_LABEL=${ARG_LABEL}"
            "-DNT_VERSION_CODE=${ARG_VERSION_CODE}"
            "-DNT_VERSION_NAME=${ARG_VERSION_NAME}"
            "-DNT_MIN_SDK=${ANDROID_PLATFORM_LEVEL}"
            "-DNT_TARGET_SDK=${_target_sdk}"
            "-DNT_BUILD_TOOLS=${_tools}"
            "-DNT_ANDROID_JAR=${_platform}/android.jar"
            "-DNT_KEYSTORE=${_keystore}"
            "-DNT_WORK=${CMAKE_CURRENT_BINARY_DIR}/${target}_apk.dir"
            "-DNT_ACTIVITY_SOURCE=${CMAKE_CURRENT_FUNCTION_LIST_DIR}/android/NtActivity.java"
            "-DNT_APP_CONFIG=${_app_config}"
            "-DNT_ICON=${_icon}"
            "-DNT_STRIP=$<$<NOT:$<CONFIG:Debug>>:${CMAKE_STRIP}>"
            "-DNT_READELF=${CMAKE_READELF}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NtAndroidApk.cmake"
        DEPENDS ${target} ${_java_sources} ${_manifest_application} ${_icon} ${_extra_libs} ${_asset_files}
        COMMENT "Packaging ${target}.apk"
        VERBATIM)
endfunction()

# Qt's names, so that a Qt 6 project builds unchanged.
if(NOT COMMAND qt_add_executable)
    function(qt_add_executable target)
        cmake_parse_arguments(ARG "WIN32;MACOSX_BUNDLE;MANUAL_FINALIZATION" "" "" ${ARGN})
        set(_sources ${ARG_UNPARSED_ARGUMENTS})
        if(ANDROID)
            add_library(${target} SHARED ${_sources})
            # main() is called by the glue of Nt GUI, so nothing may hide it or the activity's entry point.
            set_target_properties(${target} PROPERTIES
                CXX_VISIBILITY_PRESET default
                POSITION_INDEPENDENT_CODE ON)
        else()
            set(_flags)
            if(ARG_WIN32)
                list(APPEND _flags WIN32)
            endif()
            if(ARG_MACOSX_BUNDLE)
                list(APPEND _flags MACOSX_BUNDLE)
            endif()
            add_executable(${target} ${_flags} ${_sources})
        endif()
        if(COMMAND nt_enable_automoc)
            nt_enable_automoc(${target})
        endif()
        if(COMMAND nt_enable_autorcc)
            nt_enable_autorcc(${target})
        endif()
        # As Qt does, the APK is made at the end of the directory, so that it sees the target properties set after
        # this call (QT_ANDROID_EXTRA_LIBS, QT_ANDROID_PACKAGE_SOURCE_DIR), unless the project made it itself.
        if(ANDROID AND NOT ARG_MANUAL_FINALIZATION)
            if(CMAKE_VERSION VERSION_LESS 3.19)
                nt_android_add_apk(${target})
            else()
                cmake_language(DEFER CALL qt_finalize_target ${target})
            endif()
        endif()
    endfunction()
endif()

if(NOT COMMAND qt6_add_executable)
    function(qt6_add_executable)
        qt_add_executable(${ARGV})
    endfunction()
endif()

if(NOT COMMAND qt_standard_project_setup)
    macro(qt_standard_project_setup)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_INCLUDE_CURRENT_DIR ON)
    endmacro()
endif()

if(NOT COMMAND qt_finalize_target)
    function(qt_finalize_target target)
        if(ANDROID AND TARGET ${target} AND NOT TARGET ${target}_apk)
            nt_android_add_apk(${target})
        endif()
    endfunction()
endif()
if(NOT COMMAND qt6_finalize_target)
    function(qt6_finalize_target)
        qt_finalize_target(${ARGV})
    endfunction()
endif()
