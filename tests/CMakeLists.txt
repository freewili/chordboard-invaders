cmake_minimum_required(VERSION 3.13)
project(typinginvaders_tests C)
set(CMAKE_C_STANDARD 11)
enable_testing()

# Standalone tree: no Pico SDK. Pure-logic game modules are compiled directly
# against the host C compiler so tests run natively with no hardware.
set(APPSRC ${CMAKE_CURRENT_SOURCE_DIR}/../apps/typing_invaders)
set(BSPDIR ${CMAKE_CURRENT_SOURCE_DIR}/../wilibsp/bsp)

add_executable(test_hints test_hints.c ${APPSRC}/hints.c)
target_include_directories(test_hints PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME hints COMMAND test_hints)
