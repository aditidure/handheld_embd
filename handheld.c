#include "pico/stdlib.h"

#include "display/display.h"
#include "input/buttons.h"
#include "input/joystick.h"
#include "sound/sound.h"

#include "space_shooter.h"

#include <stdlib.h>

int main(void)
{
    stdio_init_all();

    // Initialize hardware

    display_init();

    buttons_init();

    joystick_init();

    sound_init();

    // Seed random number generator

    srand(time_us_32());

    // Initialize game

    space_shooter_init();

    // Start Space Shooter

    space_shooter_run();

    return 0;
}