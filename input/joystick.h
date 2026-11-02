#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "pico/stdlib.h"

// Initialize the joystick ADC pins
void joystick_init(void);

// Read normalized joystick positions (-1.0 to +1.0)
float joystick_read_x(void);
float joystick_read_y(void);

// Read raw ADC values for calibration (0 to 4095)
uint16_t joystick_read_raw_x(void);
uint16_t joystick_read_raw_y(void);

#endif // JOYSTICK_H

