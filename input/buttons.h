#ifndef BUTTONS_H
#define BUTTONS_H

#include "pico/stdlib.h"

// Initialize all joystick shield buttons
void buttons_init(void);

// Returns 1 if the specified button is pressed, 0 otherwise
int button_pressed(uint gpio);

#endif // BUTTONS_H

