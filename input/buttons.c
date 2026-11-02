
#include "pico/stdlib.h"
#include "buttons.h"

// --------------------------------------------------
// Joystick Shield Button GPIO Pin Definitions
// --------------------------------------------------

#define BUTTON_A        2
#define BUTTON_B        3
#define BUTTON_C        4
#define BUTTON_D        5
#define JOYSTICK_CLICK  8

// --------------------------------------------------
// Initialize all button GPIO pins
// --------------------------------------------------

void buttons_init(void)
{
    // List of GPIO pins connected to the buttons
    const uint button_pins[] = {
        BUTTON_A,
        BUTTON_B,
        BUTTON_C,
        BUTTON_D,
        JOYSTICK_CLICK
    };

    // Configure each button pin as an input with an internal pull-up
    for (int i = 0; i < 5; i++)
    {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
}

// --------------------------------------------------
// Check whether a button is pressed
// --------------------------------------------------

int button_pressed(uint gpio)
{
    // Buttons are active-low:
    // LOW  (0) -> Pressed
    // HIGH (1) -> Released

    return (gpio_get(gpio) == 0);
}

