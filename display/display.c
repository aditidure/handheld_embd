
#include "display.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

// SPI configuration
#define SPI_PORT spi0
#define SPI_BAUDRATE 80000000

// Display pins
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_CS   17
#define PIN_DC   16
#define PIN_RST  20

// ST7789 MADCTL (memory access control) value sets the rotation.
//   0x00 = portrait
//   0x60 = landscape (MV | MX)
//   0xA0 = landscape rotated 180 degrees (MV | MY)
// If the landscape picture appears upside down, change 0x60 to 0xA0.
#ifdef DISPLAY_LANDSCAPE
#define DISPLAY_MADCTL 0x60
#else
#define DISPLAY_MADCTL 0x00
#endif

// --------------------------------------------------
// FRAMEBUFFERS AND DUAL-CORE RENDERING
// --------------------------------------------------
//
// DISPLAY_DUAL_CORE = 1 (default):
//   Two framebuffers. Core 0 draws the next frame into the "back"
//   buffer while core 1 sends the previous frame to the panel over
//   SPI using DMA. display_show() hands the finished buffer to
//   core 1 and swaps, so drawing and transferring overlap.
//
// DISPLAY_DUAL_CORE = 0:
//   One framebuffer; core 0 draws, then waits for the transfer.
//   Useful to measure the single-core baseline for comparison.

#ifndef DISPLAY_DUAL_CORE
#define DISPLAY_DUAL_CORE 1
#endif

#define FRAME_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT)
#define FRAME_BYTES  (FRAME_PIXELS * sizeof(uint16_t))

#if DISPLAY_DUAL_CORE
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/sync.h"
#define NUM_FRAMEBUFFERS 2
#else
#define NUM_FRAMEBUFFERS 1
#endif

static uint16_t framebuffers[NUM_FRAMEBUFFERS][FRAME_PIXELS];

// Buffer that all drawing functions write into (owned by core 0).
static uint16_t *framebuffer = framebuffers[0];
static int back_index = 0;

#if DISPLAY_DUAL_CORE
static void core1_display_task(void);
#endif

// DMA
static int dma_channel;


// --------------------------------------------------
// FONT TABLE (5x7)
// --------------------------------------------------

static const uint8_t font5x7[39][5] = {

    // Digits 0-9
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9

    // Letters A-Z
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z

    // Punctuation
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x00,0x00,0x00,0x00}  // Space
};


// --------------------------------------------------
// BYTE SWAPPING
// --------------------------------------------------

static uint16_t swap_bytes(uint16_t color)
{
    return (color >> 8) | (color << 8);
}


// --------------------------------------------------
// FONT LOOKUP
// --------------------------------------------------

static const uint8_t* get_glyph(char c)
{
    c = (char)toupper((unsigned char)c);

    if (c >= '0' && c <= '9')
        return font5x7[c - '0'];

    if (c >= 'A' && c <= 'Z')
        return font5x7[10 + c - 'A'];

    if (c == ':')
        return font5x7[36];

    if (c == '-')
        return font5x7[37];

    if (c == ' ')
        return font5x7[38];

    return font5x7[38];
}


// --------------------------------------------------
// DISPLAY COMMANDS
// --------------------------------------------------

static void write_command(uint8_t command)
{
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);

    spi_write_blocking(SPI_PORT, &command, 1);

    gpio_put(PIN_CS, 1);
}


static void write_data(uint8_t *data, size_t length)
{
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);

    spi_write_blocking(SPI_PORT, data, length);

    gpio_put(PIN_CS, 1);
}


static void write_data_byte(uint8_t data)
{
    write_data(&data, 1);
}


// --------------------------------------------------
// DISPLAY ADDRESS WINDOW
// --------------------------------------------------

static void set_address_window(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    write_command(0x2A);

    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;

    write_data(data, 4);

    write_command(0x2B);

    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;

    write_data(data, 4);
}


// --------------------------------------------------
// ST7789 INITIALIZATION
// --------------------------------------------------

static void st7789_init(void)
{
    write_command(0x01);
    sleep_ms(150);

    write_command(0x11);
    sleep_ms(120);

    write_command(0x3A);
    write_data_byte(0x55);

    write_command(0x36);
    write_data_byte(DISPLAY_MADCTL);

    write_command(0x20);

    write_command(0x13);

    write_command(0x29);
    sleep_ms(100);
}


// --------------------------------------------------
// DISPLAY INITIALIZATION
// --------------------------------------------------

void display_init(void)
{
    spi_init(SPI_PORT, SPI_BAUDRATE);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    gpio_put(PIN_CS, 1);

    gpio_put(PIN_RST, 1);
    sleep_ms(10);

    gpio_put(PIN_RST, 0);
    sleep_ms(10);

    gpio_put(PIN_RST, 1);
    sleep_ms(120);

    st7789_init();

    dma_channel = dma_claim_unused_channel(true);

#if DISPLAY_DUAL_CORE
    // Core 1 now owns the SPI/DMA transfers to the panel.
    multicore_launch_core1(core1_display_task);
#endif
}


