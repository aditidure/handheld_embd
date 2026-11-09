#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>

#include "sound/notes.h"

// ==========================================
// MUSIC
// ==========================================
//
// A song is a list of notes. Each note lasts `units` beats of
// `unit_ms` milliseconds. freq = REST (0) is a silent beat.
// Music plays in the background (timer interrupt), so the game
// loop never has to wait for it.

typedef struct
{
    uint16_t freq;
    uint8_t units;
} MusicNote;

typedef struct
{
    const MusicNote *notes;
    uint16_t length;
    uint16_t unit_ms;
    bool loop;
} Song;

// ==========================================
// SOUND EFFECTS
// ==========================================
//
// An effect is a short list of segments. Each segment slides from
// freq_start to freq_end over `ms` milliseconds. `noise` adds random
// jitter for explosion-like sounds. While an effect plays it takes
// over the buzzer and the music is muted (but keeps its timing).

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
    // lower priority.
    uint8_t priority;
} SoundEffect;

#define SOUND_EFFECT(segs, prio) \
    { (segs), (uint8_t)(sizeof(segs) / sizeof((segs)[0])), (prio) }

#define SONG(notes, unit, looping) \
    { (notes), (uint16_t)(sizeof(notes) / sizeof((notes)[0])), (unit), (looping) }

// ==========================================
// API
// ==========================================

void sound_init(void);

// Start a song. Calling it again with the song already playing
// does nothing, so it is safe to call every frame.
void sound_music_play(const Song *song);
void sound_music_stop(void);
bool sound_music_is_playing(void);

// Play a sound effect without blocking.
void sound_sfx_play(const SoundEffect *effect);

// Play a single tone and WAIT until it finishes (blocking).
// Kept for older code such as the Snake game and sound test.
void sound_play_tone(uint16_t frequency, uint32_t duration_ms);

// Silence everything: music, effects and tones.
void sound_stop(void);

#endif
