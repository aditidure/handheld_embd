// ==========================================
// SOUND TEST
// ==========================================
//
// Plays a C-major scale through the PWM pin and amplifier, then a
// sliding sound effect, so you can check wiring and volume.

#include "pico/stdlib.h"
#include "sound/sound.h"

static const uint16_t scale[] = {
    NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5,
    NOTE_G5, NOTE_A5, NOTE_B5, NOTE_C6,
};

static const SfxSegment sweep_segs[] = {
    { 400, 2000, 600, false },   // rising sweep
    { 2000, 400, 600, false },   // falling sweep
};
static const SoundEffect SWEEP = SOUND_EFFECT(sweep_segs, 1);

int main(void)
{
    stdio_init_all();
    sound_init();

    while (true)
    {
        for (int i = 0; i < (int)(sizeof(scale) / sizeof(scale[0])); i++)
        {
            sound_play_tone(scale[i], 250);   // blocking: waits 250 ms
            sleep_ms(50);
        }

        sleep_ms(500);

        sound_sfx_play(&SWEEP);               // non-blocking
        sleep_ms(1500);
    }
}
