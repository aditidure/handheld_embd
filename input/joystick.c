#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "joystick.h"

// --------------------------------------------------
// Joystick ADC Channel Definitions
// --------------------------------------------------

#define JOY_X_ADC  0
#define JOY_Y_ADC  1

// GPIO pins connected to the joystick axes
#define JOY_X_PIN  26
#define JOY_Y_PIN  27

// --------------------------------------------------
// Joystick Calibration Values
// --------------------------------------------------

// X-axis calibration
#define JOY_X_MIN     0.0f
#define JOY_X_CENTER  1769.0f
#define JOY_X_MAX     3747.0f

// Y-axis calibration
#define JOY_Y_MIN     6.0f
#define JOY_Y_CENTER  2073.0f
#define JOY_Y_MAX     4095.0f

#define DEADZONE 0.20f


// --------------------------------------------------
// Normalize ADC Reading
// --------------------------------------------------

// Converts raw ADC values into a normalized range
// of [-1.0, +1.0], with 0 representing the centre.

static float normalize_axis(uint16_t value,
                            float min_value,
                            float center_value,
                            float max_value)
{
    float result;

    // Normalize values below the centre
    if (value < center_value)
    {
        result = ((float)value - center_value) /
                 (center_value - min_value);
    }
    // Normalize values above the centre
    else
    {
        result = ((float)value - center_value) /
                 (max_value - center_value);
    }

    // Restrict the output to the range [-1, +1]
    if (result > 1.0f)
        result = 1.0f;

    if (result < -1.0f)
        result = -1.0f;

    // Ignore small fluctuations around the centre
    if (result > -DEADZONE && result < DEADZONE)
        result = 0.0f;

    return result;
}


// --------------------------------------------------
// Initialize Joystick ADC
// --------------------------------------------------

void joystick_init(void)
{
    // Initialize the ADC peripheral
    adc_init();

    // Configure joystick X and Y pins for ADC input
    adc_gpio_init(JOY_X_PIN);
    adc_gpio_init(JOY_Y_PIN);
}


// --------------------------------------------------
// Read Normalized Joystick Values
// --------------------------------------------------

// Read and normalize the X-axis position
float joystick_read_x(void)
{
    adc_select_input(JOY_X_ADC);

    uint16_t value = adc_read();

    return normalize_axis(value,
                           JOY_X_MIN,
                           JOY_X_CENTER,
                           JOY_X_MAX);
}


// Read and normalize the Y-axis position
float joystick_read_y(void)
{
    adc_select_input(JOY_Y_ADC);

    uint16_t value = adc_read();

    return normalize_axis(value,
                           JOY_Y_MIN,
                           JOY_Y_CENTER,
                           JOY_Y_MAX);
}


// --------------------------------------------------
// Read Raw ADC Values (For Calibration)
// --------------------------------------------------

// Return the raw ADC reading from the X-axis
uint16_t joystick_read_raw_x(void)
{
    adc_select_input(JOY_X_ADC);

    return adc_read();
}


// Return the raw ADC reading from the Y-axis
uint16_t joystick_read_raw_y(void)
{
    adc_select_input(JOY_Y_ADC);

    return adc_read();
}

