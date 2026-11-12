// ==========================================
// BUTTON TEST
// ==========================================
//
// Prints the debounced state of every button over USB serial, plus
// how many presses have been counted. Each physical press should
// add exactly 1 to its counter: if a counter jumps by more than 1
// per press, the button is bouncing through the debounce filter.

#include <stdio.h>

#include "pico/stdlib.h"
#include "input/buttons.h"

static const char *const names[BTN_COUNT] = { "A", "B", "C", "D", "CLICK" };

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);   // time to open the serial monitor

    buttons_init();

    printf("\n===== BUTTON TEST =====\n");
    printf("Press each button once; its count should rise by 1.\n\n");

    uint32_t press_count[BTN_COUNT] = { 0 };

    while (true)
    {
        buttons_poll();

        for (int i = 0; i < BTN_COUNT; i++)
        {
            if (button_just_pressed((Button)i))
                press_count[i]++;

            printf("%s:%s(%lu)  ", names[i],
                   button_down((Button)i) ? "DOWN" : "up  ",
                   (unsigned long)press_count[i]);
        }
        printf("\n");

        sleep_ms(200);
    }
}
