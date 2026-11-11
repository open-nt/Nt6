# MIT License - see LICENSE
#
# Nt on an emulated board: a self-checking application that needs no hardware, and a web page with its results.
# The checks use the loopback interface, so they need no network set up. Build and run it with west:
#
#   west build -b mps2/an385 examples/zephyr/qemu -t run
cmake_minimum_required(VERSION 3.20)

# Nt is used straight from this repository. An application of its own lists Nt in its west manifest
# instead, and needs neither of these lines.
get_filename_component(NT_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../.. ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${NT_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nt_zephyr_qemu LANGUAGES CXX)

target_sources(app PRIVATE src/main.cpp src/webpage.cpp src/board.cpp)
target_link_libraries(app PRIVATE Nt::Core Nt::Network)
if(CONFIG_NT_HTTPSERVER)
    target_link_libraries(app PRIVATE Nt::HttpServer)
endif()
