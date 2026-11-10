# MIT License - see LICENSE
#
# A standalone project that ships data with a program, in place of Qt resources:
#
#   cmake -S examples/resources -B build-resources && cmake --build build-resources
#   build-resources/resources
cmake_minimum_required(VERSION 3.18)
project(NtResources LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Nt6 6 REQUIRED)
include(embed.cmake)

add_executable(resources main.cpp)
target_link_libraries(resources PRIVATE Nt6::Core)

# A file compiled into the program.
nt_example_embed(resources banner_txt data/banner.txt)

# Files next to the program: copy data/ beside the executable at build time...
add_custom_command(TARGET resources POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:resources>/data"
    VERBATIM)
# ...and install them where QStandardPaths::AppDataLocation looks, under <prefix>/share/resources.
include(GNUInstallDirs)
install(TARGETS resources)
install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/resources")
