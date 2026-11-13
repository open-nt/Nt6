cmake_minimum_required(VERSION 3.16)
# The version lives in the VERSION file at the root of the repository.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION NT_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
string(STRIP "${NT_VERSION}" NT_VERSION)
# project() takes only numeric versions: 6.0.0-rc1 is 6.0.0 with the suffix -rc1.
if(NOT NT_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-[0-9A-Za-z.]+)?$")
    message(FATAL_ERROR "VERSION '${NT_VERSION}' must be <major>.<minor>.<patch>[-<suffix>]")
endif()
set(NT_VERSION_NUMBER ${CMAKE_MATCH_1})
set(NT_VERSION_SUFFIX "${CMAKE_MATCH_2}")
project(Nt VERSION ${NT_VERSION_NUMBER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NT_ZEPHYR builds the headless subset for the Zephyr RTOS: Nt Core and Nt Network only, without the
# classes that need processes, shared libraries or a desktop. The Zephyr module in zephyr/ turns it
# on; see doc/zephyr.h.
option(NT_ZEPHYR "Build Nt Core and Nt Network for the Zephyr RTOS" OFF)
if(NT_ZEPHYR)
    set(_nt_hosted OFF)
else()
    set(_nt_hosted ON)
endif()

option(NT_BUILD_TESTS "Build tests" ${_nt_hosted})
option(NT_BUILD_EXAMPLES "Build examples" ${_nt_hosted})
option(NT_EXAMPLES_QT "Also build the widget examples against Qt 6 (the qt_examples target), to compare them with Nt" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtCompilerOptions.cmake)

if(NOT NT_ZEPHYR)
    find_package(Threads REQUIRED)
endif()

# The version macros come from the VERSION file.
configure_file(cmake/qconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/QtCore/qconfig.h @ONLY)

add_library(NtCore
    src/dispatcher.cpp
    src/qabstractanimation.cpp
    src/qabstracteventdispatcher.cpp
    src/qabstractitemmodel.cpp
    src/qabstractproxymodel.cpp
    src/qanimationgroup.cpp
    src/qbitarray.cpp
    src/qbuffer.cpp
    src/qbytearray.cpp
    src/qcborarray.cpp
    src/qcborcommon.cpp
    src/qcbormap.cpp
    src/qcborstreamreader.cpp
    src/qcborstreamwriter.cpp
    src/qcborvalue.cpp
    src/qcalendar.cpp
    src/qchar.cpp
    src/qchronotimer.cpp
    src/qcollator.cpp
    src/qcommandlineparser.cpp
    src/qconcatenatetablesproxymodel.cpp
    src/qcoreapplication.cpp
    src/qcoreevent.cpp
    src/qcryptographichash.cpp
    src/qdatastream.cpp
    src/qdatetime.cpp
    src/qdebug.cpp
    src/qdir.cpp
    src/qdiriterator.cpp
    src/qdirlisting.cpp
    src/qeasingcurve.cpp
    src/qeventloop.cpp
    src/qexception.cpp
    src/qfile.cpp
    src/qfileinfo.cpp
    src/qfileselector.cpp
    src/qfloat16.cpp
    src/qfutureinterface.cpp
    src/qfuturewatcher.cpp
    src/qidentityproxymodel.cpp
    src/qiodevice.cpp
    src/qitemselectionmodel.cpp
    src/qjsonarray.cpp
    src/qjsondocument.cpp
    src/qjsonobject.cpp
    src/qjsonobjectiterator.cpp
    src/qjsonparser.cpp
    src/qjsonvalue.cpp
    src/qjsonwriter.cpp
    src/qlibraryinfo.cpp
    src/qlocale.cpp
    src/qlockfile.cpp
    src/qlogging.cpp
    src/qmimebuiltin.cpp
    src/qmimedatabase.cpp
    src/qmimetype.cpp
    src/qloggingcategory.cpp
    src/qmetacontainer.cpp
    src/qmetaobject.cpp
    src/qmetatype.cpp
    src/qmetatype_convert.cpp
    src/qmetatype_streams.cpp
    src/qmimedata.cpp
    src/qnativeipckey.cpp
    src/qobject.cpp
    src/qobjectcleanuphandler.cpp
    src/qparallelanimationgroup.cpp
    src/qpauseanimation.cpp
    src/qpermissions.cpp
    src/qpropertyanimation.cpp
    src/qrandom.cpp
    src/qregularexpression.cpp
    src/qresource.cpp
    src/qrunnable.cpp
    src/qsavefile.cpp
    src/qsequentialanimationgroup.cpp
    src/qsettings.cpp
    src/qsettings_ini.cpp
    src/qsignalmapper.cpp
    src/qsocketnotifier.cpp
    src/qsortfilterproxymodel.cpp
    src/qstandardpaths.cpp
    src/qstring.cpp
    src/qstringconverter.cpp
    src/qstringlistmodel.cpp
    src/qsysinfo.cpp
    src/qtemporaryfile.cpp
    src/qtenvironmentvariables.cpp
    src/qtextboundaryfinder.cpp
    src/qtextstream.cpp
    src/qthread.cpp
    src/qthreadpool.cpp
    src/qthreadstorage.cpp
    src/qtimeline.cpp
    src/qtimer.cpp
    src/qtimezone.cpp
    src/qtranslator.cpp
    src/qtransposeproxymodel.cpp
    src/qurl.cpp
    src/quuid.cpp
    src/qvariant.cpp
    src/qvariantanimation.cpp
    src/qvariantiterable.cpp
    src/qversionnumber.cpp
    src/qxmlstream.cpp
    src/threaddata.cpp
)
# Zephyr has no processes, shared libraries, named shared memory, file change notification or statvfs().
if(NOT NT_ZEPHYR)
    target_sources(NtCore PRIVATE
        src/qfilesystemwatcher.cpp
        src/qlibrary.cpp
        src/qprocess.cpp
        src/qsharedmemory.cpp
        src/qstorageinfo.cpp
        src/qsystemsemaphore.cpp)
endif()
add_library(Nt::Core ALIAS NtCore)
add_library(Qt6::Core ALIAS NtCore)

target_include_directories(NtCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtCore>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include/nt6>
        $<INSTALL_INTERFACE:include/nt6/QtCore>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(NT_ZEPHYR)
    # The kernel's include directories, definitions and compiler flags come from zephyr_interface.
    target_link_libraries(NtCore PUBLIC zephyr_interface)
    add_dependencies(NtCore zephyr_generated_headers)
    # The C++ library of the Zephyr SDK has no std::mutex or std::condition_variable. This header
    # supplies them, for Nt and for the code that includes its headers.
    target_sources(NtCore PRIVATE
        src/zephyr/ntposix_zephyr.cpp
        src/zephyr/qstdthreads_zephyr.cpp)
    target_compile_options(NtCore PUBLIC
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/QtCore/qstdthreads_zephyr_p.h>")
    target_compile_definitions(NtCore PUBLIC
        QT_NO_FILESYSTEMWATCHER QT_NO_LIBRARY QT_NO_PROCESS QT_NO_SHAREDMEMORY QT_NO_STORAGEINFO
        QT_NO_SYSTEMSEMAPHORE)
    # The build is strict C++17, which hides the POSIX declarations of the C library unless asked for,
    # and Zephyr lacks some of them altogether (see src/zephyr/ntposix_zephyr_p.h).
    set(NT_ZEPHYR_PRIVATE_OPTIONS
        -D_GNU_SOURCE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/zephyr/ntposix_zephyr_p.h")
    target_compile_options(NtCore PRIVATE ${NT_ZEPHYR_PRIVATE_OPTIONS})
else()
    target_link_libraries(NtCore PUBLIC Threads::Threads)
    # QLibrary loads with dlopen(). QSharedMemory and QSystemSemaphore use POSIX shared memory and named
    # semaphores, which older C libraries keep in librt.
    target_link_libraries(NtCore PRIVATE ${CMAKE_DL_LIBS})
    find_library(NT_RT_LIBRARY rt)
    if(NT_RT_LIBRARY)
        target_link_libraries(NtCore PRIVATE ${NT_RT_LIBRARY})
    endif()
endif()

# Compressed resources (see ntrcc) need zlib or Zstandard, each used when it is found. Without
# them, resources compressed with that algorithm cannot be read and ntrcc cannot write them.
option(NT_USE_ZLIB "Read and write zlib-compressed resources with the system zlib" ${_nt_hosted})
option(NT_USE_ZSTD "Read and write Zstandard-compressed resources with the system libzstd" ${_nt_hosted})
set(NT_HAVE_ZLIB OFF)
set(NT_HAVE_ZSTD OFF)
if(NT_USE_ZLIB)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        set(NT_HAVE_ZLIB ON)
    else()
        message(WARNING "zlib was not found: building Nt without zlib-compressed resources")
    endif()
endif()
if(NT_USE_ZSTD)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NT_ZSTD QUIET libzstd)
    endif()
    if(NT_ZSTD_FOUND)
        set(NT_HAVE_ZSTD ON)
    else()
        message(WARNING "libzstd was not found: building Nt without Zstandard-compressed resources")
    endif()
endif()
# Links <target> with the compression libraries that were found.
function(nt_link_compression target)
    if(NT_HAVE_ZLIB)
        target_compile_definitions(${target} PRIVATE NT_HAVE_ZLIB)
        target_link_libraries(${target} PRIVATE ZLIB::ZLIB)
    endif()
    if(NT_HAVE_ZSTD)
        target_compile_definitions(${target} PRIVATE NT_HAVE_ZSTD)
        target_include_directories(${target} SYSTEM PRIVATE ${NT_ZSTD_INCLUDE_DIRS})
        target_link_directories(${target} PRIVATE ${NT_ZSTD_LIBRARY_DIRS})
        target_link_libraries(${target} PRIVATE ${NT_ZSTD_LIBRARIES})
    endif()
endfunction()
nt_link_compression(NtCore)

nt_set_compile_options(NtCore)

add_library(NtNetwork
    src/network/qabstractsocket.cpp
    src/network/qhostaddress.cpp
    src/network/qhostinfo.cpp
    src/network/qhttpheaders.cpp
    src/network/qnetworkaccessmanager.cpp
    src/network/qnetworkinterface.cpp
    src/network/qtcpserver.cpp
    src/network/qudpsocket.cpp
)
add_library(Nt::Network ALIAS NtNetwork)
add_library(Qt6::Network ALIAS NtNetwork)
target_include_directories(NtNetwork
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtNetwork>
        $<INSTALL_INTERFACE:include/nt6/QtNetwork>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtNetwork PUBLIC NtCore)
if(NT_ZEPHYR)
    target_compile_options(NtNetwork PRIVATE ${NT_ZEPHYR_PRIVATE_OPTIONS})
    target_sources(NtNetwork PRIVATE src/zephyr/qnetworkinterface_zephyr.cpp)
    target_compile_definitions(NtNetwork PUBLIC QT_NO_LOCALSERVER QT_NO_LOCALSOCKET)
else()
    # Zephyr has no Unix domain sockets.
    target_sources(NtNetwork PRIVATE
        src/network/qlocalserver.cpp
        src/network/qlocalsocket.cpp)
endif()

# TLS uses the system's OpenSSL, or Mbed TLS on Zephyr (CONFIG_NT_TLS). Without either, QSslSocket and https:
# are absent. The two libraries sit behind src/network/qtlsbackend_p.h.
option(NT_USE_OPENSSL "Build TLS support with the system OpenSSL" ${_nt_hosted})
set(NT_HAVE_OPENSSL OFF)
if(NT_USE_OPENSSL)
    find_package(OpenSSL 1.1.1 QUIET)
    if(OPENSSL_FOUND)
        set(NT_HAVE_OPENSSL ON)
    else()
        message(WARNING "OpenSSL 1.1.1 or newer was not found: building Nt Network without TLS")
    endif()
endif()
set(NT_HAVE_MBEDTLS OFF)
if(NT_ZEPHYR AND CONFIG_NT_TLS)
    set(NT_HAVE_MBEDTLS ON)
endif()
set(NT_HAVE_TLS OFF)
if(NT_HAVE_OPENSSL OR NT_HAVE_MBEDTLS)
    set(NT_HAVE_TLS ON)
    target_sources(NtNetwork PRIVATE
        src/network/qssl.cpp
        src/network/qsslserver.cpp
        src/network/qsslsocket.cpp)
    if(NT_HAVE_OPENSSL)
        target_sources(NtNetwork PRIVATE src/network/qtlsbackend_openssl.cpp)
        target_link_libraries(NtNetwork PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    else()
        # The interface library of Zephyr's Mbed TLS module.
        target_sources(NtNetwork PRIVATE src/network/qtlsbackend_mbedtls.cpp)
        target_link_libraries(NtNetwork PRIVATE mbedTLS)
    endif()
else()
    target_compile_definitions(NtNetwork PUBLIC QT_NO_SSL)
endif()
nt_set_compile_options(NtNetwork)

# Nt GPIO: on Linux through the GPIO character device, and on Zephyr when CONFIG_NT_GPIO is set.
if(NOT NT_ZEPHYR OR CONFIG_NT_GPIO)
add_library(NtGpio src/gpio/qgpioline.cpp)
add_library(Nt::Gpio ALIAS NtGpio)
target_include_directories(NtGpio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtGpio>
        $<INSTALL_INTERFACE:include/nt6/QtGpio>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gpio
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtGpio PUBLIC NtCore)
if(NT_ZEPHYR)
    target_compile_options(NtGpio PRIVATE ${NT_ZEPHYR_PRIVATE_OPTIONS})
    target_sources(NtGpio PRIVATE src/zephyr/qgpiobackend_zephyr.cpp)
else()
    target_sources(NtGpio PRIVATE src/gpio/qgpiobackend_linux.cpp)
endif()
nt_set_compile_options(NtGpio)
endif() # NOT NT_ZEPHYR OR CONFIG_NT_GPIO

# The other modules need a hosted system. This block ends after Nt Test.
if(NOT NT_ZEPHYR)

add_library(NtSerialPort
    src/serialport/qserialport.cpp
    src/serialport/qserialportbaud_linux.cpp
    src/serialport/qserialportengine_unix.cpp
    src/serialport/qserialportinfo.cpp
    src/serialport/qserialportinfo_linux.cpp
)
add_library(Nt::SerialPort ALIAS NtSerialPort)
add_library(Qt6::SerialPort ALIAS NtSerialPort)
target_include_directories(NtSerialPort
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtSerialPort>
        $<INSTALL_INTERFACE:include/nt6/QtSerialPort>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/serialport
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtSerialPort PUBLIC NtCore)
nt_set_compile_options(NtSerialPort)

add_library(NtSql
    src/sql/qsqldatabase.cpp
    src/sql/qsqldriver.cpp
    src/sql/qsqlerror.cpp
    src/sql/qsqlfield.cpp
    src/sql/qsqlquery.cpp
    src/sql/qsqlquerymodel.cpp
    src/sql/qsqlrecord.cpp
    src/sql/qsqlrelationaltablemodel.cpp
    src/sql/qsqlresult.cpp
    src/sql/qsqltablemodel.cpp
)
add_library(Nt::Sql ALIAS NtSql)
add_library(Qt6::Sql ALIAS NtSql)
target_include_directories(NtSql
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtSql>
        $<INSTALL_INTERFACE:include/nt6/QtSql>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sql
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtSql PUBLIC NtCore)
# The QSQLITE driver uses the system's SQLite. Without it, Nt SQL has no built-in driver.
option(NT_USE_SQLITE "Build the QSQLITE driver of Nt SQL with the system SQLite" ON)
set(NT_HAVE_SQLITE OFF)
if(NT_USE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        set(NT_HAVE_SQLITE ON)
    else()
        message(WARNING "SQLite 3 was not found: building Nt SQL without the QSQLITE driver")
    endif()
endif()
if(NT_HAVE_SQLITE)
    target_sources(NtSql PRIVATE src/sql/qsqlitedriver.cpp)
    target_compile_definitions(NtSql PRIVATE NT_HAVE_SQLITE)
    target_link_libraries(NtSql PRIVATE SQLite::SQLite3)
    # The table that a column of a query comes from is only known to an SQLite that was built to tell.
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES SQLite::SQLite3)
    check_cxx_source_compiles("#include <sqlite3.h>
int main() { return sqlite3_column_table_name(nullptr, 0) != nullptr; }" NT_SQLITE_COLUMN_METADATA)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(NT_SQLITE_COLUMN_METADATA)
        target_compile_definitions(NtSql PRIVATE NT_SQLITE_COLUMN_METADATA)
    endif()
endif()
nt_set_compile_options(NtSql)

add_library(NtDBus
    src/dbus/qdbusabstractadaptor.cpp
    src/dbus/qdbusabstractinterface.cpp
    src/dbus/qdbusargument.cpp
    src/dbus/qdbusconnection.cpp
    src/dbus/qdbusconnectioninterface.cpp
    src/dbus/qdbuscontext.cpp
    src/dbus/qdbuserror.cpp
    src/dbus/qdbusextratypes.cpp
    src/dbus/qdbusinterface.cpp
    src/dbus/qdbusmessage.cpp
    src/dbus/qdbusmetatype.cpp
    src/dbus/qdbusobjects.cpp
    src/dbus/qdbuspendingcall.cpp
    src/dbus/qdbusserver.cpp
    src/dbus/qdbusservicewatcher.cpp
    src/dbus/qdbustransport_unix.cpp
    src/dbus/qdbusunixfiledescriptor.cpp
    src/dbus/qdbusutil.cpp
    src/dbus/qdbusvirtualobject.cpp
)
add_library(Nt::DBus ALIAS NtDBus)
add_library(Qt6::DBus ALIAS NtDBus)
target_include_directories(NtDBus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtDBus>
        $<INSTALL_INTERFACE:include/nt6/QtDBus>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dbus
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtDBus PUBLIC NtCore)
nt_set_compile_options(NtDBus)

endif() # NOT NT_ZEPHYR

# WebSockets and the HTTP server need nothing but Nt Network, so Zephyr builds them when CONFIG_NT_HTTPSERVER asks.
if(NOT NT_ZEPHYR OR CONFIG_NT_HTTPSERVER)

add_library(NtWebSockets
    src/websockets/qmaskgenerator.cpp
    src/websockets/qwebsocket.cpp
    src/websockets/qwebsocketcorsauthenticator.cpp
    src/websockets/qwebsockethandshake.cpp
    src/websockets/qwebsockethandshakeoptions.cpp
    src/websockets/qwebsocketprotocol.cpp
    src/websockets/qwebsocketserver.cpp
)
add_library(Nt::WebSockets ALIAS NtWebSockets)
add_library(Qt6::WebSockets ALIAS NtWebSockets)
target_include_directories(NtWebSockets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtWebSockets>
        $<INSTALL_INTERFACE:include/nt6/QtWebSockets>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/websockets
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtWebSockets PUBLIC NtNetwork)
nt_set_compile_options(NtWebSockets)

add_library(NtHttpServer
    src/httpserver/qabstracthttpserver.cpp
    src/httpserver/qhttpserver.cpp
    src/httpserver/qhttpserverrequest.cpp
    src/httpserver/qhttpserverresponder.cpp
    src/httpserver/qhttpserverresponse.cpp
    src/httpserver/qhttpserverstream.cpp
    src/httpserver/qhttpserverwebsocketupgraderesponse.cpp
)
add_library(Nt::HttpServer ALIAS NtHttpServer)
add_library(Qt6::HttpServer ALIAS NtHttpServer)
target_include_directories(NtHttpServer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtHttpServer>
        $<INSTALL_INTERFACE:include/nt6/QtHttpServer>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver
        ${CMAKE_CURRENT_SOURCE_DIR}/src/websockets
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtHttpServer PUBLIC NtNetwork NtWebSockets)
nt_set_compile_options(NtHttpServer)

endif() # NOT NT_ZEPHYR OR CONFIG_NT_HTTPSERVER

if(NOT NT_ZEPHYR)

add_library(NtGui
    src/gui/platforms/offscreen/qoffscreenintegration.cpp
    src/gui/qaction.cpp
    src/gui/qactiongroup.cpp
    src/gui/qbackingstore.cpp
    src/gui/qbrush.cpp
    src/gui/qcolor.cpp
    src/gui/qcursor.cpp
    src/gui/qdesktopportal.cpp
    src/gui/qegl.cpp
    src/gui/qevent.cpp
    src/gui/qfont.cpp
    src/gui/qfontengine.cpp
    src/gui/qfontmetrics.cpp
    src/gui/qguiapplication.cpp
    src/gui/qimage.cpp
    src/gui/qimagebmp.cpp
    src/gui/qimageio.cpp
    src/gui/qimagejpeg.cpp
    src/gui/qimagepng.cpp
    src/gui/qkeysequence.cpp
    src/gui/qmatrix4x4.cpp
    src/gui/qoffscreensurface.cpp
    src/gui/qopenglcontext.cpp
    src/gui/qopenglfunctions.cpp
    src/gui/qpaintdevice.cpp
    src/gui/qpaintdevicewindow.cpp
    src/gui/qpaintengine.cpp
    src/gui/qpainter.cpp
    src/gui/qpainterpath.cpp
    src/gui/qpathclipper.cpp
    src/gui/qpalette.cpp
    src/gui/qpen.cpp
    src/gui/qpixmap.cpp
    src/gui/qquaternion.cpp
    src/gui/qplatform.cpp
    src/gui/qrasterizer.cpp
    src/gui/qrasterpaintengine.cpp
    src/gui/qrasterwindow.cpp
    src/gui/qregion.cpp
    src/gui/qscreen.cpp
    src/gui/qstylehints.cpp
    src/gui/qsurface.cpp
    src/gui/qsurfaceformat.cpp
    src/gui/qtextdocument.cpp
    src/gui/qtextformat.cpp
    src/gui/qtextlayout.cpp
    src/gui/qtextlayout_api.cpp
    src/gui/qtransform.cpp
    src/gui/qvectornd.cpp
    src/gui/qwindow.cpp
    src/gui/qwindowsysteminterface.cpp
)
add_library(Nt::Gui ALIAS NtGui)
add_library(Qt6::Gui ALIAS NtGui)
target_include_directories(NtGui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtGui>
        $<INSTALL_INTERFACE:include/nt6/QtGui>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtGui PUBLIC NtCore PRIVATE NtDBus)

# Text is rasterized with FreeType and the fonts are found with fontconfig. Without them the
# library still builds, but fonts have fixed metrics and draw nothing.
option(NT_USE_FREETYPE "Build text support of Nt GUI with FreeType and fontconfig" ON)
set(NT_HAVE_FREETYPE OFF)
if(NT_USE_FREETYPE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NT_FT QUIET freetype2 fontconfig)
    endif()
    if(NT_FT_FOUND)
        set(NT_HAVE_FREETYPE ON)
    else()
        message(WARNING "FreeType and fontconfig were not found: building Nt GUI without text rendering")
    endif()
endif()
if(NT_HAVE_FREETYPE)
    target_compile_definitions(NtGui PRIVATE NT_HAVE_FREETYPE)
    target_include_directories(NtGui SYSTEM PRIVATE ${NT_FT_INCLUDE_DIRS})
    target_link_directories(NtGui PRIVATE ${NT_FT_LIBRARY_DIRS})
    target_link_libraries(NtGui PRIVATE ${NT_FT_LIBRARIES})
endif()

# JPEG files are read and written with libjpeg. PNG and BMP need nothing but zlib, which is optional too.
option(NT_USE_JPEG "Read and write JPEG images in Nt GUI with libjpeg" ON)
set(NT_HAVE_JPEG OFF)
if(NT_USE_JPEG)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NT_JPEG QUIET libjpeg)
    endif()
    if(NT_JPEG_FOUND)
        set(NT_HAVE_JPEG ON)
    else()
        message(WARNING "libjpeg was not found: building Nt GUI without JPEG images")
    endif()
endif()
if(NT_HAVE_JPEG)
    target_compile_definitions(NtGui PRIVATE NT_HAVE_JPEG)
    target_include_directories(NtGui SYSTEM PRIVATE ${NT_JPEG_INCLUDE_DIRS})
    target_link_directories(NtGui PRIVATE ${NT_JPEG_LIBRARY_DIRS})
    target_link_libraries(NtGui PRIVATE ${NT_JPEG_LIBRARIES})
endif()
if(NT_HAVE_ZLIB)
    target_compile_definitions(NtGui PRIVATE NT_HAVE_ZLIB)
    target_link_libraries(NtGui PRIVATE ZLIB::ZLIB)
endif()

# The xcb platform talks to an X server (or XWayland); it is built when XCB is found.
option(NT_USE_XCB "Build the xcb platform of Nt GUI" ON)
set(NT_HAVE_XCB OFF)
if(NT_USE_XCB)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NT_XCB QUIET xcb)
    endif()
    if(NT_XCB_FOUND)
        set(NT_HAVE_XCB ON)
    else()
        message(WARNING "XCB was not found: building Nt GUI without the xcb platform")
    endif()
endif()
if(NT_HAVE_XCB)
    target_sources(NtGui PRIVATE
        src/gui/platforms/xcb/qxcbconnection.cpp
        src/gui/platforms/xcb/qxcbintegration.cpp
        src/gui/platforms/xcb/qxcbwindow.cpp)
    target_compile_definitions(NtGui PRIVATE NT_HAVE_XCB)
    target_include_directories(NtGui SYSTEM PRIVATE ${NT_XCB_INCLUDE_DIRS})
    target_link_directories(NtGui PRIVATE ${NT_XCB_LIBRARY_DIRS})
    target_link_libraries(NtGui PRIVATE ${NT_XCB_LIBRARIES})
endif()
nt_set_compile_options(NtGui)

add_library(NtWidgets
    src/widgets/qabstractbutton.cpp
    src/widgets/qabstractitemdelegate.cpp
    src/widgets/qabstractitemview.cpp
    src/widgets/qabstractscrollarea.cpp
    src/widgets/qabstractslider.cpp
    src/widgets/qabstractspinbox.cpp
    src/widgets/qapplication.cpp
    src/widgets/qboxlayout.cpp
    src/widgets/qcalendarwidget.cpp
    src/widgets/qbuttongroup.cpp
    src/widgets/qcheckbox.cpp
    src/widgets/qcolordialog.cpp
    src/widgets/qcommandlinkbutton.cpp
    src/widgets/qcompleter.cpp
    src/widgets/qdatetimeedit.cpp
    src/widgets/qcombobox.cpp
    src/widgets/qdial.cpp
    src/widgets/qdialog.cpp
    src/widgets/qdialogbuttonbox.cpp
    src/widgets/qdockwidget.cpp
    src/widgets/qerrormessage.cpp
    src/widgets/qfiledialog.cpp
    src/widgets/qfocusframe.cpp
    src/widgets/qfontdialog.cpp
    src/widgets/qfontcombobox.cpp
    src/widgets/qformlayout.cpp
    src/widgets/qframe.cpp
    src/widgets/qfusionstyle.cpp
    src/widgets/qgridlayout.cpp
    src/widgets/qgroupbox.cpp
    src/widgets/qheaderview.cpp
    src/widgets/qinputdialog.cpp
    src/widgets/qkeysequenceedit.cpp
    src/widgets/qlabel.cpp
    src/widgets/qlayout.cpp
    src/widgets/qlayoutitem.cpp
    src/widgets/qlcdnumber.cpp
    src/widgets/qlineedit.cpp
    src/widgets/qlistwidget.cpp
    src/widgets/qmainwindow.cpp
    src/widgets/qmdiarea.cpp
    src/widgets/qmdisubwindow.cpp
    src/widgets/qmenu.cpp
    src/widgets/qmenubar.cpp
    src/widgets/qmessagebox.cpp
    src/widgets/qprogressbar.cpp
    src/widgets/qprogressdialog.cpp
    src/widgets/qplaintextedit.cpp
    src/widgets/qpushbutton.cpp
    src/widgets/qradiobutton.cpp
    src/widgets/qrubberband.cpp
    src/widgets/qscrollarea.cpp
    src/widgets/qscrollbar.cpp
    src/widgets/qsizepolicy.cpp
    src/widgets/qsplashscreen.cpp
    src/widgets/qsizegrip.cpp
    src/widgets/qslider.cpp
    src/widgets/qspinbox.cpp
    src/widgets/qsplitter.cpp
    src/widgets/qstackedlayout.cpp
    src/widgets/qstackedwidget.cpp
    src/widgets/qstatusbar.cpp
    src/widgets/qstyle.cpp
    src/widgets/qstyleditemdelegate.cpp
    src/widgets/qstylefactory.cpp
    src/widgets/qstyleoption.cpp
    src/widgets/qtabbar.cpp
    src/widgets/qtablewidget.cpp
    src/widgets/qtextcursor.cpp
    src/widgets/qtextedit.cpp
    src/widgets/qtextbrowser.cpp
    src/widgets/qtoolbar.cpp
    src/widgets/qtoolbox.cpp
    src/widgets/qtoolbutton.cpp
    src/widgets/qtabwidget.cpp
    src/widgets/qtooltip.cpp
    src/widgets/qtreeview.cpp
    src/widgets/qtreewidget.cpp
    src/widgets/qwidget.cpp
    src/widgets/qwidgetaction.cpp
    src/widgets/qwidgetwindow.cpp
    src/widgets/qwizard.cpp
)
add_library(Nt::Widgets ALIAS NtWidgets)
add_library(Qt6::Widgets ALIAS NtWidgets)
target_include_directories(NtWidgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtWidgets>
        $<INSTALL_INTERFACE:include/nt6/QtWidgets>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtWidgets PUBLIC NtGui)
nt_set_compile_options(NtWidgets)

add_library(NtSvg
    src/svg/qsvgdocument.cpp
    src/svg/qsvggenerator.cpp
    src/svg/qsvgrenderer.cpp
    src/svg/qsvgxml.cpp
)
add_library(Nt::Svg ALIAS NtSvg)
add_library(Qt6::Svg ALIAS NtSvg)
target_include_directories(NtSvg
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtSvg>
        $<INSTALL_INTERFACE:include/nt6/QtSvg>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/svg
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)
target_link_libraries(NtSvg PUBLIC NtGui)
nt_set_compile_options(NtSvg)

add_library(NtSvgWidgets
    src/svgwidgets/qsvgwidget.cpp
)
add_library(Nt::SvgWidgets ALIAS NtSvgWidgets)
add_library(Qt6::SvgWidgets ALIAS NtSvgWidgets)
target_include_directories(NtSvgWidgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtSvgWidgets>
        $<INSTALL_INTERFACE:include/nt6/QtSvgWidgets>
)
target_link_libraries(NtSvgWidgets PUBLIC NtSvg NtWidgets)
nt_set_compile_options(NtSvgWidgets)

# Nt Charts draws its shapes with the rasterizer of Nt GUI, through its private header.
add_library(NtCharts
    src/charts/qabstractaxis.cpp
    src/charts/qabstractbarseries.cpp
    src/charts/qabstractseries.cpp
    src/charts/qareaseries.cpp
    src/charts/qbarcategoryaxis.cpp
    src/charts/qbarset.cpp
    src/charts/qchart.cpp
    src/charts/qchartengine.cpp
    src/charts/qchartview.cpp
    src/charts/qdatetimeaxis.cpp
    src/charts/qlegend.cpp
    src/charts/qpieseries.cpp
    src/charts/qvalueaxis.cpp
    src/charts/qxyseries.cpp
)
add_library(Nt::Charts ALIAS NtCharts)
add_library(Qt6::Charts ALIAS NtCharts)
target_include_directories(NtCharts
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtCharts>
        $<INSTALL_INTERFACE:include/nt6/QtCharts>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/charts
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)
target_link_libraries(NtCharts PUBLIC NtWidgets)
nt_set_compile_options(NtCharts)

# Nt OpenGL: QOpenGLWindow, QOpenGLPaintDevice and its paint engine, and framebuffer objects. The engine flattens and
# strokes paths with the rasterizer of Nt GUI, through its private headers. OpenGL itself is loaded at run time.
add_library(NtOpenGL
    src/opengl/qopenglframebufferobject.cpp
    src/opengl/qopenglhelpers.cpp
    src/opengl/qopenglpaintdevice.cpp
    src/opengl/qopenglpaintengine.cpp
    src/opengl/qopenglwindow.cpp
)
add_library(Nt::OpenGL ALIAS NtOpenGL)
add_library(Qt6::OpenGL ALIAS NtOpenGL)
target_include_directories(NtOpenGL
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtOpenGL>
        $<INSTALL_INTERFACE:include/nt6/QtOpenGL>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/opengl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)
target_link_libraries(NtOpenGL PUBLIC NtGui)
nt_set_compile_options(NtOpenGL)

# Nt Qml: the JavaScript engine and, later, the QML engine. The interpreter reports script errors as C++
# exceptions internally, which never leave the library.
add_library(NtQml
    src/qml/qjsbuiltins.cpp
    src/qml/qjsdate.cpp
    src/qml/qjsengine.cpp
    src/qml/qjsinterp.cpp
    src/qml/qjsparser.cpp
    src/qml/qjsqobject.cpp
    src/qml/qjsruntime.cpp
    src/qml/qjsstring.cpp
    src/qml/qjsvalue.cpp
    src/qml/qqmlcomponent.cpp
    src/qml/qqmlcreator.cpp
    src/qml/qqmlengine.cpp
    src/qml/qqmltypes.cpp
)
add_library(Nt::Qml ALIAS NtQml)
add_library(Qt6::Qml ALIAS NtQml)
target_include_directories(NtQml
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtQml>
        $<INSTALL_INTERFACE:include/nt6/QtQml>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qml
)
target_link_libraries(NtQml PUBLIC NtCore)
nt_set_compile_options(NtQml)

# Nt FM: an FM synthesiser for game sound effects. The default sound output on Linux loads ALSA at run time,
# so the module needs no audio headers to build.
add_library(NtFM
    src/fm/qfmbank.cpp
    src/fm/qfmengine.cpp
    src/fm/qfmmidi.cpp
    src/fm/qfmmusic.cpp
    src/fm/qfmopl.cpp
    src/fm/qfmpatch.cpp
    src/fm/qfmsynth.cpp
)
add_library(Nt::FM ALIAS NtFM)
target_include_directories(NtFM
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtFM>
        $<INSTALL_INTERFACE:include/nt6/QtFM>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fm
)
target_link_libraries(NtFM PUBLIC NtCore)
# VGZ music is gzipped VGM.
if(NT_HAVE_ZLIB)
    target_compile_definitions(NtFM PRIVATE NT_HAVE_ZLIB)
    target_link_libraries(NtFM PRIVATE ZLIB::ZLIB)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NtFM PRIVATE src/fm/qfmoutput_alsa.cpp)
else()
    target_sources(NtFM PRIVATE src/fm/qfmoutput_null.cpp)
endif()
nt_set_compile_options(NtFM)

# Nt FM QML types: FmSound, FmMusic and the rest, through Nt Qml.
add_library(NtFMQml src/fmqml/qfmqml.cpp)
add_library(Nt::FMQml ALIAS NtFMQml)
target_include_directories(NtFMQml
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtFMQml>
        $<INSTALL_INTERFACE:include/nt6/QtFMQml>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fm
)
target_link_libraries(NtFMQml PUBLIC NtFM NtQml)
nt_set_compile_options(NtFMQml)

# Nt Gamepad: the API of Qt Gamepad from Qt 5, on evdev. Key navigation sends key events through Nt GUI.
add_library(NtGamepad
    src/gamepad/qgamepad.cpp
    src/gamepad/qgamepadkeynavigation.cpp
    src/gamepad/qgamepadmanager.cpp
    src/gamepad/qgamepadmapping.cpp
    src/gamepad/qgamepadbackend_linux.cpp
)
add_library(Nt::Gamepad ALIAS NtGamepad)
target_include_directories(NtGamepad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtGamepad>
        $<INSTALL_INTERFACE:include/nt6/QtGamepad>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad
)
target_link_libraries(NtGamepad PUBLIC NtCore PRIVATE NtGui)
nt_set_compile_options(NtGamepad)

# Nt Multimedia: the audio and camera parts of Qt Multimedia. Linux uses ALSA, loaded at run time, so the
# library builds without its headers, and Video4Linux2 for cameras; other platforms have no devices yet.
add_library(NtMultimedia
    src/multimedia/qaudiobuffer.cpp
    src/multimedia/qaudiodecoder.cpp
    src/multimedia/qaudiodevice.cpp
    src/multimedia/qaudioformat.cpp
    src/multimedia/qaudioinput.cpp
    src/multimedia/qaudiooutput.cpp
    src/multimedia/qaudiosink.cpp
    src/multimedia/qaudiosource.cpp
    src/multimedia/qaudiostream.cpp
    src/multimedia/qcamera.cpp
    src/multimedia/qcameradevice.cpp
    src/multimedia/qimagecapture.cpp
    src/multimedia/qmediacapturesession.cpp
    src/multimedia/qmediadevices.cpp
    src/multimedia/qmediaformat.cpp
    src/multimedia/qmediametadata.cpp
    src/multimedia/qmediaplayer.cpp
    src/multimedia/qmediarecorder.cpp
    src/multimedia/qsoundeffect.cpp
    src/multimedia/qtaudio.cpp
    src/multimedia/qvideoframe.cpp
    src/multimedia/qvideoframeformat.cpp
    src/multimedia/qvideosink.cpp
    src/multimedia/qwavedecoder.cpp
)
add_library(Nt::Multimedia ALIAS NtMultimedia)
add_library(Qt6::Multimedia ALIAS NtMultimedia)
target_include_directories(NtMultimedia
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtMultimedia>
        $<INSTALL_INTERFACE:include/nt6/QtMultimedia>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia
)
target_link_libraries(NtMultimedia PUBLIC NtCore NtGui)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NtMultimedia PRIVATE src/multimedia/qaudiobackend_alsa.cpp src/multimedia/qvideobackend_v4l2.cpp)
else()
    target_sources(NtMultimedia PRIVATE src/multimedia/qaudiobackend_null.cpp src/multimedia/qvideobackend_null.cpp)
endif()
nt_set_compile_options(NtMultimedia)

# Nt Multimedia Widgets: QVideoWidget, which draws video frames with QPainter.
add_library(NtMultimediaWidgets
    src/multimediawidgets/qvideowidget.cpp
)
add_library(Nt::MultimediaWidgets ALIAS NtMultimediaWidgets)
add_library(Qt6::MultimediaWidgets ALIAS NtMultimediaWidgets)
target_include_directories(NtMultimediaWidgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtMultimediaWidgets>
        $<INSTALL_INTERFACE:include/nt6/QtMultimediaWidgets>
)
target_link_libraries(NtMultimediaWidgets PUBLIC NtMultimedia NtWidgets)
nt_set_compile_options(NtMultimediaWidgets)

add_library(NtConcurrent
    src/concurrent/qtconcurrentthreadengine.cpp
)
add_library(Nt::Concurrent ALIAS NtConcurrent)
add_library(Qt6::Concurrent ALIAS NtConcurrent)
target_include_directories(NtConcurrent
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtConcurrent>
        $<INSTALL_INTERFACE:include/nt6/QtConcurrent>
)
target_link_libraries(NtConcurrent PUBLIC NtCore)
nt_set_compile_options(NtConcurrent)

add_library(NtTest
    src/test/qsignalspy.cpp
    src/test/qtestcase.cpp
    src/test/qtesteventloop.cpp
    src/test/qtestlog.cpp
    src/test/qtesttostring.cpp
)
add_library(Nt::Test ALIAS NtTest)
add_library(Qt6::Test ALIAS NtTest)
target_include_directories(NtTest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtTest>
        $<INSTALL_INTERFACE:include/nt6/QtTest>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test
)
target_link_libraries(NtTest PUBLIC NtCore)
nt_set_compile_options(NtTest)

endif() # NOT NT_ZEPHYR

# Moc replacement and its CMake helpers. A cross build runs the tools on the build machine, so it
# uses ntmoc and ntrcc built for that (see cmake/NtHostTools.cmake).
if(CMAKE_CROSSCOMPILING)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtHostTools.cmake)
else()
    add_executable(ntmoc tools/ntmoc/main.cpp)
    target_compile_definitions(ntmoc PRIVATE NT_VERSION_STRING="${NT_VERSION}")
    nt_set_compile_options(ntmoc)
endif()
add_executable(Nt::moc ALIAS ntmoc)
add_executable(Qt6::moc ALIAS ntmoc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtMoc.cmake)
nt_moc_library(NtCore include/QtCore)
if(NOT NT_HAVE_TLS)
    set(NT_MOC_DEFINES -DQT_NO_SSL)
endif()
nt_moc_library(NtNetwork include/QtNetwork)
set(NT_MOC_DEFINES)
if(NOT NT_ZEPHYR OR CONFIG_NT_GPIO)
nt_moc_library(NtGpio include/QtGpio)
endif()
if(NOT NT_ZEPHYR)
nt_moc_library(NtSerialPort include/QtSerialPort)
nt_moc_library(NtSql include/QtSql)
nt_moc_library(NtDBus include/QtDBus)
endif() # NOT NT_ZEPHYR
if(NOT NT_ZEPHYR OR CONFIG_NT_HTTPSERVER)
if(NOT NT_HAVE_TLS)
    set(NT_MOC_DEFINES -DQT_NO_SSL)
endif()
nt_moc_library(NtWebSockets include/QtWebSockets)
set(NT_MOC_DEFINES)
nt_moc_library(NtHttpServer include/QtHttpServer)
endif() # NOT NT_ZEPHYR OR CONFIG_NT_HTTPSERVER
if(NOT NT_ZEPHYR)
nt_moc_library(NtGui include/QtGui src/gui/qpa)
nt_moc_library(NtWidgets include/QtWidgets)
nt_moc_library(NtSvg include/QtSvg)
nt_moc_library(NtSvgWidgets include/QtSvgWidgets)
nt_moc_library(NtCharts include/QtCharts)
nt_moc_library(NtQml include/QtQml src/qml)
nt_moc_library(NtFM include/QtFM)
nt_moc_library(NtFMQml include/QtFMQml)
nt_moc_library(NtGamepad include/QtGamepad)
nt_moc_library(NtMultimedia include/QtMultimedia)
nt_moc_library(NtMultimediaWidgets include/QtMultimediaWidgets)
nt_moc_library(NtOpenGL include/QtOpenGL)
endif() # NOT NT_ZEPHYR

# Resource compiler and its CMake helpers.
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(ntrcc tools/ntrcc/main.cpp)
    target_compile_definitions(ntrcc PRIVATE NT_VERSION_STRING="${NT_VERSION}")
    nt_link_compression(ntrcc)
    nt_set_compile_options(ntrcc)
endif()
add_executable(Nt::rcc ALIAS ntrcc)
add_executable(Qt6::rcc ALIAS ntrcc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtRcc.cmake)

# The rest is the documentation, the packages, the tests and the examples, none of which is cross built.
if(CMAKE_CROSSCOMPILING)
    return()
endif()

# Documentation generator; build the docs with the `docs` target.
add_executable(ndoc tools/ndoc/main.cpp)
add_executable(Nt::ndoc ALIAS ndoc)
target_compile_definitions(ndoc PRIVATE NT_VERSION_STRING="${NT_VERSION}")
nt_set_compile_options(ndoc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtDoc.cmake)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    nt_add_docs(docs CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/doc/ndoc.conf STRICT REQUIRE_COMPLETE)

    # The README badge is published next to the documentation.
    add_custom_target(badge
        COMMAND ${CMAKE_COMMAND}
            -DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/VERSION
            -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/doc/html/badges
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtBadge.cmake
        COMMENT "Writing version badge"
        VERBATIM)
    add_dependencies(docs badge)
endif()

# QLibraryInfo::path() reports the directories of the installation.
include(GNUInstallDirs)
set(NT_LIBRARYINFO_SOURCE src/qlibraryinfo)
set_source_files_properties(${NT_LIBRARYINFO_SOURCE}.cpp PROPERTIES COMPILE_DEFINITIONS
    "NT_INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\";NT_INSTALL_LIBDIR=\"${CMAKE_INSTALL_LIBDIR}\";NT_INSTALL_BINDIR=\"${CMAKE_INSTALL_BINDIR}\";NT_INSTALL_LIBEXECDIR=\"${CMAKE_INSTALL_LIBEXECDIR}\";NT_INSTALL_INCLUDEDIR=\"${CMAKE_INSTALL_INCLUDEDIR}\";NT_INSTALL_DATADIR=\"${CMAKE_INSTALL_DATADIR}\";NT_INSTALL_SYSCONFDIR=\"${CMAKE_INSTALL_SYSCONFDIR}\"")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtInstall.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtPackaging.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtSnap.cmake)
