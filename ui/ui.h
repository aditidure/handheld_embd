#ifndef UI_H
#define UI_H

#include <stdint.h>

#include "display/display.h"

// ==========================================
// SHARED UI PALETTE AND DRAWING HELPERS
// Used by the menu and all games so they share one look.
// ==========================================

// Convert 8-bit RGB to RGB565.
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define COL_HUD_BG       RGB565(10, 14, 40)
#define COL_PANEL_BG     RGB565(12, 16, 48)
#define COL_ACCENT       RGB565(0, 200, 255)
#define COL_ACCENT_DIM   RGB565(0, 70, 100)
#define COL_LABEL        RGB565(140, 150, 180)
#define COL_DIM          RGB565(60, 64, 80)
#define COL_ORANGE       RGB565(255, 140, 0)
#define COL_DARK_RED     RGB565(120, 20, 0)
#define COL_PINK         RGB565(255, 60, 90)
#define COL_CYAN         RGB565(0, 230, 255)

// Width in pixels of a string drawn with the 5x7 font.
int ui_text_width(const char *text, int scale);

// Draw text / a number horizontally centred on the screen.
void ui_text_centered(int y, const char *text, uint16_t color, int scale);
void ui_number_centered(int y, int number, uint16_t color, int scale);

void ui_fill_circle(int cx, int cy, int r, uint16_t color);
void ui_draw_circle(int cx, int cy, int r, uint16_t color);

void ui_fill_triangle(int x0, int y0, int x1, int y1,
                      int x2, int y2, uint16_t color);

// Filled box with a double cyan border.
void ui_draw_panel(int x, int y, int w, int h);

#endif
