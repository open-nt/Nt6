# MIT License - see LICENSE
#
# Warning, hardening and sanitizer settings shared by every Nt target.
#
#   nt_set_compile_options(<target>...)
#
# Options (set before including this file):
#   NT_WARNINGS_AS_ERRORS  Treat warnings as errors. Off by default so a newer
#                          compiler cannot break a consumer's build; check.sh
#                          and CI turn it on.
#   NT_HARDENING           Add security hardening flags to optimised builds.
#   NT_SANITIZE            Semicolon-separated sanitizers, e.g. "address;undefined".

include(CheckCXXCompilerFlag)

option(NT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NT_HARDENING "Build with security hardening flags" ON)
set(NT_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. address;undefined or thread")

# Production builds default to Release; this only applies to single-config generators.
get_property(_nt_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT _nt_multi_config
   AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# The libraries are static; position independent code lets them be linked into
# PIE executables and shared objects. A Zephyr image is linked at fixed addresses, and its stack
# protection and other hardening are chosen with Kconfig.
if(NT_ZEPHYR)
    set(NT_HARDENING OFF)
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(_nt_warning_flags)
set(_nt_hardening_flags)
set(_nt_hardening_link_flags)
set(_nt_sanitize_flags)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_nt_warning_flags
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wconversion -Wsign-conversion
        -Wold-style-cast
        -Wcast-align
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wdouble-promotion
        -Wformat -Wformat-security
        -Wmissing-declarations)
    if(NT_WARNINGS_AS_ERRORS)
        list(APPEND _nt_warning_flags -Werror)
    endif()

    if(NT_HARDENING)
        # Probe each flag: some are not available on every compiler or architecture.
        foreach(flag -fstack-protector-strong -fstack-clash-protection -fcf-protection)
            string(MAKE_C_IDENTIFIER "NT_HAS${flag}" var)
            check_cxx_compiler_flag(${flag} ${var})
            if(${var})
                list(APPEND _nt_hardening_flags ${flag})
            endif()
        endforeach()
        # _FORTIFY_SOURCE needs optimisation, and some distributions define it already.
        list(APPEND _nt_hardening_flags
            "$<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>"
            "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND _nt_hardening_link_flags -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
        endif()
    endif()

    if(NT_SANITIZE)
        string(REPLACE ";" "," _nt_sanitizers "${NT_SANITIZE}")
        set(_nt_sanitize_flags -fsanitize=${_nt_sanitizers} -fno-omit-frame-pointer)
        if("undefined" IN_LIST NT_SANITIZE)
            list(APPEND _nt_sanitize_flags -fno-sanitize-recover=undefined)
        endif()
    endif()
elseif(MSVC)
    set(_nt_warning_flags /W4 /permissive-)
    if(NT_WARNINGS_AS_ERRORS)
        list(APPEND _nt_warning_flags /WX)
    endif()
endif()

# Applies the project's warnings, hardening and sanitizers to <target>.
function(nt_set_compile_options)
    foreach(target IN LISTS ARGN)
        get_target_property(type ${target} TYPE)
        target_compile_options(${target} PRIVATE
            ${_nt_warning_flags} ${_nt_hardening_flags} ${_nt_sanitize_flags})
        if(NOT type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE
                ${_nt_hardening_link_flags} ${_nt_sanitize_flags})
        endif()
    endforeach()
endfunction()
