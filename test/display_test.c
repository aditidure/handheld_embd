// ==========================================
// DISPLAY TEST
// ==========================================
//
// Checks the display wiring, orientation and colours: draws a border,
// colour bars and text, and shows the measured FPS. If the picture is
// upside down, change DISPLAY_MADCTL in display/display.c.

#include <stdio.h>

#include "pico/stdlib.h"
#include "display/display.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);   // time to open the serial monitor

    display_init();
    display_set_fps_logging(true);

    const uint16_t bars[] = { RED, GREEN, BLUE, YELLOW, WHITE };
    const int num_bars = (int)(sizeof(bars) / sizeof(bars[0]));

    int frame = 0;

    while (true)
    {
        display_clear(BLACK);

        // Border: all four edges must be visible on the panel.
        display_draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);

        display_draw_string(10, 10, "DISPLAY TEST", WHITE, 2);

        // Colour bars: check red/green/blue are not swapped.
        int bar_w = (SCREEN_WIDTH - 20) / num_bars;
        for (int i = 0; i < num_bars; i++)
            display_fill_rect(10 + i * bar_w, 40, bar_w - 4, 40, bars[i]);

        // A moving square shows the screen is updating.
        int x = 10 + (frame * 3) % (SCREEN_WIDTH - 40);
        display_fill_rect(x, 100, 20, 20, YELLOW);

        display_draw_string(10, 140, "FPS:", GREEN, 2);
        display_draw_number(70, 140, (int)display_get_stats().fps,
                            YELLOW, 2);

        display_show();
        frame++;
    }
}
