#include <stdio.h>
#include "pico/stdlib.h"

#include "input/joystick.h"

int main(void)
{
    // Initialize USB serial communication
    stdio_init_all();
    sleep_ms(2000);

    printf("\n================================\n");
    printf("       JOYSTICK TEST\n");
    printf("================================\n");

    // Initialize joystick ADC
    joystick_init();

    printf("Joystick initialized!\n\n");

    while (true)
    {
        // Read normalized joystick positions
        float x = joystick_read_x();
        float y = joystick_read_y();

        // Read raw ADC values
        uint16_t raw_x = joystick_read_raw_x();
        uint16_t raw_y = joystick_read_raw_y();

        // Display joystick readings
        printf("X: %.3f | Y: %.3f | Raw X: %u | Raw Y: %u\n",
               x, y, raw_x, raw_y);

        sleep_ms(200);
    }

    return 0;
}