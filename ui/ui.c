#include "ui/ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int ui_text_width(const char *text, int scale)
{
    int len = (int)strlen(text);
    return (len > 0) ? len * 6 * scale - scale : 0;
}

void ui_text_centered(int y, const char *text, uint16_t color, int scale)
{
    int x = (SCREEN_WIDTH - ui_text_width(text, scale)) / 2;
    display_draw_string(x, y, text, color, scale);
}

void ui_number_centered(int y, int number, uint16_t color, int scale)
{
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", number);
    ui_text_centered(y, buffer, color, scale);
}

void ui_fill_circle(int cx, int cy, int r, uint16_t color)
{
    if (r <= 0)
    {
        display_draw_pixel(cx, cy, color);
        return;
    }

    for (int dy = -r; dy <= r; dy++)
    {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        display_fill_rect(cx - dx, cy + dy, 2 * dx + 1, 1, color);
    }
}

void ui_draw_circle(int cx, int cy, int r, uint16_t color)
{
    int x = r;
    int y = 0;
    int err = 1 - r;

    while (x >= y)
    {
        display_draw_pixel(cx + x, cy + y, color);
        display_draw_pixel(cx + y, cy + x, color);
        display_draw_pixel(cx - y, cy + x, color);
        display_draw_pixel(cx - x, cy + y, color);
        display_draw_pixel(cx - x, cy - y, color);
        display_draw_pixel(cx - y, cy - x, color);
        display_draw_pixel(cx + y, cy - x, color);
        display_draw_pixel(cx + x, cy - y, color);

        y++;
        if (err < 0)
        {
            err += 2 * y + 1;
        }
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void ui_fill_triangle(int x0, int y0, int x1, int y1,
                      int x2, int y2, uint16_t color)
{
    int t;

    // Sort vertices by y (y0 <= y1 <= y2).
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }

    if (y0 == y2)
    {
        int min_x = x0, max_x = x0;
        if (x1 < min_x) min_x = x1;
        if (x2 < min_x) min_x = x2;
        if (x1 > max_x) max_x = x1;
        if (x2 > max_x) max_x = x2;
        display_fill_rect(min_x, y0, max_x - min_x + 1, 1, color);
        return;
    }

    for (int y = y0; y <= y2; y++)
    {
        float xa = (float)x0 +
                   (float)(x2 - x0) * (float)(y - y0) / (float)(y2 - y0);
        float xb;

        if (y < y1)
            xb = (float)x0 +
                 (float)(x1 - x0) * (float)(y - y0) / (float)(y1 - y0);
        else if (y2 == y1)
            xb = (float)x1;
        else
            xb = (float)x1 +
                 (float)(x2 - x1) * (float)(y - y1) / (float)(y2 - y1);

        int a = (int)(xa + 0.5f);
        int b = (int)(xb + 0.5f);
        if (a > b) { t = a; a = b; b = t; }

        display_fill_rect(a, y, b - a + 1, 1, color);
    }
}

void ui_draw_panel(int x, int y, int w, int h)
{
    display_fill_rect(x, y, w, h, COL_PANEL_BG);
    display_draw_rect(x, y, w, h, COL_ACCENT);
    display_draw_rect(x + 2, y + 2, w - 4, h - 4, COL_ACCENT_DIM);
}

void ui_draw_pause_overlay(void)
{
    const int w = 200;
    const int h = 100;
    const int x = (SCREEN_WIDTH - w) / 2;
    const int y = (SCREEN_HEIGHT - h) / 2;

    ui_draw_panel(x, y, w, h);

    ui_text_centered(y + 16, "PAUSED", YELLOW, 3);
    display_draw_line(x + 20, y + 46, x + w - 21, y + 46, COL_ACCENT_DIM);

    ui_text_centered(y + 58, "CLICK: RESUME", WHITE, 1);
    ui_text_centered(y + 76, "D: QUIT TO MENU", COL_LABEL, 1);
}
