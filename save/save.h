#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stdint.h>

// ==========================================
// PERSISTENT BEST SCORES (stored in flash)
// ==========================================
//
// Best scores are kept in the last 4 KB sector of the Pico's flash,
// so they survive power-off. The sector holds 16 pages of 256 bytes.
// Each save writes a new record to the next empty page; the sector
// is erased only when all 16 pages are used (wear levelling).
// Every record has a magic number, version, sequence number and
// CRC32, so a half-written record (power lost mid-save) is ignored
// and the previous good record is used instead.

typedef enum
{
    SAVE_SPACE_SHOOTER = 0,
    SAVE_SNAKE,
    SAVE_SLOT_COUNT
} SaveSlot;

// Load the newest valid record from flash. Call once at startup.
void save_init(void);

// Best score stored for a game (0 if none yet).
uint32_t save_get_best(SaveSlot slot);

// Submit a finished game's score. If it beats the stored best it is
// written to flash and true is returned ("new best").
bool save_submit_score(SaveSlot slot, uint32_t score);

// Reset every stored best score to 0 (writes to flash).
void save_reset_all(void);

#endif
