# Generated CMake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pico SDK configuration
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(handheld C CXX ASM)

pico_sdk_init()


# ==========================================
# MAIN GAME EXECUTABLE
# ==========================================

add_executable(handheld
    handheld.c
    menu.c
    space_shooter.c
    snake.c

    ui/ui.c

    input/buttons.c
    input/joystick.c

    display/display.c

    game/physics.c
    game/score.c

    sound/sound.c
)

# The menu and all its games run in landscape (320x240).
target_compile_definitions(handheld PRIVATE DISPLAY_LANDSCAPE=1)


# ==========================================
# JOYSTICK TEST
# ==========================================

add_executable(joystick_test
    test/joystick_test.c
    input/joystick.c
)


# ==========================================
# BUTTON TEST
# ==========================================

add_executable(buttons_test
    test/buttons_test.c
    input/buttons.c
)


# ==========================================
# PHYSICS TEST
# ==========================================

add_executable(physics_test
    test/physics_test.c
    game/physics.c
    input/buttons.c
    input/joystick.c
)


# ==========================================
# DISPLAY TEST
# ==========================================

add_executable(display_test
    test/display_test.c
    display/display.c
)


# ==========================================
# SNAKE INTEGRATION TEST
# Buttons + Display + Game Logic
# ==========================================

add_executable(snake_test
    test/snake_test.c
    input/buttons.c
    display/display.c
    game/score.c
    sound/sound.c

)
add_executable(sound_test
    test/sound_test.c
    sound/sound.c
)

target_include_directories(sound_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(sound_test
    pico_stdlib
    hardware_pwm
    hardware_clocks
)

pico_add_extra_outputs(sound_test)


# ==========================================
# COMMON CONFIGURATION
# ==========================================

foreach(target handheld joystick_test buttons_test physics_test display_test snake_test)

    target_link_libraries(${target}
        pico_stdlib
        hardware_adc
        hardware_spi
        hardware_dma
        hardware_pwm
        hardware_clocks
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    pico_enable_stdio_usb(${target} 1)
    pico_enable_stdio_uart(${target} 0)

    pico_add_extra_outputs(${target})

endforeach()