// --------------------------------------------------
// CLEAR FRAMEBUFFER
// --------------------------------------------------

void display_clear(uint16_t color)
{
    uint16_t swapped = swap_bytes(color);

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        framebuffer[i] = swapped;
    }
}


// --------------------------------------------------
// DRAW FILLED RECTANGLE
// --------------------------------------------------

void display_fill_rect(int x, int y,
                       int width, int height,
                       uint16_t color)
{
    if (width <= 0 || height <= 0)
        return;

    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if (x + width > SCREEN_WIDTH)
        width = SCREEN_WIDTH - x;

    if (y + height > SCREEN_HEIGHT)
        height = SCREEN_HEIGHT - y;

    if (width <= 0 || height <= 0)
        return;

    uint16_t swapped = swap_bytes(color);

    for (int row = y; row < y + height; row++)
    {
        for (int col = x; col < x + width; col++)
        {
            framebuffer[row * SCREEN_WIDTH + col] = swapped;
        }
    }
}


// --------------------------------------------------
// FRAME TRANSFER (SPI + DMA)
// --------------------------------------------------

// Send one whole framebuffer to the panel and wait until done.
// Runs on core 1 in dual-core mode, on core 0 otherwise.
static void transfer_frame(const uint16_t *buffer)
{
    set_address_window(0, 0,
                       SCREEN_WIDTH - 1,
                       SCREEN_HEIGHT - 1);

    write_command(0x2C);

    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);

    dma_channel_config config =
        dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, spi_get_dreq(SPI_PORT, true));

    dma_channel_configure(
        dma_channel,
        &config,
        &spi_get_hw(SPI_PORT)->dr,
        buffer,
        FRAME_BYTES,
        true
    );

    dma_channel_wait_for_finish_blocking(dma_channel);

    while (spi_is_busy(SPI_PORT))
    {
        tight_loop_contents();
    }

    gpio_put(PIN_CS, 1);
}


#if DISPLAY_DUAL_CORE

// --------------------------------------------------
// CORE 1: DISPLAY TASK
// --------------------------------------------------
//
// Hand-over protocol (shared variables + memory barriers):
//   core 0 sets transfer_index, then transfer_busy = true, then SEV.
//   core 1 sends that buffer, sets transfer_busy = false, then SEV.
// WFE puts the waiting core to sleep until the other signals.

static volatile bool transfer_busy = false;
static volatile int transfer_index = 0;
static volatile uint32_t core1_transfer_us = 0;

static void core1_display_task(void)
{
    // Lets core 0 safely pause this core while it writes to flash
    // (saving best scores), since code is executed from flash.
    flash_safe_execute_core_init();

    while (true)
    {
        while (!transfer_busy)
            __wfe();

        __dmb();

        uint32_t start = time_us_32();
        transfer_frame(framebuffers[transfer_index]);
        core1_transfer_us = time_us_32() - start;

        __dmb();
        transfer_busy = false;
        __sev();
    }
}

#endif


// --------------------------------------------------
// PERFORMANCE STATS
// --------------------------------------------------

#define STATS_WINDOW_US  1000000u

static DisplayStats stats;
static bool fps_overlay = false;
static bool fps_logging = false;

static uint32_t window_start_us = 0;
static uint32_t window_frames = 0;
static uint64_t window_transfer_us = 0;
static uint64_t window_wait_us = 0;
static uint32_t window_worst_us = 0;
static uint32_t last_frame_end_us = 0;

static void draw_fps_overlay(void)
{
    char text[12];
    snprintf(text, sizeof(text), "FPS %d", (int)(stats.fps + 0.5f));

    int width = (int)strlen(text) * 6 + 3;
    int y = SCREEN_HEIGHT - 11;

    display_fill_rect(0, y, width, 11, BLACK);
    display_draw_string(2, y + 2, text, YELLOW, 1);
}

// Called once per frame on core 0.
//   frame_end_us : when display_show() finished
//   wait_us      : time core 0 spent blocked on the display
//   transfer_us  : duration of a frame transfer
static void update_stats(uint32_t frame_end_us, uint32_t wait_us,
                         uint32_t transfer_us)
{
    if (last_frame_end_us != 0)
    {
        uint32_t frame_us = frame_end_us - last_frame_end_us;
        if (frame_us > window_worst_us)
            window_worst_us = frame_us;
    }
    last_frame_end_us = frame_end_us;

    if (window_start_us == 0)
        window_start_us = frame_end_us;

    window_frames++;
    window_wait_us += wait_us;
    window_transfer_us += transfer_us;

    uint32_t elapsed = frame_end_us - window_start_us;
    if (elapsed < STATS_WINDOW_US)
        return;

    float frames = (float)window_frames;

    stats.fps = frames * 1000000.0f / (float)elapsed;
    stats.frame_ms = (float)elapsed / 1000.0f / frames;
    stats.worst_frame_ms = (float)window_worst_us / 1000.0f;
    stats.transfer_ms = (float)window_transfer_us / 1000.0f / frames;
    stats.wait_ms = (float)window_wait_us / 1000.0f / frames;
    stats.render_ms = stats.frame_ms - stats.wait_ms;

    if (fps_logging)
    {
        printf("[%s] FPS %.1f | frame %.1f ms (worst %.1f) | "
               "core0 render %.1f ms, wait %.1f ms | DMA %.1f ms\n",
               DISPLAY_DUAL_CORE ? "dual-core" : "single-core",
               stats.fps, stats.frame_ms, stats.worst_frame_ms,
               stats.render_ms, stats.wait_ms, stats.transfer_ms);
    }

    window_start_us = frame_end_us;
    window_frames = 0;
    window_transfer_us = 0;
    window_wait_us = 0;
    window_worst_us = 0;
}

