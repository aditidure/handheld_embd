#include "save/save.h"

#include <stddef.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"

// --------------------------------------------------
// Flash layout
// --------------------------------------------------

// Last sector of flash, far away from the program image.
#define SAVE_SECTOR_OFFSET  (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define PAGES_PER_SECTOR    ((int)(FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE))

#define SAVE_MAGIC          0x48485343u   // "HHSC"
#define SAVE_VERSION        1u

#define FLASH_TIMEOUT_MS    100

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;                 // increases with every save
    uint32_t scores[SAVE_SLOT_COUNT];
    uint32_t checksum;                 // CRC32 of all fields above
} SaveRecord;

_Static_assert(sizeof(SaveRecord) <= FLASH_PAGE_SIZE,
               "SaveRecord must fit in one flash page");

// --------------------------------------------------
// State
// --------------------------------------------------

static SaveRecord current;          // copy of the newest record
static int current_page = -1;       // page holding it, -1 if none

// Page buffer passed to the flash programmer (must be in RAM).
static uint8_t page_buffer[FLASH_PAGE_SIZE];

// --------------------------------------------------
// Helpers
// --------------------------------------------------

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }

    return ~crc;
}

static uint32_t record_checksum(const SaveRecord *record)
{
    return crc32((const uint8_t *)record, offsetof(SaveRecord, checksum));
}

// Flash is memory-mapped (XIP), so a page can be read directly.
static const uint8_t *page_address(int page)
{
    return (const uint8_t *)(XIP_BASE + SAVE_SECTOR_OFFSET +
                             (uint32_t)page * FLASH_PAGE_SIZE);
}

static bool record_valid(const SaveRecord *record)
{
    return record->magic == SAVE_MAGIC &&
           record->version == SAVE_VERSION &&
           record->checksum == record_checksum(record);
}

static bool page_erased(int page)
{
    const uint8_t *p = page_address(page);

    for (int i = 0; i < (int)FLASH_PAGE_SIZE; i++)
    {
        if (p[i] != 0xFF)
            return false;
    }

    return true;
}

// --------------------------------------------------
// Flash write (runs with interrupts / other core paused)
// --------------------------------------------------

typedef struct
{
    bool erase_first;
    int page;
} FlashJob;

static void flash_job(void *param)
{
    const FlashJob *job = (const FlashJob *)param;

    if (job->erase_first)
        flash_range_erase(SAVE_SECTOR_OFFSET, FLASH_SECTOR_SIZE);

    flash_range_program(SAVE_SECTOR_OFFSET +
                            (uint32_t)job->page * FLASH_PAGE_SIZE,
                        page_buffer, FLASH_PAGE_SIZE);
}

static bool write_current(void)
{
    // Use the next page; erase the sector when it is full or when
    // the next page is not blank (e.g. old/foreign data).
    FlashJob job;
    job.page = current_page + 1;
    job.erase_first = false;

    if (job.page >= PAGES_PER_SECTOR || !page_erased(job.page))
    {
        job.page = 0;
        job.erase_first = true;
    }

    current.magic = SAVE_MAGIC;
    current.version = SAVE_VERSION;
    current.sequence++;
    current.checksum = record_checksum(&current);

    memset(page_buffer, 0xFF, sizeof(page_buffer));
    memcpy(page_buffer, &current, sizeof(current));

    // flash_safe_execute pauses interrupts (and core 1, if it is
    // running) while flash is busy, since code runs from flash.
    int result = flash_safe_execute(flash_job, &job, FLASH_TIMEOUT_MS);

    if (result != PICO_OK)
        return false;

    current_page = job.page;
    return true;
}

// --------------------------------------------------
// Public API
// --------------------------------------------------

void save_init(void)
{
    memset(&current, 0, sizeof(current));
    current_page = -1;

    // Pick the valid record with the highest sequence number.
    for (int page = 0; page < PAGES_PER_SECTOR; page++)
    {
        const SaveRecord *record = (const SaveRecord *)page_address(page);

        if (!record_valid(record))
            continue;

        if (current_page < 0 || record->sequence > current.sequence)
        {
            current = *record;
            current_page = page;
        }
    }
}

uint32_t save_get_best(SaveSlot slot)
{
    if ((unsigned)slot >= SAVE_SLOT_COUNT)
        return 0;

    return current.scores[slot];
}

bool save_submit_score(SaveSlot slot, uint32_t score)
{
    if ((unsigned)slot >= SAVE_SLOT_COUNT)
        return false;

    if (score <= current.scores[slot])
        return false;

    current.scores[slot] = score;
    write_current();

    // Even if the flash write failed, the new best is kept in RAM
    // for this session, so the player still sees "NEW BEST".
    return true;
}

void save_reset_all(void)
{
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        current.scores[i] = 0;

    write_current();
}
