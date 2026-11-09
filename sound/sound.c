#include "sound.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

#define SOUND_PIN        15

// The sound engine runs every millisecond from a timer interrupt.
#define TICK_US          1000

// Duty cycle divisors: music is played softer than sound effects
// so effects stand out (1/8 duty vs 1/2 duty).
#define MUSIC_DUTY_DIV   8
#define SFX_DUTY_DIV     2

// Fraction of each music note that is actually sounded. The short
// gap at the end separates repeated notes (staccato feel).
#define NOTE_GATE_NUM    7
#define NOTE_GATE_DEN    8

#define MIN_FREQ         20
#define MAX_FREQ         8000

static uint slice_num;
static uint32_t sys_clock_hz;
static repeating_timer_t tick_timer;

// What the PWM is currently outputting.
static uint16_t out_freq = 0;
static uint8_t out_duty_div = 0;

// Music state.
static const Song *volatile music_song = NULL;
static volatile bool music_playing = false;
static uint16_t music_index = 0;
static uint32_t music_elapsed = 0;

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

static void output(uint16_t freq, uint8_t duty_div)
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

    if (freq == out_freq && duty_div == out_duty_div)
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
    pwm_set_gpio_level(SOUND_PIN, (uint16_t)(top / duty_div));

    out_freq = freq;
    out_duty_div = duty_div;
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

static uint16_t tick_music(void)
{
    if (!music_playing)
        return 0;

    const Song *song = music_song;
    const MusicNote *note = &song->notes[music_index];

    uint32_t length = (uint32_t)note->units * song->unit_ms;
    uint32_t gate = length * NOTE_GATE_NUM / NOTE_GATE_DEN;

    uint16_t freq = (music_elapsed < gate) ? note->freq : 0;

    music_elapsed += TICK_US / 1000;

    if (music_elapsed >= length)
    {
        music_elapsed = 0;
        music_index++;

        if (music_index >= song->length)
        {
            music_index = 0;
            if (!song->loop)
                music_playing = false;
        }
    }

    return freq;
}

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

    // Music always advances so it stays in time while muted.
    uint16_t music_freq = tick_music();

    uint16_t sfx_freq = 0;
    bool sfx_active = tick_sfx(&sfx_freq);

    if (tone_remaining > 0)
    {
        output(tone_freq, SFX_DUTY_DIV);
        tone_remaining--;
    }
    else if (sfx_active)
    {
        output(sfx_freq, SFX_DUTY_DIV);
    }
    else
    {
        output(music_freq, MUSIC_DUTY_DIV);
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

void sound_music_play(const Song *song)
{
    if (song == NULL || song->length == 0)
        return;

    if (music_playing && music_song == song)
        return;

    uint32_t irq = save_and_disable_interrupts();
    music_song = song;
    music_index = 0;
    music_elapsed = 0;
    music_playing = true;
    restore_interrupts(irq);
}

void sound_music_stop(void)
{
    uint32_t irq = save_and_disable_interrupts();
    music_playing = false;
    music_song = NULL;
    restore_interrupts(irq);
}

bool sound_music_is_playing(void)
{
    return music_playing;
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
    music_playing = false;
    music_song = NULL;
    sfx_playing = false;
    tone_remaining = 0;
    restore_interrupts(irq);
}
