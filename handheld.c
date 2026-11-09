// ==========================================
// HANDHELD GAME CONSOLE - entry point
// ==========================================
//
// Raspberry Pi Pico 2 (RP2350) handheld with an ST7789 SPI display,
// joystick, buttons and a piezo buzzer. main() initialises the
// hardware, loads the saved best scores and starts the game menu.
//
// Core 0 runs the menu and games; core 1 sends finished frames to
// the display (see display/display.c).

#include "pico/stdlib.h"

#include "display/display.h"
#include "input/buttons.h"
#include "input/joystick.h"
#include "sound/sound.h"

#include "save/save.h"
#include "menu.h"

#include <stdlib.h>

int main(void)
{
    // USB serial, used for FPS / timing logs.
    stdio_init_all();

    // Hardware. display_init() also starts core 1 (display task).
    display_init();
    buttons_init();
    joystick_init();
    sound_init();

    // Print FPS / frame-time stats over USB serial every second.
    display_set_fps_logging(true);

    // Load the best scores saved in flash.
    save_init();

    // Different random asteroid / apple positions on every boot.
    srand(time_us_32());

    // Game-select menu; never returns.
    menu_run();

    return 0;
}