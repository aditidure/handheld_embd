#ifndef SOUND_H
#define SOUND_H

// ==========================================
// SOUND (piezo buzzer on GP15, driven by PWM)
// ==========================================
//
// A 1 ms timer interrupt drives the buzzer, so sound effects play in
// the background and never pause the game loop.
//
// A sound effect is a short list of segments. Each segment slides
// its pitch from freq_start to freq_end over `ms` milliseconds;
// `noise` adds random pitch jitter for explosion-like sounds.
// A segment with both frequencies 0 is a silent gap.

#include <stdint.h>
#include <stdbool.h>

#include "sound/notes.h"

typedef struct
{
    uint16_t freq_start;
    uint16_t freq_end;
    uint16_t ms;
    bool noise;
} SfxSegment;

typedef struct
{
    const SfxSegment *segments;
    uint8_t count;
    // A new effect only interrupts a playing one of equal or
    // lower priority (e.g. a laser shot never cuts off a crash).
    uint8_t priority;
} SoundEffect;

// Build a SoundEffect from a static SfxSegment array.
#define SOUND_EFFECT(segs, prio) \
    { (segs), (uint8_t)(sizeof(segs) / sizeof((segs)[0])), (prio) }

void sound_init(void);

// Start a sound effect without blocking.
void sound_sfx_play(const SoundEffect *effect);

// Play a single tone and WAIT until it finishes (blocking).
// Used by the Snake test program and the sound test.
void sound_play_tone(uint16_t frequency, uint32_t duration_ms);

// Silence everything immediately.
void sound_stop(void);

#endif
