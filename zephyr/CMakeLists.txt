# MIT License - see LICENSE
#
# Zephyr module glue: builds the headless subset of Nt as part of a Zephyr application. The
# application links with Nt::Core, and with Nt::Network when CONFIG_NT_NETWORK is set.

if(CONFIG_NT)
    set(NT_ZEPHYR ON CACHE BOOL "Build Nt Core and Nt Network for the Zephyr RTOS" FORCE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/nt)
endif()
