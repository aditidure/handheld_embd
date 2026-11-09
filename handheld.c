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
    stdio_init_all();

    // Initialize hardware

    display_init();

    buttons_init();

    joystick_init();

    sound_init();

    // Print FPS / frame-time stats over USB serial every second

    display_set_fps_logging(true);

    // Load best scores saved in flash

    save_init();

    // Seed random number generator

    srand(time_us_32());

    // Show the game-select menu (never returns)

    menu_run();

    return 0;
}