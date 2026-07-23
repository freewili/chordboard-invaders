add_executable(typing_invaders
    main.c hints.c words.c aliens.c)
target_link_libraries(typing_invaders freewili2_bsp hardware_flash)
pico_set_binary_type(typing_invaders copy_to_ram)   # required: firmware runs from SRAM
pico_add_extra_outputs(typing_invaders)             # .uf2 / .bin / .map
