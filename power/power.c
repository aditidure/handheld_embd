#include "power/power.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

#include "display/display.h"
#include "input/buttons.h"
#include "input/joystick.h"
#include "sound/sound.h"
#include "ui/ui.h"

// Joystick deflection that counts as "the player is doing something".
#define JOY_ACTIVITY_THRESHOLD  0.3f

// Last time we saw activity other than a button press (joystick
// movement, or waking up).
static uint32_t last_other_activity_ms = 0;

static uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static void show_sleep_message(void)
{
    display_clear(BLACK);
    ui_text_centered(SCREEN_HEIGHT / 2 - 14, "SLEEPING", COL_ACCENT, 3);
    ui_text_centered(SCREEN_HEIGHT / 2 + 16, "PRESS ANY BUTTON TO WAKE",
                     COL_LABEL, 1);
    display_show();
    sleep_ms(600);
}

void power_sleep(void)
{
    uint32_t sleep_start = now_ms();

    sound_stop();
    show_sleep_message();
    display_sleep();

    // Wait until the button that triggered sleep has been released,
    // otherwise it would wake us straight away.
    while (buttons_any_down())
        __wfi();

    uint32_t normal_khz = clock_get_hz(clk_sys) / 1000;
    bool clock_lowered = false;

#if POWER_SLEEP_CLOCK_KHZ > 0
    clock_lowered = set_sys_clock_khz(POWER_SLEEP_CLOCK_KHZ, false);
#endif

    // Sleep until a button is pressed. WFI halts the core until the
    // next interrupt (the 1 ms button sampler), then we re-check.
    while (!buttons_any_down())
        __wfi();

    if (clock_lowered)
        set_sys_clock_khz(normal_khz, true);

    display_wake();

    // Swallow the wake-up press.
    while (buttons_any_down())
        __wfi();
    buttons_clear();

    last_other_activity_ms = now_ms();

    printf("[power] slept for %lu ms\n",
           (unsigned long)(last_other_activity_ms - sleep_start));
}

bool power_sleep_if_idle(void)
{
    uint32_t now = now_ms();

    float jx = joystick_read_x();
    float jy = joystick_read_y();

    if (jx > JOY_ACTIVITY_THRESHOLD || jx < -JOY_ACTIVITY_THRESHOLD ||
        jy > JOY_ACTIVITY_THRESHOLD || jy < -JOY_ACTIVITY_THRESHOLD)
    {
        last_other_activity_ms = now;
    }

    uint32_t last_activity = buttons_last_press_ms();
    if ((int32_t)(last_other_activity_ms - last_activity) > 0)
        last_activity = last_other_activity_ms;

    if (now - last_activity < POWER_IDLE_TIMEOUT_MS)
        return false;

    power_sleep();
    return true;
}
