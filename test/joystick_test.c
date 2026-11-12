// ==========================================
// JOYSTICK TEST / CALIBRATION
// ==========================================
//
// Prints raw ADC values (0-4095) and normalised positions (-1..+1)
// over USB serial. To calibrate, note the raw values with the stick
// at rest and at each extreme, and copy them into the JOY_*_MIN /
// CENTER / MAX constants in input/joystick.c.

#include <stdio.h>

#include "pico/stdlib.h"
#include "input/joystick.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);   // time to open the serial monitor

    joystick_init();

    printf("\n===== JOYSTICK TEST =====\n");

    while (true)
    {
        uint16_t raw_x = joystick_read_raw_x();
        uint16_t raw_y = joystick_read_raw_y();
        float x = joystick_read_x();
        float y = joystick_read_y();

        printf("raw X: %4u  raw Y: %4u  |  X: %+.2f  Y: %+.2f\n",
               raw_x, raw_y, x, y);

        sleep_ms(200);
    }
}