endif()

if(NT_BUILD_TESTS)
    enable_testing()

    add_executable(nt_tests tests/test_main.cpp)
    target_link_libraries(nt_tests PRIVATE Nt::Core)
    add_test(NAME nt_tests COMMAND nt_tests)

    add_executable(nt_core_tests tests/test_core.cpp)
    target_link_libraries(nt_core_tests PRIVATE Nt::Core)
    add_test(NAME nt_core_tests COMMAND nt_core_tests)

    foreach(suite animation cbor coreapi coremore datetime filesystem globals mime models storage streams text types xmlstream)
        add_executable(nt_${suite}_tests tests/test_${suite}.cpp)
        target_link_libraries(nt_${suite}_tests PRIVATE Nt::Core)
        add_test(NAME nt_${suite}_tests COMMAND nt_${suite}_tests)
    endforeach()

    add_executable(nt_core_value_tests tests/test_core_values.cpp)
    target_link_libraries(nt_core_value_tests PRIVATE Nt::Core)
    target_compile_definitions(nt_core_value_tests PRIVATE NT_EXPECTED_VERSION="${PROJECT_VERSION}")
    add_test(NAME nt_core_value_tests COMMAND nt_core_value_tests)

    add_executable(nt_json_tests tests/test_json.cpp)
    target_link_libraries(nt_json_tests PRIVATE Nt::Core)
    nt_set_compile_options(nt_json_tests)
    add_test(NAME nt_json_tests COMMAND nt_json_tests)

    add_executable(nt_transitive_include_tests tests/test_transitive_includes.cpp)
    target_link_libraries(nt_transitive_include_tests PRIVATE Nt::Core)
    nt_set_compile_options(nt_transitive_include_tests)
    add_test(NAME nt_transitive_include_tests COMMAND nt_transitive_include_tests)

    if(NT_HAVE_OPENSSL)
        add_executable(nt_ssl_tests tests/test_ssl.cpp)
        target_include_directories(nt_ssl_tests PRIVATE tests)
        target_link_libraries(nt_ssl_tests PRIVATE Nt::Network OpenSSL::SSL OpenSSL::Crypto)
        add_test(NAME nt_ssl_tests COMMAND nt_ssl_tests)
        nt_set_compile_options(nt_ssl_tests)
    endif()

    add_executable(nt_network_tests tests/test_network.cpp)
    target_link_libraries(nt_network_tests PRIVATE Nt::Network)
    add_test(NAME nt_network_tests COMMAND nt_network_tests)

    add_executable(nt_sql_tests tests/test_sql.cpp)
    target_link_libraries(nt_sql_tests PRIVATE Nt::Sql)
    add_test(NAME nt_sql_tests COMMAND nt_sql_tests)
    nt_set_compile_options(nt_sql_tests)

    # The tests reach the private headers to check what a pseudo terminal hides.
    add_executable(nt_serialport_tests tests/test_serialport.cpp)
    target_include_directories(nt_serialport_tests PRIVATE src/serialport)
    target_link_libraries(nt_serialport_tests PRIVATE Nt::SerialPort)
    add_test(NAME nt_serialport_tests COMMAND nt_serialport_tests)
    nt_set_compile_options(nt_serialport_tests)

    # The GPIO tests replace the platform backend with one of their own.
    add_executable(nt_gpio_tests tests/test_gpio.cpp)
    target_include_directories(nt_gpio_tests PRIVATE src/gpio)
    target_link_libraries(nt_gpio_tests PRIVATE Nt::Gpio)
    add_test(NAME nt_gpio_tests COMMAND nt_gpio_tests)
    nt_set_compile_options(nt_gpio_tests)

    # The QProcess tests start a helper program built next to them.
    add_executable(nt_process_helper tests/process_helper.cpp)
    nt_set_compile_options(nt_process_helper)
    add_executable(nt_process_tests tests/test_process.cpp)
    target_include_directories(nt_process_tests PRIVATE src)
    target_link_libraries(nt_process_tests PRIVATE Nt::Core)
    target_compile_definitions(nt_process_tests PRIVATE NT_PROCESS_HELPER="$<TARGET_FILE:nt_process_helper>")
    add_dependencies(nt_process_tests nt_process_helper)
    add_test(NAME nt_process_tests COMMAND nt_process_tests)
    nt_set_compile_options(nt_process_tests)

    add_executable(nt_dirlock_tests tests/test_dirlock.cpp)
    target_link_libraries(nt_dirlock_tests PRIVATE Nt::Core)
    add_test(NAME nt_dirlock_tests COMMAND nt_dirlock_tests)
    nt_set_compile_options(nt_dirlock_tests)

    add_executable(nt_filesystemwatcher_tests tests/test_filesystemwatcher.cpp)
    target_link_libraries(nt_filesystemwatcher_tests PRIVATE Nt::Core)
    add_test(NAME nt_filesystemwatcher_tests COMMAND nt_filesystemwatcher_tests)
    nt_set_compile_options(nt_filesystemwatcher_tests)

    add_executable(nt_settings_tests tests/test_settings.cpp)
    target_link_libraries(nt_settings_tests PRIVATE Nt::Core)
    add_test(NAME nt_settings_tests COMMAND nt_settings_tests)
    nt_set_compile_options(nt_settings_tests)

    add_test(NAME ndoc_tests
             COMMAND ${CMAKE_COMMAND}
                 -DNDOC=$<TARGET_FILE:ndoc>
                 -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/ndoc
                 -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/ndoc_tests
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ndoc/check.cmake)

    add_test(NAME badge_tests
             COMMAND ${CMAKE_COMMAND}
                 -DBADGE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtBadge.cmake
                 -DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/VERSION
                 -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/badge_tests
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/badge/check.cmake)

    nt_wrap_cpp(moc_test_sources tests/moc_classes.h)
    add_executable(nt_moc_tests tests/test_moc.cpp ${moc_test_sources})
    target_include_directories(nt_moc_tests PRIVATE tests)
    target_link_libraries(nt_moc_tests PRIVATE Nt::Core)
    add_test(NAME nt_moc_tests COMMAND nt_moc_tests)

    add_executable(nt_test_tests tests/test_qtest.cpp)
    target_link_libraries(nt_test_tests PRIVATE Nt::Test)
    nt_enable_automoc(nt_test_tests)
    add_test(NAME nt_test_tests COMMAND nt_test_tests)

    # Resources compiled with ntrcc in each of the ways the CMake helpers offer: from a .qrc file,
    # from a list of files, with AUTORCC, and as a binary .rcc file read at run time.
    nt_add_resources(resource_test_sources tests/resources/test.qrc)
    add_executable(nt_resource_tests tests/test_resources.cpp tests/resources/autorcc.qrc ${resource_test_sources})
    nt_add_resources(nt_resource_tests listed PREFIX /listed BASE tests/resources FILES
                     tests/resources/hello.txt tests/resources/icons/open.svg)
    nt_enable_autorcc(nt_resource_tests)
    nt_add_binary_resources(nt_resource_tests_rcc tests/resources/binary.qrc
                            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/resource_tests.rcc)
    add_dependencies(nt_resource_tests nt_resource_tests_rcc)
    target_include_directories(nt_resource_tests PRIVATE src)
    target_link_libraries(nt_resource_tests PRIVATE Nt::Core)
    target_compile_definitions(nt_resource_tests PRIVATE
        NT_RESOURCE_RCC="${CMAKE_CURRENT_BINARY_DIR}/resource_tests.rcc"
        NT_RCC="$<TARGET_FILE:ntrcc>"
        NT_RESOURCE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/resources")
    add_test(NAME nt_resource_tests COMMAND nt_resource_tests)

    # QLibrary, QSharedMemory and QSystemSemaphore. The library that the QLibrary tests load is built next to
    # the test.
    add_library(nt_test_plugin MODULE tests/test_library_plugin.cpp)
    set_target_properties(nt_test_plugin PROPERTIES PREFIX "lib" SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_plugins)
    add_executable(nt_ipc_tests tests/test_ipc.cpp)
    target_link_libraries(nt_ipc_tests PRIVATE Nt::Core)
    target_compile_definitions(nt_ipc_tests PRIVATE NT_TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/test_plugins")
    add_dependencies(nt_ipc_tests nt_test_plugin)
    add_test(NAME nt_ipc_tests COMMAND nt_ipc_tests)
    if(NT_HAVE_ZLIB)
        target_compile_definitions(nt_resource_tests PRIVATE NT_HAVE_ZLIB)
    endif()
    if(NT_HAVE_ZSTD)
        target_compile_definitions(nt_resource_tests PRIVATE NT_HAVE_ZSTD)
    endif()
    nt_set_compile_options(nt_resource_tests)

    add_executable(nt_logging_tests tests/test_logging.cpp tests/test_logging_nooutput.cpp)
    target_link_libraries(nt_logging_tests PRIVATE Nt::Core)
    add_test(NAME nt_logging_tests COMMAND nt_logging_tests)
    nt_set_compile_options(nt_logging_tests)

    add_executable(nt_thread_tests tests/test_threads.cpp)
    target_link_libraries(nt_thread_tests PRIVATE Nt::Core)
    add_test(NAME nt_thread_tests COMMAND nt_thread_tests)
    nt_set_compile_options(nt_thread_tests)

    add_executable(nt_core_utils_tests tests/test_core_utils.cpp)
    target_link_libraries(nt_core_utils_tests PRIVATE Nt::Core)
    add_test(NAME nt_core_utils_tests COMMAND nt_core_utils_tests)
    nt_set_compile_options(nt_core_utils_tests)
    add_executable(nt_future_tests tests/test_futures.cpp)
    target_link_libraries(nt_future_tests PRIVATE Nt::Core)
    add_test(NAME nt_future_tests COMMAND nt_future_tests)
    nt_set_compile_options(nt_future_tests)

    add_executable(nt_eventdispatcher_tests tests/test_eventdispatcher.cpp)
    target_link_libraries(nt_eventdispatcher_tests PRIVATE Nt::Core)
    add_test(NAME nt_eventdispatcher_tests COMMAND nt_eventdispatcher_tests)
    nt_set_compile_options(nt_eventdispatcher_tests)

    add_executable(nt_concurrent_tests tests/test_concurrent.cpp)
    target_link_libraries(nt_concurrent_tests PRIVATE Nt::Concurrent)
    add_test(NAME nt_concurrent_tests COMMAND nt_concurrent_tests)
    nt_set_compile_options(nt_concurrent_tests)

    nt_wrap_cpp(meta_test_sources tests/meta_classes.h)
    add_executable(nt_meta_tests tests/test_meta.cpp ${meta_test_sources})
    target_include_directories(nt_meta_tests PRIVATE tests)
    target_link_libraries(nt_meta_tests PRIVATE Nt::Core)
    add_test(NAME nt_meta_tests COMMAND nt_meta_tests)
    nt_enable_automoc(nt_meta_tests)
    target_compile_definitions(nt_meta_tests PRIVATE NT_META_FEATURE)
    nt_set_compile_options(nt_meta_tests)

    # The D-Bus tests start a private bus with dbus-daemon, and skip those tests without it.
    find_program(NT_DBUS_DAEMON_EXECUTABLE dbus-daemon)
    find_program(NT_DBUS_SEND_EXECUTABLE dbus-send)
    nt_wrap_cpp(dbus_test_sources tests/dbus_classes.h)
    add_executable(nt_dbus_tests tests/test_dbus.cpp ${dbus_test_sources})
    target_include_directories(nt_dbus_tests PRIVATE tests src/dbus)
    target_link_libraries(nt_dbus_tests PRIVATE Nt::DBus Nt::Test)
    if(NT_DBUS_DAEMON_EXECUTABLE)
        target_compile_definitions(nt_dbus_tests PRIVATE NT_DBUS_DAEMON="${NT_DBUS_DAEMON_EXECUTABLE}")
    endif()
    if(NT_DBUS_SEND_EXECUTABLE)
        target_compile_definitions(nt_dbus_tests PRIVATE NT_DBUS_SEND="${NT_DBUS_SEND_EXECUTABLE}")
    endif()
    add_test(NAME nt_dbus_tests COMMAND nt_dbus_tests)
    nt_set_compile_options(nt_dbus_tests)

    add_executable(nt_websockets_tests tests/test_websockets.cpp)
    target_link_libraries(nt_websockets_tests PRIVATE Nt::WebSockets)
    add_test(NAME nt_websockets_tests COMMAND nt_websockets_tests)
    nt_set_compile_options(nt_websockets_tests)

    add_executable(nt_httpserver_tests tests/test_httpserver.cpp)
    target_link_libraries(nt_httpserver_tests PRIVATE Nt::HttpServer)
    add_test(NAME nt_httpserver_tests COMMAND nt_httpserver_tests)
    nt_set_compile_options(nt_httpserver_tests)

    # The TLS tests make their certificates with OpenSSL (tests/test_certificates.h).
    foreach(tls_test nt_websockets_tests nt_httpserver_tests)
        if(NT_HAVE_OPENSSL)
            target_include_directories(${tls_test} PRIVATE tests)
            target_link_libraries(${tls_test} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
            target_compile_definitions(${tls_test} PRIVATE NT_TEST_TLS)
        endif()
    endforeach()

    add_executable(nt_variant_tests tests/test_variant.cpp)
    target_link_libraries(nt_variant_tests PRIVATE Nt::Core)
    add_test(NAME nt_variant_tests COMMAND nt_variant_tests)
    nt_set_compile_options(nt_variant_tests)

    # The GUI tests run on the offscreen platform, so they need no display. They reach the private
    # platform headers to play the window system.
    add_executable(nt_gui_tests tests/test_gui.cpp)
    target_include_directories(nt_gui_tests PRIVATE src/gui)
    target_link_libraries(nt_gui_tests PRIVATE Nt::Gui)
    if(NT_HAVE_ZLIB)
        target_compile_definitions(nt_gui_tests PRIVATE NT_HAVE_ZLIB)
    endif()
    if(NT_HAVE_JPEG)
        target_compile_definitions(nt_gui_tests PRIVATE NT_HAVE_JPEG)
    endif()
    add_test(NAME nt_gui_tests COMMAND nt_gui_tests)
    set_tests_properties(nt_gui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_gui_tests)

    # The widget tests run on the offscreen platform too, and play the window system with the private headers.
    add_executable(nt_widgets_tests tests/test_widgets.cpp)
    target_include_directories(nt_widgets_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_tests COMMAND nt_widgets_tests)
    set_tests_properties(nt_widgets_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_tests)

    # The controls (radio buttons, line edits, sliders, spin boxes, combo boxes, tabs) on the offscreen platform.
    add_executable(nt_widgets_itemviews_tests tests/test_widgets_itemviews.cpp)
    target_include_directories(nt_widgets_itemviews_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_itemviews_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_itemviews_tests COMMAND nt_widgets_itemviews_tests)
    set_tests_properties(nt_widgets_itemviews_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_itemviews_tests)

    # The rich text classes of Nt GUI: formats, QTextLayout and QTextDocument with its HTML.
    add_executable(nt_richtext_tests tests/test_richtext.cpp)
    target_link_libraries(nt_richtext_tests PRIVATE Nt::Gui)
    add_test(NAME nt_richtext_tests COMMAND nt_richtext_tests)
    set_tests_properties(nt_richtext_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_richtext_tests)

    # Rich text in the widgets: QLabel, QTextEdit and the links of QTextBrowser.
    add_executable(nt_widgets_richtext_tests tests/test_widgets_richtext.cpp)
    target_include_directories(nt_widgets_richtext_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_richtext_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_richtext_tests COMMAND nt_widgets_richtext_tests)
    set_tests_properties(nt_widgets_richtext_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_richtext_tests)

    add_executable(nt_widgets_controls_tests tests/test_widgets_controls.cpp)
    target_include_directories(nt_widgets_controls_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_controls_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_controls_tests COMMAND nt_widgets_controls_tests)
    set_tests_properties(nt_widgets_controls_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_controls_tests)

    # The main window, tool bars, dock widgets and size grips on the offscreen platform.
    add_executable(nt_widgets_mainwindow_tests tests/test_widgets_mainwindow.cpp)
    target_include_directories(nt_widgets_mainwindow_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_mainwindow_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_mainwindow_tests COMMAND nt_widgets_mainwindow_tests)
    set_tests_properties(nt_widgets_mainwindow_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_mainwindow_tests)

    # Windows with a parent, modality and dialogs on the offscreen platform.
    add_executable(nt_widgets_dialogs_tests tests/test_widgets_dialogs.cpp)
    target_include_directories(nt_widgets_dialogs_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_dialogs_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_dialogs_tests COMMAND nt_widgets_dialogs_tests)
    set_tests_properties(nt_widgets_dialogs_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_dialogs_tests)

    # The base classes of the input widgets and button groups on the offscreen platform.
    add_executable(nt_widgets_input_tests tests/test_widgets_input.cpp)
    target_include_directories(nt_widgets_input_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_input_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_input_tests COMMAND nt_widgets_input_tests)
    set_tests_properties(nt_widgets_input_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_input_tests)

    # Frames, scroll bars, scroll areas and tables on the offscreen platform.
    add_executable(nt_widgets_scroll_tests tests/test_widgets_scroll.cpp)
    target_include_directories(nt_widgets_scroll_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_scroll_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_scroll_tests COMMAND nt_widgets_scroll_tests)
    set_tests_properties(nt_widgets_scroll_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_scroll_tests)

    add_executable(nt_widgets_menu_tests tests/test_widgets_menu.cpp)
    target_include_directories(nt_widgets_menu_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_widgets_menu_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_widgets_menu_tests COMMAND nt_widgets_menu_tests)
    set_tests_properties(nt_widgets_menu_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_widgets_menu_tests)

    # Drawing at a device pixel ratio of 2, on the offscreen platform.
    add_executable(nt_gui_scale_tests tests/test_gui_scale.cpp)
    target_include_directories(nt_gui_scale_tests PRIVATE src/gui src/widgets)
    target_link_libraries(nt_gui_scale_tests PRIVATE Nt::Widgets)
    add_test(NAME nt_gui_scale_tests COMMAND nt_gui_scale_tests)
    set_tests_properties(nt_gui_scale_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_gui_scale_tests)

    # 3D math: QVector2D/3D/4D, QQuaternion and QMatrix4x4.
    add_executable(nt_math3d_tests tests/test_math3d.cpp)
    target_link_libraries(nt_math3d_tests PRIVATE Nt::Gui)
    add_test(NAME nt_math3d_tests COMMAND nt_math3d_tests)
    nt_set_compile_options(nt_math3d_tests)

    # Vector drawing: QTransform, QPainterPath, gradients, pens, and QPainter drawing them, through pixels.
    add_executable(nt_painter_tests tests/test_painter.cpp)
    target_link_libraries(nt_painter_tests PRIVATE Nt::Svg)
    add_test(NAME nt_painter_tests COMMAND nt_painter_tests)
    set_tests_properties(nt_painter_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_painter_tests)

    # Nt OpenGL: contexts, framebuffer objects, QPainter through OpenGL against the raster engine, and QOpenGLWindow,
    # on the offscreen platform. The tests report a skip where there is no OpenGL.
    add_executable(nt_opengl_tests tests/test_opengl.cpp)
    target_link_libraries(nt_opengl_tests PRIVATE Nt::OpenGL)
    add_test(NAME nt_opengl_tests COMMAND nt_opengl_tests)
    set_tests_properties(nt_opengl_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_opengl_tests)

    # Nt SVG: the reader, the rasterizer, the generator and the widget, on the offscreen platform.
    add_executable(nt_svg_tests tests/test_svg.cpp)
    target_include_directories(nt_svg_tests PRIVATE src/svg src/gui)
    target_link_libraries(nt_svg_tests PRIVATE Nt::SvgWidgets)
    add_test(NAME nt_svg_tests COMMAND nt_svg_tests)
    set_tests_properties(nt_svg_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_svg_tests)

    # Nt Charts: the model, axes and ticks, drawing through pixels, the mouse and zooming, offscreen.
    add_executable(nt_charts_tests tests/test_charts.cpp)
    target_include_directories(nt_charts_tests PRIVATE src/charts src/gui)
    target_link_libraries(nt_charts_tests PRIVATE Nt::Charts)
    add_test(NAME nt_charts_tests COMMAND nt_charts_tests)
    set_tests_properties(nt_charts_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_charts_tests)

    # Nt Qml: the JavaScript language, its library, QJSValue and QObjects in scripts.
    add_executable(nt_jsengine_tests tests/test_jsengine.cpp tests/jsengine_classes.h)
    target_link_libraries(nt_jsengine_tests PRIVATE Nt::Qml)
    nt_enable_automoc(nt_jsengine_tests)
    add_test(NAME nt_jsengine_tests COMMAND nt_jsengine_tests)
    nt_set_compile_options(nt_jsengine_tests)

    # The QML engine: documents, bindings, components, modules and the QtQml types.
    add_executable(nt_qml_tests tests/test_qml.cpp tests/qml_classes.h)
    target_link_libraries(nt_qml_tests PRIVATE Nt::Qml)
    nt_enable_automoc(nt_qml_tests)
    add_test(NAME nt_qml_tests COMMAND nt_qml_tests)
    nt_set_compile_options(nt_qml_tests)

    # Nt FM: patches, the synthesiser rendered offline, the real-time command path and a fake output.
    add_executable(nt_fm_tests tests/test_fm.cpp)
    target_include_directories(nt_fm_tests PRIVATE src/fm)
    target_link_libraries(nt_fm_tests PRIVATE Nt::FM)
    if(NT_HAVE_ZLIB)
        target_compile_definitions(nt_fm_tests PRIVATE NT_HAVE_ZLIB)
        target_link_libraries(nt_fm_tests PRIVATE ZLIB::ZLIB)
    endif()
    add_test(NAME nt_fm_tests COMMAND nt_fm_tests)

    # The QML types of Nt FM, through documents run offline.
    add_executable(nt_fmqml_tests tests/test_fmqml.cpp)
    target_link_libraries(nt_fmqml_tests PRIVATE Nt::FMQml)
    add_test(NAME nt_fmqml_tests COMMAND nt_fmqml_tests)
    nt_set_compile_options(nt_fmqml_tests)

    # Nt Multimedia against a fake audio backend: streams, sound effects, the media player and WAV decoding.
    add_executable(nt_multimedia_tests tests/test_multimedia.cpp)
    target_include_directories(nt_multimedia_tests PRIVATE src/multimedia)
    target_link_libraries(nt_multimedia_tests PRIVATE Nt::Multimedia)
    add_test(NAME nt_multimedia_tests COMMAND nt_multimedia_tests)
    nt_set_compile_options(nt_multimedia_tests)
    # Cameras, video frames, image capture, recording and decoding, against fake backends.
    add_executable(nt_multimedia_capture_tests tests/test_multimedia_capture.cpp)
    target_include_directories(nt_multimedia_capture_tests PRIVATE src/multimedia)
    target_link_libraries(nt_multimedia_capture_tests PRIVATE Nt::Multimedia)
    add_test(NAME nt_multimedia_capture_tests COMMAND nt_multimedia_capture_tests)
    nt_set_compile_options(nt_multimedia_capture_tests)
    # QVideoWidget on the offscreen platform.
    add_executable(nt_multimediawidgets_tests tests/test_multimediawidgets.cpp)
    target_link_libraries(nt_multimediawidgets_tests PRIVATE Nt::MultimediaWidgets)
    add_test(NAME nt_multimediawidgets_tests COMMAND nt_multimediawidgets_tests)
    set_tests_properties(nt_multimediawidgets_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_multimediawidgets_tests)

    # The gamepad tests replace the evdev platform with one of their own, and send key events to an
    # offscreen window.
    add_executable(nt_gamepad_tests tests/test_gamepad.cpp)
    target_include_directories(nt_gamepad_tests PRIVATE src/gamepad)
    target_link_libraries(nt_gamepad_tests PRIVATE Nt::Gamepad Nt::Gui)
    add_test(NAME nt_gamepad_tests COMMAND nt_gamepad_tests)
    set_tests_properties(nt_gamepad_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    nt_set_compile_options(nt_gamepad_tests)
    nt_set_compile_options(nt_fm_tests)

    # The xcb tests need an X server (or XWayland); they report a skip without one.
    if(NT_HAVE_XCB)
        add_executable(nt_gui_xcb_tests tests/test_gui_xcb.cpp)
        target_include_directories(nt_gui_xcb_tests SYSTEM PRIVATE ${NT_XCB_INCLUDE_DIRS})
        target_include_directories(nt_gui_xcb_tests PRIVATE src/gui tests)
        target_link_directories(nt_gui_xcb_tests PRIVATE ${NT_XCB_LIBRARY_DIRS})
        target_link_libraries(nt_gui_xcb_tests PRIVATE Nt::Gui ${NT_XCB_LIBRARIES})
        add_test(NAME nt_gui_xcb_tests COMMAND nt_gui_xcb_tests)
        nt_set_compile_options(nt_gui_xcb_tests)

        add_executable(nt_gui_xcb_scale_tests tests/test_gui_xcb_scale.cpp)
        target_include_directories(nt_gui_xcb_scale_tests SYSTEM PRIVATE ${NT_XCB_INCLUDE_DIRS})
        target_include_directories(nt_gui_xcb_scale_tests PRIVATE src/gui tests)
        target_link_directories(nt_gui_xcb_scale_tests PRIVATE ${NT_XCB_LIBRARY_DIRS})
        target_link_libraries(nt_gui_xcb_scale_tests PRIVATE Nt::Gui ${NT_XCB_LIBRARIES})
        add_test(NAME nt_gui_xcb_scale_tests COMMAND nt_gui_xcb_scale_tests)
        nt_set_compile_options(nt_gui_xcb_scale_tests)

        add_executable(nt_widgets_xcb_tests tests/test_widgets_xcb.cpp)
        target_include_directories(nt_widgets_xcb_tests SYSTEM PRIVATE ${NT_XCB_INCLUDE_DIRS})
        target_include_directories(nt_widgets_xcb_tests PRIVATE src/gui tests)
        target_link_directories(nt_widgets_xcb_tests PRIVATE ${NT_XCB_LIBRARY_DIRS})
        target_link_libraries(nt_widgets_xcb_tests PRIVATE Nt::Widgets ${NT_XCB_LIBRARIES})
        add_test(NAME nt_widgets_xcb_tests COMMAND nt_widgets_xcb_tests)
        nt_set_compile_options(nt_widgets_xcb_tests)
    endif()

    nt_set_compile_options(nt_tests nt_core_tests nt_core_value_tests nt_network_tests nt_moc_tests nt_ipc_tests
                           nt_test_tests)
    foreach(suite animation cbor coreapi coremore datetime filesystem globals mime models storage streams text types xmlstream)
        nt_set_compile_options(nt_${suite}_tests)
    endforeach()
endif()

if(NT_BUILD_EXAMPLES)
    # examples/hello/CMakeLists.txt is the standalone version of this target.
    add_executable(hello examples/hello/hello.cpp)
    target_link_libraries(hello PRIVATE Nt::Core)

    # Same flow as a Qt project using CMAKE_AUTOMOC.
    add_executable(moc_example examples/moc/main.cpp examples/moc/shape.h)
    target_link_libraries(moc_example PRIVATE Nt::Core)
    nt_enable_automoc(moc_example)
    nt_set_compile_options(hello moc_example)

    # examples/signals: the patterns of the "Signals and Slots" guide; it checks itself.
    add_executable(signals_example examples/signals/main.cpp examples/signals/thermostat.h)
    target_link_libraries(signals_example PRIVATE Nt::Core)
    nt_enable_automoc(signals_example)
    nt_set_compile_options(signals_example)

    if(NT_BUILD_TESTS)
        add_test(NAME signals_example COMMAND signals_example)
    endif()

    # A test written the way a Qt project writes one, with the .moc file included.
    add_executable(tst_counter examples/test/tst_counter.cpp)
    target_link_libraries(tst_counter PRIVATE Nt::Test)
    nt_enable_automoc(tst_counter)
    nt_set_compile_options(tst_counter)
    if(NT_BUILD_TESTS)
        add_test(NAME tst_counter COMMAND tst_counter)
    endif()

    # A window that is painted with QPainter; see "Using Nt GUI" in the documentation.
    add_executable(hello_window examples/gui/hello_window.cpp)
    target_link_libraries(hello_window PRIVATE Nt::Gui)
    nt_set_compile_options(hello_window)

    # Paths, gradients, pens and transformations, antialiased, in software or with OpenGL (painting --gl); also renders
    # the pictures of the documentation (cmake --build build --target painter_images) and times the scenes in both
    # (painting --bench).
    add_executable(painting examples/gui/painting.cpp)
    target_link_libraries(painting PRIVATE Nt::OpenGL)
    nt_set_compile_options(painting)
    add_custom_target(painter_images
        COMMAND painting --write "${CMAKE_CURRENT_SOURCE_DIR}/doc/images"
        COMMENT "Rendering the painter pictures to doc/images/painter"
        VERBATIM)

    # An OpenGL window painted with OpenGL and QPainter, with or without vsync; see QOpenGLWindow.
    add_executable(glwindow examples/opengl/glwindow.cpp)
    target_link_libraries(glwindow PRIVATE Nt::OpenGL)
    nt_set_compile_options(glwindow)

    # A form made of widgets and layouts, with the Nt logo as an SVG resource; see "Using Nt Widgets" in the
    # documentation.
    nt_add_resources(hello_widgets_resources examples/widgets/hello_widgets.qrc)
    add_executable(hello_widgets examples/widgets/hello_widgets.cpp ${hello_widgets_resources})
    target_link_libraries(hello_widgets PRIVATE Nt::SvgWidgets)
    nt_set_compile_options(hello_widgets)

    # A tour of the widgets: layouts, controls, custom widgets, a dark theme.
    nt_add_resources(gallery_resources examples/widgets/gallery.qrc)
    add_executable(gallery examples/widgets/gallery.cpp ${gallery_resources})
    target_link_libraries(gallery PRIVATE Nt::SvgWidgets)
    nt_set_compile_options(gallery)

    # Renders each widget in its states to doc/images/widgets, the pictures of the reference documentation:
    #   cmake --build build --target widget_images
    add_executable(render_widgets examples/widgets/render_widgets.cpp)
    target_link_libraries(render_widgets PRIVATE Nt::Widgets)
    nt_set_compile_options(render_widgets)
    add_custom_target(widget_images
        COMMAND render_widgets "${CMAKE_CURRENT_SOURCE_DIR}/doc/images"
        COMMENT "Rendering the widgets to doc/images/widgets"
        VERBATIM)

    # The same widget examples against Qt 6, as hello_widgets_qt and gallery_qt in qt-examples/. They are built
    # by a project of their own, because Nt provides the Qt6:: names itself.
    if(NT_EXAMPLES_QT)
        include(ExternalProject)
        ExternalProject_Add(qt_examples
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples/qt"
            BINARY_DIR "${CMAKE_BINARY_DIR}/qt-examples"
            CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            BUILD_ALWAYS ON
            INSTALL_COMMAND "")
    endif()

    # A live dashboard of charts; see "Using Nt Charts" in the documentation.
    add_executable(chart_dashboard examples/charts/dashboard.cpp)
    target_link_libraries(chart_dashboard PRIVATE Nt::Charts)
    nt_set_compile_options(chart_dashboard)

    # Scripting with QJSEngine; see "Using Nt Qml" in the documentation.
    add_executable(js_scripting examples/qml/scripting.cpp)
    target_link_libraries(js_scripting PRIVATE Nt::Qml)
    nt_enable_automoc(js_scripting)
    nt_set_compile_options(js_scripting)

    # A QML document with a C++ type; see "Using Nt QML" in the documentation.
    add_executable(hello_qml examples/qml/hello_qml.cpp)
    target_link_libraries(hello_qml PRIVATE Nt::Qml)
    nt_enable_automoc(hello_qml)
    nt_set_compile_options(hello_qml)

    # Renders each kind of chart to doc/images/charts, the pictures of the reference documentation:
    #   cmake --build build --target chart_images
    add_executable(render_charts examples/charts/render_charts.cpp)
    target_link_libraries(render_charts PRIVATE Nt::Charts)
    nt_set_compile_options(render_charts)
    add_custom_target(chart_images
        COMMAND render_charts "${CMAKE_CURRENT_SOURCE_DIR}/doc/images"
        COMMENT "Rendering the charts to doc/images/charts"
        VERBATIM)

    # Shows an SVG file in a window, or converts what it draws; see "Using Nt SVG" in the documentation.
    add_executable(svg_viewer examples/svg/svg_viewer.cpp)
    target_link_libraries(svg_viewer PRIVATE Nt::SvgWidgets)
    nt_set_compile_options(svg_viewer)

    # An address book in an SQLite database; see "Using Nt SQL" in the documentation.
    add_executable(contacts examples/sql/contacts.cpp)
    target_link_libraries(contacts PRIVATE Nt::Sql)
    nt_set_compile_options(contacts)

    # A serial terminal; see "Using Nt Serial Port" in the documentation.
    add_executable(serialterm examples/serialport/serialterm.cpp)
    target_link_libraries(serialterm PRIVATE Nt::SerialPort)
    nt_set_compile_options(serialterm)

    # A push-to-talk switch; see "Using Nt GPIO" in the documentation.
    add_executable(ptt examples/gpio/ptt.cpp)
    target_link_libraries(ptt PRIVATE Nt::Gpio)
    nt_set_compile_options(ptt)

    # Prints what the gamepads do; see "Using Nt Gamepad" in the documentation.
    add_executable(gamepadinfo examples/gamepad/gamepadinfo.cpp)
    target_link_libraries(gamepadinfo PRIVATE Nt::Gamepad)
    nt_set_compile_options(gamepadinfo)

    # Plays or renders FM sound effects; see "Using Nt FM" in the documentation.
    # Lists audio devices and plays a tone; plays WAV files. See "Using Nt Multimedia" in the documentation.
    add_executable(audiodevices examples/multimedia/audiodevices.cpp)
    target_link_libraries(audiodevices PRIVATE Nt::Multimedia)
    nt_set_compile_options(audiodevices)
    add_executable(audioplayer examples/multimedia/audioplayer.cpp)
    target_link_libraries(audioplayer PRIVATE Nt::Multimedia)
    nt_set_compile_options(audioplayer)
    add_executable(camera examples/multimedia/camera.cpp)
    target_link_libraries(camera PRIVATE Nt::Multimedia)
    nt_set_compile_options(camera)
    add_executable(recorder examples/multimedia/recorder.cpp)
    target_link_libraries(recorder PRIVATE Nt::Multimedia)
    nt_set_compile_options(recorder)
    add_executable(decoder examples/multimedia/decoder.cpp)
    target_link_libraries(decoder PRIVATE Nt::Multimedia)
    nt_set_compile_options(decoder)
    add_executable(viewfinder examples/multimedia/viewfinder.cpp)
    target_link_libraries(viewfinder PRIVATE Nt::MultimediaWidgets)
    nt_set_compile_options(viewfinder)

    add_executable(fmplay examples/fm/fmplay.cpp)
    target_link_libraries(fmplay PRIVATE Nt::FM)
    nt_set_compile_options(fmplay)

    # Sound effects and music from a QML document; see "Using Nt FM" in the documentation.
    add_executable(fmqml examples/fm/fmqml.cpp)
    target_link_libraries(fmqml PRIVATE Nt::FMQml)
    nt_set_compile_options(fmqml)

    # An echo server and client; see "Using Nt WebSockets" in the documentation.
    add_executable(websocket_echo examples/websockets/echo.cpp)
    target_link_libraries(websocket_echo PRIVATE Nt::WebSockets)
    nt_set_compile_options(websocket_echo)

    # A small web service; see "Using Nt HTTP Server" in the documentation.
    add_executable(hello_server examples/httpserver/hello_server.cpp)
    target_link_libraries(hello_server PRIVATE Nt::HttpServer)
    nt_set_compile_options(hello_server)

    # A clock on the session bus; see "Using Nt D-Bus" in the documentation.
    add_executable(dbus_clock examples/dbus/clock.cpp)
    target_link_libraries(dbus_clock PRIVATE Nt::DBus)
    nt_enable_automoc(dbus_clock)
    nt_set_compile_options(dbus_clock)
endif()
