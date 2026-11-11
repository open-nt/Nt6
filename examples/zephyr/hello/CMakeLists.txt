# MIT License - see LICENSE
#
# A Zephyr application that uses Nt Core and Nt Network. Build it with west, for example:
#
#   west build -b mimxrt1170_evk/mimxrt1176/cm7 examples/zephyr/hello
cmake_minimum_required(VERSION 3.20)

# Nt is used straight from this repository. An application of its own lists Nt in its west manifest
# instead, and needs neither of these lines.
get_filename_component(NT_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../.. ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${NT_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nt_zephyr_hello LANGUAGES CXX)

target_sources(app PRIVATE src/main.cpp)
target_link_libraries(app PRIVATE Nt::Core Nt::Network)
