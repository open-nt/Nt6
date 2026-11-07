# MIT License - see LICENSE
#
# Builds against an installed Nt, to check that a package is usable:
#
#   cmake -S packaging/consumer -B /tmp/consumer && cmake --build /tmp/consumer && /tmp/consumer/consumer

cmake_minimum_required(VERSION 3.18)
project(NtConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Nt6 6 REQUIRED)

add_executable(consumer main.cpp counter.h)
target_link_libraries(consumer PRIVATE Nt6::Network)
nt_enable_automoc(consumer)
