#include "sound.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

#define SOUND_PIN        15

// The sound engine runs every millisecond from a timer interrupt.
#define TICK_US          1000

// PWM duty cycle = top / DUTY_DIV. 2 means a 50% square wave, which
// gives the strongest fundamental tone at the amplifier input.
#define DUTY_DIV         2

#define MIN_FREQ         20
#define MAX_FREQ         8000

static uint slice_num;
static uint32_t sys_clock_hz;
static repeating_timer_t tick_timer;

// Frequency the PWM is currently outputting (0 = silent).
static uint16_t out_freq = 0;

// Sound effect state.
static const SoundEffect *volatile sfx_current = NULL;
static volatile bool sfx_playing = false;
static uint8_t sfx_index = 0;
static uint16_t sfx_elapsed = 0;

// Blocking single-tone state (sound_play_tone).
static volatile uint16_t tone_freq = 0;
static volatile uint32_t tone_remaining = 0;

// Pseudo-random generator for noise effects.
static uint16_t lfsr = 0xACE1u;

// ==========================================
// PWM OUTPUT
// ==========================================

static void output(uint16_t freq)
{
    if (freq == 0)
    {
        if (out_freq != 0)
        {
            pwm_set_gpio_level(SOUND_PIN, 0);
            out_freq = 0;
        }
        return;
    }

    if (freq < MIN_FREQ)
        freq = MIN_FREQ;
    if (freq > MAX_FREQ)
        freq = MAX_FREQ;

    if (freq == out_freq)
        return;

    // Choose the smallest clock divider that keeps the counter
    // wrap value within 16 bits, so low notes work too.
    uint32_t div = sys_clock_hz / ((uint32_t)freq * 65536u) + 1u;
    if (div > 255u)
        div = 255u;

    uint32_t top = sys_clock_hz / (div * freq) - 1u;
    if (top > 65535u)
        top = 65535u;

    pwm_set_clkdiv(slice_num, (float)div);
    pwm_set_wrap(slice_num, (uint16_t)top);
    pwm_set_gpio_level(SOUND_PIN, (uint16_t)(top / DUTY_DIV));

    out_freq = freq;
}

static uint16_t next_random(void)
{
    // 16-bit Galois LFSR.
    uint16_t lsb = lfsr & 1u;
    lfsr >>= 1;
    if (lsb)
        lfsr ^= 0xB400u;
    return lfsr;
}

// ==========================================
// TIMER TICK (runs every millisecond)
// ==========================================

// Advance the current effect by 1 ms. Returns false if none is
// playing, otherwise writes the frequency to output.
static bool tick_sfx(uint16_t *freq_out)
{
    if (!sfx_playing)
        return false;

    const SoundEffect *fx = sfx_current;
    const SfxSegment *seg = &fx->segments[sfx_index];

    int32_t start = seg->freq_start;
    int32_t end = seg->freq_end;
    int32_t freq = start;

    if (seg->ms > 0)
        freq = start + (end - start) * (int32_t)sfx_elapsed / (int32_t)seg->ms;

    // Noise: jitter the pitch randomly around the slide (+/- 50%).
    if (seg->noise && freq > 1)
        freq = freq / 2 + (int32_t)(next_random() % (uint32_t)freq);

    *freq_out = (start == 0 && end == 0) ? 0 : (uint16_t)freq;

    sfx_elapsed += TICK_US / 1000;

    if (sfx_elapsed >= seg->ms)
    {
        sfx_elapsed = 0;
        sfx_index++;

        if (sfx_index >= fx->count)
            sfx_playing = false;
    }

    return true;
}

static bool on_tick(repeating_timer_t *timer)
{
    (void)timer;

    uint16_t sfx_freq = 0;
    bool sfx_active = tick_sfx(&sfx_freq);

    if (tone_remaining > 0)
    {
        output(tone_freq);
        tone_remaining--;
    }
    else if (sfx_active)
    {
        output(sfx_freq);
    }
    else
    {
        output(0);
    }

    return true;
}

// ==========================================
// PUBLIC API
// ==========================================

void sound_init(void)
{
    gpio_set_function(SOUND_PIN, GPIO_FUNC_PWM);

    slice_num = pwm_gpio_to_slice_num(SOUND_PIN);
    sys_clock_hz = clock_get_hz(clk_sys);

    pwm_set_clkdiv(slice_num, 8.0f);
    pwm_set_wrap(slice_num, 1000);
    pwm_set_gpio_level(SOUND_PIN, 0);
    pwm_set_enabled(slice_num, true);

    // Negative interval = fixed rate between callback starts.
    add_repeating_timer_us(-TICK_US, on_tick, NULL, &tick_timer);
}

void sound_sfx_play(const SoundEffect *effect)
{
    if (effect == NULL || effect->count == 0)
        return;

    uint32_t irq = save_and_disable_interrupts();

    if (!sfx_playing || effect->priority >= sfx_current->priority)
    {
        sfx_current = effect;
        sfx_index = 0;
        sfx_elapsed = 0;
        sfx_playing = true;
    }

    restore_interrupts(irq);
}

void sound_play_tone(uint16_t frequency, uint32_t duration_ms)
{
    uint32_t irq = save_and_disable_interrupts();
    tone_freq = frequency;
    tone_remaining = duration_ms;
    restore_interrupts(irq);

    sleep_ms(duration_ms);
}

void sound_stop(void)
{
    uint32_t irq = save_and_disable_interrupts();
    sfx_playing = false;
    tone_remaining = 0;
    restore_interrupts(irq);
}
