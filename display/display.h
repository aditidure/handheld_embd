#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

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
#endif