#include "pico/stdlib.h"
#include "display/display.h"

int main(void)
{
    stdio_init_all();

    sleep_ms(2000);

    display_init();
    

    while (true)
{
    display_clear(BLACK);

display_draw_string(20, 30, "HELLO", WHITE, 2);

display_draw_string(20, 70, "SCORE:", GREEN, 2);
display_draw_number(110, 70, 120, YELLOW, 2);

display_draw_string(20, 110, "BEST:", GREEN, 2);
display_draw_number(110, 110, 250, YELLOW, 2);

display_show();

    sleep_ms(1000);
}

    return 0;
}