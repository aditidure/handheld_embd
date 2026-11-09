#include "buttons.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"

// --------------------------------------------------
// Pin mapping (index = Button enum value)
// --------------------------------------------------

static const uint button_pins[BTN_COUNT] = {
    2,  // BTN_A
    3,  // BTN_B
    4,  // BTN_C
    5,  // BTN_D
    8,  // BTN_CLICK
};

#define SAMPLE_INTERVAL_US  1000

// --------------------------------------------------
// State written by the timer interrupt
// --------------------------------------------------

static volatile bool stable_down[BTN_COUNT];     // debounced state
static volatile uint8_t pending_presses[BTN_COUNT];

static uint8_t change_count[BTN_COUNT];         // ms the pin disagreed

// --------------------------------------------------
// Snapshot used by the game for the current frame
// --------------------------------------------------

static bool frame_pressed[BTN_COUNT];

static repeating_timer_t sample_timer;

// --------------------------------------------------
// 1 ms sampling + debounce (runs in interrupt)
// --------------------------------------------------

static bool sample_buttons(repeating_timer_t *timer)
{
    (void)timer;

    for (int i = 0; i < BTN_COUNT; i++)
    {
        // Active-low: pin reads 0 when pressed.
        bool raw_down = (gpio_get(button_pins[i]) == 0);

        if (raw_down == stable_down[i])
        {
            change_count[i] = 0;
            continue;
        }

        // Pin disagrees with the stable state. Accept the new state
        // only once it has held for the full debounce time.
        if (++change_count[i] >= BUTTON_DEBOUNCE_MS)
        {
            stable_down[i] = raw_down;
            change_count[i] = 0;

            if (raw_down && pending_presses[i] < 255)
                pending_presses[i]++;
        }
    }

    return true;
}

// --------------------------------------------------
// Public API
// --------------------------------------------------

void buttons_init(void)
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }

    // Let the pull-ups settle, then take the current levels as the
    // starting state so a held button is not reported as a press.
    sleep_ms(1);

    for (int i = 0; i < BTN_COUNT; i++)
    {
        stable_down[i] = (gpio_get(button_pins[i]) == 0);
        change_count[i] = 0;
        pending_presses[i] = 0;
        frame_pressed[i] = false;
    }

    // Negative interval = fixed rate between callback starts.
    add_repeating_timer_us(-SAMPLE_INTERVAL_US, sample_buttons,
                           NULL, &sample_timer);
}

void buttons_poll(void)
{
    uint32_t irq = save_and_disable_interrupts();

    for (int i = 0; i < BTN_COUNT; i++)
    {
        frame_pressed[i] = (pending_presses[i] > 0);
        pending_presses[i] = 0;
    }

    restore_interrupts(irq);
}

bool button_down(Button button)
{
    if (button < 0 || button >= BTN_COUNT)
        return false;

    return stable_down[button];
}

bool button_just_pressed(Button button)
{
    if (button < 0 || button >= BTN_COUNT)
        return false;

    return frame_pressed[button];
}

void buttons_clear(void)
{
    uint32_t irq = save_and_disable_interrupts();

    for (int i = 0; i < BTN_COUNT; i++)
    {
        pending_presses[i] = 0;
        frame_pressed[i] = false;
    }

    restore_interrupts(irq);
}

int button_pressed(uint gpio)
{
    // Buttons are active-low:
    // LOW  (0) -> Pressed
    // HIGH (1) -> Released
    return (gpio_get(gpio) == 0);
}
