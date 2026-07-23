cmake_minimum_required(VERSION 3.13)

# RP2350B board header (PICO_RP2350A=0 -> 48 GPIO, 16MB flash).
# Do NOT pass -DPICO_BOARD on the command line - that overrides this.
set(PICO_BOARD freewili2 CACHE STRING "Board type")
list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/wilibsp/bsp/boards")

# Fall back to fetching the Pico SDK when no local install is configured.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_FETCH_FROM_GIT on)
endif()
include(wilibsp/pico_sdk_import.cmake)

project(typinginvaders C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_subdirectory(wilibsp/bsp)
add_subdirectory(apps/typing_invaders)
