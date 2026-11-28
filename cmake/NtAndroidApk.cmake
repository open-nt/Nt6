# MIT License - see LICENSE
#
# Script mode: cmake -DNT_APK=... -DNT_LIBRARY=... -P NtAndroidApk.cmake
# Makes the signed APK of an application: its native library, loaded by org.nt.NtActivity (or a plain
# NativeActivity without a JDK), and any Java sources, permissions and <application> entries the application adds
# (NT_APP_CONFIG, written by nt_android_add_apk()), with its launcher icon if it has one (NT_ICON), its prebuilt
# libraries and its assets. Run by the <target>_apk target of nt_android_add_apk().
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

set(NT_APP_ACTIVITY "")
set(NT_APP_PERMISSIONS "")
set(NT_APP_JAVA_SOURCES "")
set(NT_APP_MANIFEST_APPLICATION "")
set(NT_APP_EXTRA_LIBS "")
set(NT_APP_ASSETS "")
if(DEFINED NT_APP_CONFIG AND EXISTS "${NT_APP_CONFIG}")
    include("${NT_APP_CONFIG}")
endif()

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
# The application's own Java needs the compiler: without it the APK would start an activity it does not contain.
if(NOT _nt_java_ok AND (NT_APP_JAVA_SOURCES OR NT_APP_ACTIVITY))
    message(FATAL_ERROR "${NT_APK}: the application has Java sources or its own activity, which need javac and the d8 of "
        "the SDK build tools")
endif()

