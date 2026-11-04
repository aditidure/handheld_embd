#include "pico/stdlib.h"
#include "sound/sound.h"

int main()
{
    stdio_init_all();

    sound_init();

    while (true)
    {
        sound_play_tone(440, 500);
        sleep_ms(300);

        sound_play_tone(880, 500);
        sleep_ms(300);

        sound_play_tone(1200, 500);
        sleep_ms(1000);
    }

    return 0;
}