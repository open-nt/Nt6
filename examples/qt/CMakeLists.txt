# The widget examples built against Qt 6 instead of Nt, from the very same sources, to compare the two:
#   cmake -S examples/qt -B build-qt && cmake --build build-qt
# The top-level project builds this with -DNT_EXAMPLES_QT=ON (the qt_examples target).
cmake_minimum_required(VERSION 3.18)
project(nt_examples_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(hello_widgets_qt ../widgets/hello_widgets.cpp)
target_link_libraries(hello_widgets_qt PRIVATE Qt6::Widgets)

add_executable(gallery_qt ../widgets/gallery.cpp)
target_link_libraries(gallery_qt PRIVATE Qt6::Widgets)
