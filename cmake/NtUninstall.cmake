# MIT License - see LICENSE
#
# Removes what "cmake --install" installed. Run through the "uninstall" target, which passes
#
#   BINARY_DIR   the build directory holding install_manifest*.txt
#   PREFIX       CMAKE_INSTALL_PREFIX
#   KEEP_DIRS    directories that are never removed, even when empty (bin, lib, include, ...)
#
# A DESTDIR in the environment is honoured, as it is for "cmake --install".

file(GLOB manifests "${BINARY_DIR}/install_manifest*.txt")
if(NOT manifests)
    message(FATAL_ERROR "No install_manifest*.txt in ${BINARY_DIR}; was Nt installed from this build?")
endif()

set(keep "$ENV{DESTDIR}${PREFIX}")
foreach(dir IN LISTS KEEP_DIRS)
    list(APPEND keep "$ENV{DESTDIR}${dir}")
endforeach()

set(files "")
foreach(manifest IN LISTS manifests)
    file(STRINGS "${manifest}" lines)
    list(APPEND files ${lines})
endforeach()
list(REMOVE_DUPLICATES files)

foreach(file IN LISTS files)
    if(EXISTS "${file}" OR IS_SYMLINK "${file}")
        message(STATUS "Uninstalling: ${file}")
        file(REMOVE "${file}")
    else()
        message(STATUS "Not found: ${file}")
    endif()

    # Remove the directories this left empty, up to a standard install directory.
    get_filename_component(dir "${file}" DIRECTORY)
    while(dir AND NOT dir IN_LIST keep AND IS_DIRECTORY "${dir}")
        file(GLOB entries "${dir}/*")
        if(entries)
            break()
        endif()
        message(STATUS "Removing directory: ${dir}")
        file(REMOVE_RECURSE "${dir}")
        get_filename_component(dir "${dir}" DIRECTORY)
    endwhile()
endforeach()
