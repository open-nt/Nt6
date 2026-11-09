# MIT License - see LICENSE
#
# The `snap` target builds the snap described by snap/snapcraft.yaml and writes
# open-nt6_<version>_<arch>.snap to the build directory:
#
#   cmake -S . -B build
#   cmake --build build --target snap
#
# snapcraft builds inside LXD or Multipass (managed mode), so the host does not
# need the snap's build dependencies. Pass extra options, for example
# --destructive-mode on a matching host, with -DNT_SNAPCRAFT_ARGS="...".
# The snap is not part of the default build or of CPack.

find_program(NT_SNAPCRAFT snapcraft)
set(NT_SNAPCRAFT_ARGS "" CACHE STRING "Extra arguments for `snapcraft pack` in the snap target")

if(NT_SNAPCRAFT)
    separate_arguments(nt_snapcraft_args NATIVE_COMMAND "${NT_SNAPCRAFT_ARGS}")
    add_custom_target(snap
        COMMAND ${NT_SNAPCRAFT} pack ${nt_snapcraft_args} --output ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building the snap in ${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
        VERBATIM)
else()
    add_custom_target(snap
        COMMAND ${CMAKE_COMMAND} -E echo "snapcraft not found: install it with 'sudo snap install snapcraft --classic'"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()
