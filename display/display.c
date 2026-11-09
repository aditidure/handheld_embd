
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

// Framebuffer
static uint16_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

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
// DISPLAY FRAMEBUFFER USING DMA
// --------------------------------------------------

void display_show(void)
{
    set_address_window(0, 0,
                       SCREEN_WIDTH - 1,
                       SCREEN_HEIGHT - 1);

    write_command(0x2C);

    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);

    dma_channel_config config =
        dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(
        &config, DMA_SIZE_8);

    channel_config_set_read_increment(
        &config, true);

    channel_config_set_write_increment(
        &config, false);

    channel_config_set_dreq(
        &config, spi_get_dreq(SPI_PORT, true));

    dma_channel_configure(
        dma_channel,
        &config,
        &spi_get_hw(SPI_PORT)->dr,
        framebuffer,
        sizeof(framebuffer),
        true
    );

    dma_channel_wait_for_finish_blocking(dma_channel);

    while (spi_is_busy(SPI_PORT))
    {
        tight_loop_contents();
    }

    gpio_put(PIN_CS, 1);
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

