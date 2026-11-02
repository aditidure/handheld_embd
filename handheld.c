#include "pico/stdlib.h"
#include "display/display.h"

/*
 * Temporary main program.
 * Snake is built separately by the snake_test executable.
 * This keeps the main handheld target independent of the Snake test.
 */

int main(void)
{
    stdio_init_all();

    display_init();
    display_clear(BLACK);
    display_show();

    while (true) {
        /* Main handheld game will be added here later. */
        sleep_ms(100);
    }

    return 0;
}
