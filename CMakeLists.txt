# ==========================================
# PICO HANDHELD GAME CONSOLE - build file
# ==========================================
#
# Targets:
#   handheld        the console firmware (menu + games)
#   *_test          stand-alone hardware test programs
#
# Build:   cmake -S . -B build && cmake --build build --target handheld
# Options: -DHANDHELD_DUAL_CORE=OFF   single-core build for FPS comparison

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------
# Pico SDK (paths used by the Raspberry Pi Pico VS Code extension)
# ------------------------------------------

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

# Raspberry Pi Pico 2 (RP2350, Arm cores).
set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(handheld C CXX ASM)

pico_sdk_init()

# ------------------------------------------
# Options
# ------------------------------------------

# Dual-core rendering: core 1 sends frames to the display while core 0
# draws the next one. OFF builds the single-core baseline.
option(HANDHELD_DUAL_CORE "Render with both RP2350 cores" ON)

# ------------------------------------------
# Settings shared by every program
# ------------------------------------------

function(handheld_common target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

    target_compile_definitions(${target} PRIVATE
        DISPLAY_LANDSCAPE=1                              # 320x240
        DISPLAY_DUAL_CORE=$<BOOL:${HANDHELD_DUAL_CORE}>
    )

    target_link_libraries(${target}
        pico_stdlib
        pico_multicore      # core 1 display task
        pico_flash          # flash_safe_execute (score saving)
        hardware_adc        # joystick
        hardware_spi        # display
        hardware_dma        # display frame transfer
        hardware_pwm        # sound
        hardware_clocks
        hardware_flash      # best scores
    )

    # printf goes to USB serial.
    pico_enable_stdio_usb(${target} 1)
    pico_enable_stdio_uart(${target} 0)

    # Also produce .uf2 / .bin / .hex / .dis files.
    pico_add_extra_outputs(${target})
endfunction()

# ------------------------------------------
# Main firmware
# ------------------------------------------

add_executable(handheld
    handheld.c
    menu.c
    space_shooter.c
    snake.c

    display/display.c
    input/buttons.c
    input/joystick.c
    sound/sound.c
    save/save.c
    power/power.c
    ui/ui.c
)
handheld_common(handheld)

# ------------------------------------------
# Hardware test programs
# ------------------------------------------

add_executable(display_test  test/display_test.c  display/display.c)
add_executable(buttons_test  test/buttons_test.c  input/buttons.c)
add_executable(joystick_test test/joystick_test.c input/joystick.c)
add_executable(sound_test    test/sound_test.c    sound/sound.c)

foreach(test display_test buttons_test joystick_test sound_test)
    handheld_common(${test})
endforeach()
