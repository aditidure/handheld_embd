#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// Orientation is chosen per program. Define DISPLAY_LANDSCAPE
// (e.g. with target_compile_definitions in CMakeLists.txt) to get
// a 320x240 landscape screen; otherwise it is 240x320 portrait.
#ifdef DISPLAY_LANDSCAPE
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#else
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#endif

// RGB565 colours
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0

void display_init(void);

void display_clear(uint16_t color);

void display_fill_rect(int x, int y,
                       int width, int height,
                       uint16_t color);

void display_show(void);
void display_draw_pixel(int x, int y, uint16_t color);

void display_draw_line(int x0, int y0,
                       int x1, int y1,
                       uint16_t color);

void display_draw_rect(int x, int y,
                       int width, int height,
                       uint16_t color);

                       // Text rendering
void display_draw_char(int x, int y, char c,
                       uint16_t color, int scale);

void display_draw_string(int x, int y, const char *text,
                         uint16_t color, int scale);

void display_draw_number(int x, int y, int number,
                         uint16_t color, int scale);

// --------------------------------------------------
// Performance measurement
// --------------------------------------------------
// display_show() times every frame. Stats are averaged over
// one-second windows.

typedef struct
{
    float fps;              // frames shown per second
    float frame_ms;         // average time between frames
    float worst_frame_ms;   // slowest frame in the window
    float transfer_ms;      // average DMA transfer time to the panel
    float wait_ms;          // core 0 time blocked waiting for display
    float render_ms;        // core 0 time doing game logic + drawing
} DisplayStats;

// Latest one-second stats (all zero until the first second passes).
DisplayStats display_get_stats(void);

// Small "FPS nn" box drawn in the bottom-left corner of every frame.
void display_set_fps_overlay(bool enabled);
bool display_get_fps_overlay(void);

// Print the stats over USB serial once per second.
void display_set_fps_logging(bool enabled);
#endif