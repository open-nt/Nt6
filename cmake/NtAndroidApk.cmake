# MIT License - see LICENSE
#
# Script mode: cmake -DNT_APK=... -DNT_LIBRARY=... -P NtAndroidApk.cmake
# Makes the signed APK of an application whose only code is one native library, loaded by
# android.app.NativeActivity. Run by the <target>_apk target of nt_android_add_apk().
#
# Included normally (by NtAndroid.cmake) it does nothing.

if(NOT CMAKE_SCRIPT_MODE_FILE)
    return()
endif()

foreach(var NT_APK NT_LIBRARY NT_LIBRARY_NAME NT_ABI NT_PACKAGE NT_LABEL NT_MIN_SDK NT_TARGET_SDK NT_BUILD_TOOLS
        NT_ANDROID_JAR NT_KEYSTORE NT_WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "NtAndroidApk.cmake: ${var} is not set")
    endif()
endforeach()

macro(_nt_run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "failed (${_result}): ${ARGN}\n${_out}")
    endif()
endmacro()

# The label and the version go into XML.
string(REPLACE "&" "&amp;" _label "${NT_LABEL}")
string(REPLACE "<" "&lt;" _label "${_label}")
string(REPLACE "\"" "&quot;" _label "${_label}")

# The Java part: org.nt.NtActivity, which gives the keyboard a view to type into. Without a JDK the application runs as a
# plain NativeActivity, which has no keyboard.
set(_nt_activity "android.app.NativeActivity")
set(_nt_has_code "false")
find_program(NT_JAVAC javac)
if(NT_JAVAC AND NT_ACTIVITY_SOURCE AND EXISTS "${NT_BUILD_TOOLS}/d8")
    set(_nt_java_ok TRUE)
else()
    set(_nt_java_ok FALSE)
    message(STATUS "No javac (or no source of NtActivity): ${NT_APK} has no on-screen keyboard")
endif()

file(REMOVE_RECURSE "${NT_WORK}")
file(MAKE_DIRECTORY "${NT_WORK}/lib/${NT_ABI}")
if(_nt_java_ok)
    file(MAKE_DIRECTORY "${NT_WORK}/classes" "${NT_WORK}/dex")
    execute_process(COMMAND "${NT_JAVAC}" -source 8 -target 8 -Xlint:-options -classpath "${NT_ANDROID_JAR}"
            -d "${NT_WORK}/classes" "${NT_ACTIVITY_SOURCE}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(_result EQUAL 0)
        file(GLOB_RECURSE _nt_classes "${NT_WORK}/classes/*.class")
        execute_process(COMMAND "${NT_BUILD_TOOLS}/d8" --min-api ${NT_MIN_SDK} --lib "${NT_ANDROID_JAR}"
                --output "${NT_WORK}/dex" ${_nt_classes}
            RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    endif()
    if(_result EQUAL 0 AND EXISTS "${NT_WORK}/dex/classes.dex")
        set(_nt_activity "org.nt.NtActivity")
        set(_nt_has_code "true")
    else()
        message(WARNING "NtActivity could not be compiled; ${NT_APK} has no on-screen keyboard:\n${_out}")
    endif()
endif()

file(WRITE "${NT_WORK}/AndroidManifest.xml" "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"
    package=\"${NT_PACKAGE}\"
    android:versionCode=\"${NT_VERSION_CODE}\"
    android:versionName=\"${NT_VERSION_NAME}\">
    <uses-sdk android:minSdkVersion=\"${NT_MIN_SDK}\" android:targetSdkVersion=\"${NT_TARGET_SDK}\"/>
    <uses-permission android:name=\"android.permission.INTERNET\"/>
    <uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\"/>
    <uses-permission android:name=\"android.permission.CHANGE_WIFI_MULTICAST_STATE\"/>
    <application android:label=\"${_label}\" android:hasCode=\"${_nt_has_code}\" android:extractNativeLibs=\"true\">
        <activity android:name=\"${_nt_activity}\"
            android:label=\"${_label}\"
            android:exported=\"true\"
            android:configChanges=\"orientation|keyboardHidden|keyboard|screenSize|smallestScreenSize|screenLayout|density|uiMode|navigation\">
            <meta-data android:name=\"android.app.lib_name\" android:value=\"${NT_LIBRARY_NAME}\"/>
            <intent-filter>
                <action android:name=\"android.intent.action.MAIN\"/>
                <category android:name=\"android.intent.category.LAUNCHER\"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
")
file(COPY "${NT_LIBRARY}" DESTINATION "${NT_WORK}/lib/${NT_ABI}")

set(_aapt2 "${NT_BUILD_TOOLS}/aapt2")
set(_aapt "${NT_BUILD_TOOLS}/aapt")
_nt_run("${_aapt2}" link -o "${NT_WORK}/base.apk" -I "${NT_ANDROID_JAR}" --manifest "${NT_WORK}/AndroidManifest.xml"
    --min-sdk-version ${NT_MIN_SDK} --target-sdk-version ${NT_TARGET_SDK})
get_filename_component(_library_file "${NT_LIBRARY}" NAME)
execute_process(COMMAND "${_aapt}" add base.apk "lib/${NT_ABI}/${_library_file}"
    WORKING_DIRECTORY "${NT_WORK}" RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "aapt add failed:\n${_out}")
endif()
if(_nt_has_code)
    execute_process(COMMAND "${_aapt}" add "${NT_WORK}/base.apk" classes.dex
        WORKING_DIRECTORY "${NT_WORK}/dex" RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "aapt add classes.dex failed:\n${_out}")
    endif()
endif()
file(REMOVE "${NT_WORK}/aligned.apk")
_nt_run("${NT_BUILD_TOOLS}/zipalign" -f -p 4 "${NT_WORK}/base.apk" "${NT_WORK}/aligned.apk")

# The debug key of the SDK, made on first use.
if(NOT EXISTS "${NT_KEYSTORE}")
    get_filename_component(_key_dir "${NT_KEYSTORE}" DIRECTORY)
    file(MAKE_DIRECTORY "${_key_dir}")
    find_program(NT_KEYTOOL keytool REQUIRED)
    _nt_run("${NT_KEYTOOL}" -genkeypair -keystore "${NT_KEYSTORE}" -storepass android -keypass android
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US")
endif()
file(REMOVE "${NT_APK}")
_nt_run("${NT_BUILD_TOOLS}/apksigner" sign --ks "${NT_KEYSTORE}" --ks-pass pass:android --key-pass pass:android
    --ks-key-alias androiddebugkey --out "${NT_APK}" "${NT_WORK}/aligned.apk")
message(STATUS "Made ${NT_APK}")
