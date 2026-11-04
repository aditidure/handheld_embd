#include "sound.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define SOUND_PIN 15
#define PWM_CLK_DIV 8.0f

static uint slice_num;

void sound_init(void)
{
    gpio_set_function(SOUND_PIN, GPIO_FUNC_PWM);

    slice_num = pwm_gpio_to_slice_num(SOUND_PIN);

    pwm_set_clkdiv(slice_num, PWM_CLK_DIV);
    pwm_set_wrap(slice_num, 1000);

    pwm_set_gpio_level(SOUND_PIN, 0);

    pwm_set_enabled(slice_num, true);
}

void sound_play_tone(uint16_t frequency, uint32_t duration_ms)
{
    if (frequency < 300 || frequency > 4000)
        return;

    uint32_t clock = clock_get_hz(clk_sys);

    uint32_t top = (clock / (8 * frequency)) - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(SOUND_PIN, top / 2);

    sleep_ms(duration_ms);

    sound_stop();
}

void sound_stop(void)
{
    pwm_set_gpio_level(SOUND_PIN, 0);
}