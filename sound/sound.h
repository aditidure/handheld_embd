#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void sound_init(void);
void sound_play_tone(uint16_t frequency, uint32_t duration_ms);
void sound_stop(void);

#endif