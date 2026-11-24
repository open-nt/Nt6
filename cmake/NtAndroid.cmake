# MIT License - see LICENSE
#
# Building applications for Android, the way a Qt 6 project does.
#
#   qt_add_executable(<name> [WIN32|MACOSX_BUNDLE] [MANUAL_FINALIZATION] <sources>...)
#   qt_standard_project_setup()
#   qt_finalize_target(<name>)
#   nt_android_add_apk(<target> [PACKAGE <id>] [LABEL <text>] [VERSION_CODE <n>] [VERSION_NAME <text>]
#                      [KEYSTORE <file>])
#
# On Android qt_add_executable() makes a shared library, which android.app.NativeActivity loads and which defines
# main(), and an <name>.apk next to it (the target <name>_apk, part of "all"). The package is signed with the debug
# key of the Android SDK (~/.android/debug.keystore, created when missing), so that `adb install` takes it. Elsewhere
# it is add_executable(). The Android SDK is found from ANDROID_SDK_ROOT, ANDROID_HOME or the directory above the
# NDK.

include(${CMAKE_CURRENT_LIST_DIR}/NtAndroidApk.cmake OPTIONAL)

set(NT_ANDROID_TARGET_SDK "" CACHE STRING "targetSdkVersion of the applications; the newest platform of the SDK when empty")

function(nt_android_add_apk target)
    cmake_parse_arguments(ARG "" "PACKAGE;LABEL;VERSION_CODE;VERSION_NAME;KEYSTORE" "" ${ARGN})
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
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NtAndroidApk.cmake"
        DEPENDS ${target}
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
        if(ANDROID AND NOT ARG_MANUAL_FINALIZATION)
            nt_android_add_apk(${target})
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
