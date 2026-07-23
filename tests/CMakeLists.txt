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

add_executable(test_words test_words.c ${APPSRC}/words.c ${APPSRC}/hints.c)
target_include_directories(test_words PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME words COMMAND test_words)

add_executable(test_aliens test_aliens.c ${APPSRC}/aliens.c)
target_include_directories(test_aliens PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME aliens COMMAND test_aliens)

add_executable(test_game_core test_game_core.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_core PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_core COMMAND test_game_core)

add_executable(test_game_typing test_game_typing.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_typing PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_typing COMMAND test_game_typing)

add_executable(test_game_boss test_game_boss.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_boss PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_boss COMMAND test_game_boss)

add_executable(test_sfx test_sfx.c ${APPSRC}/sfx.c)
target_include_directories(test_sfx PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME sfx COMMAND test_sfx)
