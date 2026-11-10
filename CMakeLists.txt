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

option(NT_BUILD_TESTS "Build tests" ON)
option(NT_BUILD_EXAMPLES "Build examples" ON)
option(NT_EXAMPLES_QT "Also build the widget examples against Qt 6 (the qt_examples target), to compare them with Nt" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtCompilerOptions.cmake)

find_package(Threads REQUIRED)

# The version macros come from the VERSION file.
configure_file(cmake/qconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/QtCore/qconfig.h @ONLY)

add_library(NtCore
    src/dispatcher.cpp
    src/qabstractitemmodel.cpp
    src/qabstractproxymodel.cpp
    src/qbuffer.cpp
    src/qbytearray.cpp
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
    src/qeasingcurve.cpp
    src/qeventloop.cpp
    src/qexception.cpp
    src/qfile.cpp
    src/qfileinfo.cpp
    src/qfilesystemwatcher.cpp
    src/qfloat16.cpp
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
    src/qloggingcategory.cpp
    src/qmetaobject.cpp
    src/qmetatype.cpp
    src/qmetatype_convert.cpp
    src/qmetatype_streams.cpp
    src/qmimedata.cpp
    src/qobject.cpp
    src/qobjectcleanuphandler.cpp
    src/qprocess.cpp
    src/qrandom.cpp
    src/qregularexpression.cpp
    src/qresource.cpp
    src/qrunnable.cpp
    src/qsavefile.cpp
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
    src/qvariantiterable.cpp
    src/qversionnumber.cpp
    src/threaddata.cpp
)
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
target_link_libraries(NtCore PUBLIC Threads::Threads)

# Compressed resources (see ntrcc) need zlib or Zstandard, each used when it is found. Without
# them, resources compressed with that algorithm cannot be read and ntrcc cannot write them.
option(NT_USE_ZLIB "Read and write zlib-compressed resources with the system zlib" ON)
option(NT_USE_ZSTD "Read and write Zstandard-compressed resources with the system libzstd" ON)
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
    src/network/qlocalserver.cpp
    src/network/qlocalsocket.cpp
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

# TLS uses the system's OpenSSL. Without it, QSslSocket and https: are absent.
option(NT_USE_OPENSSL "Build TLS support with the system OpenSSL" ON)
set(NT_HAVE_OPENSSL OFF)
if(NT_USE_OPENSSL)
    find_package(OpenSSL 1.1.1 QUIET)
    if(OPENSSL_FOUND)
        set(NT_HAVE_OPENSSL ON)
    else()
        message(WARNING "OpenSSL 1.1.1 or newer was not found: building Nt Network without TLS")
    endif()
endif()
if(NT_HAVE_OPENSSL)
    target_sources(NtNetwork PRIVATE
        src/network/qssl.cpp
        src/network/qsslserver.cpp
        src/network/qsslsocket.cpp)
    target_link_libraries(NtNetwork PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    target_compile_definitions(NtNetwork PUBLIC QT_NO_SSL)
endif()
nt_set_compile_options(NtNetwork)

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

add_library(NtGui
    src/gui/platforms/offscreen/qoffscreenintegration.cpp
    src/gui/qaction.cpp
    src/gui/qactiongroup.cpp
    src/gui/qbackingstore.cpp
    src/gui/qbrush.cpp
    src/gui/qcolor.cpp
    src/gui/qdesktopportal.cpp
    src/gui/qevent.cpp
    src/gui/qfont.cpp
    src/gui/qfontengine.cpp
    src/gui/qfontmetrics.cpp
    src/gui/qguiapplication.cpp
    src/gui/qimage.cpp
    src/gui/qpixmap.cpp
    src/gui/qkeysequence.cpp
    src/gui/qpaintdevice.cpp
    src/gui/qpaintdevicewindow.cpp
    src/gui/qpaintengine.cpp
    src/gui/qpainter.cpp
    src/gui/qpalette.cpp
    src/gui/qpen.cpp
    src/gui/qplatform.cpp
    src/gui/qrasterpaintengine.cpp
    src/gui/qrasterwindow.cpp
    src/gui/qregion.cpp
    src/gui/qscreen.cpp
    src/gui/qstylehints.cpp
    src/gui/qtextlayout.cpp
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

# Moc replacement and its CMake helpers.
add_executable(ntmoc tools/ntmoc/main.cpp)
add_executable(Nt::moc ALIAS ntmoc)
add_executable(Qt6::moc ALIAS ntmoc)
target_compile_definitions(ntmoc PRIVATE NT_VERSION_STRING="${NT_VERSION}")
nt_set_compile_options(ntmoc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtMoc.cmake)
nt_moc_library(NtCore include/QtCore)
if(NOT NT_HAVE_OPENSSL)
    set(NT_MOC_DEFINES -DQT_NO_SSL)
endif()
nt_moc_library(NtNetwork include/QtNetwork)
set(NT_MOC_DEFINES)
nt_moc_library(NtSerialPort include/QtSerialPort)
nt_moc_library(NtDBus include/QtDBus)
if(NOT NT_HAVE_OPENSSL)
    set(NT_MOC_DEFINES -DQT_NO_SSL)
endif()
nt_moc_library(NtWebSockets include/QtWebSockets)
set(NT_MOC_DEFINES)
nt_moc_library(NtHttpServer include/QtHttpServer)
nt_moc_library(NtGui include/QtGui src/gui/qpa)
nt_moc_library(NtWidgets include/QtWidgets)

# Resource compiler and its CMake helpers.
add_executable(ntrcc tools/ntrcc/main.cpp)
add_executable(Nt::rcc ALIAS ntrcc)
add_executable(Qt6::rcc ALIAS ntrcc)
target_compile_definitions(ntrcc PRIVATE NT_VERSION_STRING="${NT_VERSION}")
nt_link_compression(ntrcc)
nt_set_compile_options(ntrcc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtRcc.cmake)

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

    foreach(suite datetime filesystem models streams text types)
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

    # The tests reach the private headers to check what a pseudo terminal hides.
    add_executable(nt_serialport_tests tests/test_serialport.cpp)
    target_include_directories(nt_serialport_tests PRIVATE src/serialport)
    target_link_libraries(nt_serialport_tests PRIVATE Nt::SerialPort)
    add_test(NAME nt_serialport_tests COMMAND nt_serialport_tests)
    nt_set_compile_options(nt_serialport_tests)

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

    nt_set_compile_options(nt_tests nt_core_tests nt_core_value_tests nt_network_tests nt_moc_tests
                           nt_test_tests)
    foreach(suite datetime filesystem models streams text types)
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

    # A form made of widgets and layouts; see "Using Nt Widgets" in the documentation.
    add_executable(hello_widgets examples/widgets/hello_widgets.cpp)
    target_link_libraries(hello_widgets PRIVATE Nt::Widgets)
    nt_set_compile_options(hello_widgets)

    # A tour of the widgets: layouts, controls, custom widgets, a dark theme.
    add_executable(gallery examples/widgets/gallery.cpp)
    target_link_libraries(gallery PRIVATE Nt::Widgets)
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

    # A serial terminal; see "Using Nt Serial Port" in the documentation.
    add_executable(serialterm examples/serialport/serialterm.cpp)
    target_link_libraries(serialterm PRIVATE Nt::SerialPort)
    nt_set_compile_options(serialterm)

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