DisplayStats display_get_stats(void)
{
    return stats;
}

void display_set_fps_overlay(bool enabled)
{
    fps_overlay = enabled;
}

bool display_get_fps_overlay(void)
{
    return fps_overlay;
}

void display_set_fps_logging(bool enabled)
{
    fps_logging = enabled;
}


// --------------------------------------------------
// SHOW FRAME
// --------------------------------------------------

void display_show(void)
{
    if (fps_overlay)
        draw_fps_overlay();

#if DISPLAY_DUAL_CORE
    // Wait until core 1 has finished sending the previous frame.
    // That buffer becomes our next back buffer.
    uint32_t wait_start = time_us_32();

    while (transfer_busy)
        __wfe();

    __dmb();
    uint32_t wait_us = time_us_32() - wait_start;
    uint32_t transfer_us = core1_transfer_us;

    // Hand the finished frame to core 1...
    transfer_index = back_index;
    __dmb();
    transfer_busy = true;
    __sev();

    // ...and draw the next frame into the other buffer meanwhile.
    back_index ^= 1;
    framebuffer = framebuffers[back_index];

    update_stats(time_us_32(), wait_us, transfer_us);
#else
    uint32_t start = time_us_32();
    transfer_frame(framebuffer);
    uint32_t transfer_us = time_us_32() - start;

    update_stats(time_us_32(), transfer_us, transfer_us);
#endif
}


// --------------------------------------------------
// PANEL POWER
// --------------------------------------------------

void display_sleep(void)
{
#if DISPLAY_DUAL_CORE
    // Let core 1 finish sending the last frame before we use SPI.
    while (transfer_busy)
        __wfe();
#endif

    write_command(0x28);   // DISPOFF
    write_command(0x10);   // SLPIN
    sleep_ms(5);
}

void display_wake(void)
{
    write_command(0x11);   // SLPOUT
    sleep_ms(120);         // panel needs 120 ms after sleep-out
    write_command(0x29);   // DISPON

    // Restart FPS measurement so the sleep is not counted as a frame.
    window_start_us = 0;
    window_frames = 0;
    window_transfer_us = 0;
    window_wait_us = 0;
    window_worst_us = 0;
    last_frame_end_us = 0;
}


// --------------------------------------------------
// DRAW PIXEL
// --------------------------------------------------

void display_draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= SCREEN_WIDTH ||
        y < 0 || y >= SCREEN_HEIGHT)
        return;

    framebuffer[y * SCREEN_WIDTH + x] = swap_bytes(color);
}


// --------------------------------------------------
// DRAW LINE
// --------------------------------------------------

void display_draw_line(int x0, int y0,
                       int x1, int y1,
                       uint16_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (1)
    {
        display_draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


// --------------------------------------------------
// DRAW RECTANGLE OUTLINE
// --------------------------------------------------

void display_draw_rect(int x, int y,
                       int width, int height,
                       uint16_t color)
{
    if (width <= 0 || height <= 0)
        return;

    display_draw_line(x, y,
                      x + width - 1, y, color);

    display_draw_line(x, y + height - 1,
                      x + width - 1, y + height - 1, color);

    display_draw_line(x, y,
                      x, y + height - 1, color);

    display_draw_line(x + width - 1, y,
                      x + width - 1, y + height - 1, color);
}


// --------------------------------------------------
// TEXT RENDERING
// --------------------------------------------------

void display_draw_char(int x, int y, char c,
                       uint16_t color, int scale)
{
    if (scale <= 0)
        return;

    const uint8_t *glyph = get_glyph(c);

    for (int col = 0; col < 5; col++)
    {
        for (int row = 0; row < 7; row++)
        {
            if (glyph[col] & (1u << row))
            {
                display_fill_rect(
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}


void display_draw_string(int x, int y, const char *text,
                         uint16_t color, int scale)
{
    if (text == NULL || scale <= 0)
        return;

    int cursor_x = x;

    while (*text)
    {
        display_draw_char(
            cursor_x,
            y,
            *text,
            color,
            scale
        );

        cursor_x += 6 * scale;
        text++;
    }
}


void display_draw_number(int x, int y, int number,
                         uint16_t color, int scale)
{
    char buffer[12];

    snprintf(buffer, sizeof(buffer), "%d", number);

    display_draw_string(x, y, buffer, color, scale);
}