file(REMOVE_RECURSE "${NT_WORK}")
file(MAKE_DIRECTORY "${NT_WORK}/lib/${NT_ABI}")
if(_nt_java_ok)
    file(MAKE_DIRECTORY "${NT_WORK}/classes" "${NT_WORK}/dex")
    execute_process(COMMAND "${NT_JAVAC}" -source 8 -target 8 -Xlint:-options -classpath "${NT_ANDROID_JAR}"
            -d "${NT_WORK}/classes" "${NT_ACTIVITY_SOURCE}" ${NT_APP_JAVA_SOURCES}
        RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(_result EQUAL 0)
        file(GLOB_RECURSE _nt_classes "${NT_WORK}/classes/*.class")
        execute_process(COMMAND "${NT_BUILD_TOOLS}/d8" --min-api ${NT_MIN_SDK} --lib "${NT_ANDROID_JAR}"
                --output "${NT_WORK}/dex" ${_nt_classes}
            RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    endif()
    if(_result EQUAL 0 AND EXISTS "${NT_WORK}/dex/classes.dex")
        set(_nt_activity "org.nt.NtActivity")
        if(NT_APP_ACTIVITY)
            set(_nt_activity "${NT_APP_ACTIVITY}")
        endif()
        set(_nt_has_code "true")
    elseif(NT_APP_JAVA_SOURCES OR NT_APP_ACTIVITY)
        message(FATAL_ERROR "The Java sources of ${NT_APK} could not be compiled:\n${_out}")
    else()
        message(WARNING "NtActivity could not be compiled; ${NT_APK} has no on-screen keyboard:\n${_out}")
    endif()
endif()

# Permissions: the network ones every Nt application has, then the application's, each once.
set(_nt_permissions android.permission.INTERNET android.permission.ACCESS_NETWORK_STATE
    android.permission.CHANGE_WIFI_MULTICAST_STATE ${NT_APP_PERMISSIONS})
list(REMOVE_DUPLICATES _nt_permissions)
set(_nt_permission_xml "")
foreach(_permission IN LISTS _nt_permissions)
    string(APPEND _nt_permission_xml "    <uses-permission android:name=\"${_permission}\"/>\n")
endforeach()
set(_nt_application_xml "")
if(NT_APP_MANIFEST_APPLICATION)
    file(READ "${NT_APP_MANIFEST_APPLICATION}" _nt_application_xml)
endif()

# The launcher icon, if the application has one: a PNG, compiled as the resource @mipmap/ic_launcher. It is filed
# as extra-extra-extra-high density, so that it is scaled down rather than up.
set(_nt_icon_attribute "")
set(_nt_resources)
if(NT_ICON)
    if(NOT EXISTS "${NT_ICON}")
        message(FATAL_ERROR "${NT_APK}: the icon ${NT_ICON} does not exist")
    endif()
    file(MAKE_DIRECTORY "${NT_WORK}/res/mipmap-xxxhdpi" "${NT_WORK}/compiled")
    configure_file("${NT_ICON}" "${NT_WORK}/res/mipmap-xxxhdpi/ic_launcher.png" COPYONLY)
    _nt_run("${NT_BUILD_TOOLS}/aapt2" compile -o "${NT_WORK}/compiled"
        "${NT_WORK}/res/mipmap-xxxhdpi/ic_launcher.png")
    file(GLOB _nt_resources "${NT_WORK}/compiled/*.flat")
    set(_nt_icon_attribute " android:icon=\"@mipmap/ic_launcher\"")
endif()

file(WRITE "${NT_WORK}/AndroidManifest.xml" "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"
    package=\"${NT_PACKAGE}\"
    android:versionCode=\"${NT_VERSION_CODE}\"
    android:versionName=\"${NT_VERSION_NAME}\">
    <uses-sdk android:minSdkVersion=\"${NT_MIN_SDK}\" android:targetSdkVersion=\"${NT_TARGET_SDK}\"/>
${_nt_permission_xml}    <application android:label=\"${_label}\"${_nt_icon_attribute} android:hasCode=\"${_nt_has_code}\" android:extractNativeLibs=\"true\">
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
${_nt_application_xml}    </application>
</manifest>
")
file(COPY "${NT_LIBRARY}" DESTINATION "${NT_WORK}/lib/${NT_ABI}")
# Outside a Debug build the library goes in without its debugging information, which can be most of its size.
if(NT_STRIP)
    get_filename_component(_nt_library_copy "${NT_LIBRARY}" NAME)
    _nt_run("${NT_STRIP}" --strip-unneeded "${NT_WORK}/lib/${NT_ABI}/${_nt_library_copy}")
endif()

set(_aapt2 "${NT_BUILD_TOOLS}/aapt2")
set(_aapt "${NT_BUILD_TOOLS}/aapt")
# Assets are stored uncompressed, so that QFile maps them rather than inflating them into memory.
set(_nt_asset_options)
if(NT_APP_ASSETS)
    set(_nt_asset_options -A "${NT_APP_ASSETS}" --no-compress)
endif()
_nt_run("${_aapt2}" link -o "${NT_WORK}/base.apk" -I "${NT_ANDROID_JAR}" --manifest "${NT_WORK}/AndroidManifest.xml"
    --min-sdk-version ${NT_MIN_SDK} --target-sdk-version ${NT_TARGET_SDK} ${_nt_asset_options} ${_nt_resources})
get_filename_component(_library_file "${NT_LIBRARY}" NAME)
set(_nt_library_entries "lib/${NT_ABI}/${_library_file}")
foreach(_extra IN LISTS NT_APP_EXTRA_LIBS)
    if(NOT EXISTS "${_extra}")
        message(FATAL_ERROR "${NT_APK}: the extra library ${_extra} does not exist")
    endif()
    # The library goes in under its SONAME, which is what the application's library names as needed: a prebuilt
    # libtensorflowlite_c.so, for one, calls itself libtensorflowlite_jni.so.
    get_filename_component(_extra_file "${_extra}" NAME)
    if(NT_READELF)
        execute_process(COMMAND "${NT_READELF}" -d "${_extra}" OUTPUT_VARIABLE _nt_dynamic ERROR_QUIET)
        if(_nt_dynamic MATCHES "\\(SONAME\\)[^[]*\\[([^]]+)\\]")
            set(_extra_file "${CMAKE_MATCH_1}")
        endif()
    endif()
    configure_file("${_extra}" "${NT_WORK}/lib/${NT_ABI}/${_extra_file}" COPYONLY)
    list(APPEND _nt_library_entries "lib/${NT_ABI}/${_extra_file}")
endforeach()
execute_process(COMMAND "${_aapt}" add base.apk ${_nt_library_entries}
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
