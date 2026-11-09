# MIT License - see LICENSE
#
# The widget examples as a standalone project, built against an installed Nt such as the snap:
#
#   sudo snap install open-nt6
#   sudo apt install cmake g++ libssl-dev libfreetype-dev libfontconfig-dev libxcb1-dev
#   cmake -S examples/widgets -B build-widgets -DCMAKE_PREFIX_PATH=/snap/open-nt6/current/usr
#   cmake --build build-widgets
#   build-widgets/hello_widgets
#
# Nt's libraries are static, so the OpenSSL, FreeType, fontconfig and XCB development packages are needed to link
# them. Without a display, run the examples with `-platform offscreen`.
cmake_minimum_required(VERSION 3.18)
project(NtWidgetsExamples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# The snap's default location, so that a plain `cmake -S examples/widgets -B build` finds it too.
list(APPEND CMAKE_PREFIX_PATH /snap/open-nt6/current/usr)
find_package(Nt6 6 REQUIRED)

# A form made of widgets and layouts; see "Using Nt Widgets" in the documentation.
add_executable(hello_widgets hello_widgets.cpp ntlogo.h)
target_link_libraries(hello_widgets PRIVATE Nt6::Widgets)

# A tour of the widgets: layouts, controls, custom widgets, a dark theme.
add_executable(gallery gallery.cpp ntlogo.h)
target_link_libraries(gallery PRIVATE Nt6::Widgets)

# Renders each widget in its states to <directory>/widgets/*.png: `render_widgets <directory>`.
add_executable(render_widgets render_widgets.cpp)
target_link_libraries(render_widgets PRIVATE Nt6::Widgets)
