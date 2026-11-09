#include "menu.h"

#include "pico/stdlib.h"
#include "display/display.h"
#include "input/buttons.h"
#include "input/joystick.h"
#include "sound/sound.h"
#include "ui/ui.h"

#include "space_shooter.h"
#include "snake.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// ==========================================
// CONSTANTS
// ==========================================


#define JOY_ON           0.6f   // stick must pass this to move
#define JOY_OFF          0.3f   // and return inside this to re-arm

#define FRAME_MS         16

#define CARD_W           130
#define CARD_H           124
#define CARD_GAP         20
#define CARD_Y           56

#define NUM_STARS        40

#define COL_CARD_BG      RGB565(8, 10, 24)
#define COL_ROCK         RGB565(120, 95, 75)
#define COL_SHIP_HULL    RGB565(210, 220, 235)
#define COL_SHIP_WING    RGB565(40, 110, 220)
#define COL_SNAKE_HEAD   RGB565(150, 255, 100)
#define COL_SNAKE_BODY   RGB565(50, 210, 80)
#define COL_APPLE        RGB565(235, 40, 50)

// ==========================================
// MUSIC AND SOUND EFFECTS (original)
// ==========================================

// No background music in the menu: only short effects.
static const SfxSegment move_segs[] = {
    {1300, 1300, 25, false},
};

static const SfxSegment select_segs[] = {
    {600, 1800, 120, false},
};

static const SoundEffect SFX_MOVE = SOUND_EFFECT(move_segs, 1);
static const SoundEffect SFX_SELECT = SOUND_EFFECT(select_segs, 5);

// ==========================================
// GAME ICONS
// ==========================================

static void draw_shooter_icon(int cx, int cy, uint32_t now)
{
    // A few stars.
    display_draw_pixel(cx - 40, cy - 20, WHITE);
    display_draw_pixel(cx + 35, cy + 18, COL_LABEL);
    display_draw_pixel(cx - 30, cy + 22, COL_LABEL);
    display_draw_pixel(cx + 42, cy - 5, WHITE);

    // Asteroid.
    ui_fill_circle(cx + 26, cy - 16, 7, COL_ROCK);
    ui_fill_circle(cx + 24, cy - 18, 2, COL_DIM);

    // Laser bolt, travelling up.
    int bolt_y = cy - 14 - (int)((now / 40) % 12);
    display_fill_rect(cx - 1, bolt_y, 3, 7, COL_ORANGE);
    display_fill_rect(cx, bolt_y + 1, 1, 5, YELLOW);

    // Ship.
    int sy = cy + 10;
    ui_fill_triangle(cx - 3, sy - 2, cx - 12, sy + 10, cx - 3, sy + 8,
                     COL_SHIP_WING);
    ui_fill_triangle(cx + 3, sy - 2, cx + 12, sy + 10, cx + 3, sy + 8,
                     COL_SHIP_WING);
    ui_fill_triangle(cx, sy - 12, cx - 5, sy + 9, cx + 5, sy + 9,
                     COL_SHIP_HULL);
    ui_fill_circle(cx, sy - 1, 2, COL_ACCENT);
}

static void draw_snake_icon(int cx, int cy, uint32_t now)
{
    // S-shaped snake made of 8x8 blocks on a 9 px grid.
    static const int8_t seg[][2] = {
        {-3, 1}, {-2, 1}, {-1, 1}, {-1, 0}, {-1, -1},
        {0, -1}, {1, -1}, {1, 0}, {1, 1}, {2, 1},
    };
    const int n = (int)(sizeof(seg) / sizeof(seg[0]));

    for (int i = 0; i < n; i++)
    {
        int x = cx + seg[i][0] * 9 - 4;
        int y = cy + seg[i][1] * 9 - 4;
        uint16_t color = (i == n - 1) ? COL_SNAKE_HEAD : COL_SNAKE_BODY;
        display_fill_rect(x, y, 8, 8, color);
    }

    // Eyes on the head (moving right).
    int hx = cx + 2 * 9 - 4;
    int hy = cy + 9 - 4;
    display_fill_rect(hx + 5, hy + 1, 2, 2, BLACK);
    display_fill_rect(hx + 5, hy + 5, 2, 2, BLACK);

    // Pulsing apple.
    int r = ((now / 300) % 2) ? 4 : 3;
    ui_fill_circle(cx + 38, cy + 10, r, COL_APPLE);
    display_fill_rect(cx + 38, cy + 4, 3, 2, COL_SNAKE_BODY);
}

// ==========================================
// GAME LIST
// To add a new game, add one entry here.
// ==========================================

typedef struct
{
    const char *name;
    void (*draw_icon)(int cx, int cy, uint32_t now);
    void (*run)(void);
    int (*best_score)(void);
} GameEntry;

static int shooter_best(void)
{
    return (int)space_shooter_best_score();
}

static const GameEntry games[] = {
    {"SHOOTER", draw_shooter_icon, space_shooter_run, shooter_best},
    {"SNAKE",   draw_snake_icon,   snake_game_run,    snake_game_best_score},
};

#define NUM_GAMES ((int)(sizeof(games) / sizeof(games[0])))

// ==========================================
// DRAWING
// ==========================================

static void draw_stars(uint32_t now)
{
    for (int i = 0; i < NUM_STARS; i++)
    {
        int x = (i * 73 + 17) % SCREEN_WIDTH;
        int y = (i * 131 + 29) % SCREEN_HEIGHT;
        bool bright = ((now / 300 + (uint32_t)i) % 5) == 0;
        display_draw_pixel(x, y, bright ? WHITE : COL_DIM);
    }
}

static void draw_card(int index, bool selected, uint32_t now)
{
    const GameEntry *g = &games[index];

    int total_w = NUM_GAMES * CARD_W + (NUM_GAMES - 1) * CARD_GAP;
    int x = (SCREEN_WIDTH - total_w) / 2 + index * (CARD_W + CARD_GAP);
    int y = CARD_Y;

    if (selected)
    {
        ui_draw_panel(x, y, CARD_W, CARD_H);

        // Bouncing arrow above the selected card.
        int ax = x + CARD_W / 2;
        int ay = y - 9 + (int)((now / 200) % 2);
        ui_fill_triangle(ax - 5, ay, ax + 5, ay, ax, ay + 5, YELLOW);
    }
    else
    {
        display_fill_rect(x, y, CARD_W, CARD_H, COL_CARD_BG);
        display_draw_rect(x, y, CARD_W, CARD_H, COL_DIM);
    }

    // Only the selected game's icon animates.
    g->draw_icon(x + CARD_W / 2, y + 46, selected ? now : 0);

    uint16_t name_color = selected ? WHITE : COL_LABEL;
    int name_x = x + (CARD_W - ui_text_width(g->name, 2)) / 2;
    display_draw_string(name_x, y + 84, g->name, name_color, 2);

    char best[20];
    snprintf(best, sizeof(best), "BEST %d", g->best_score());
    int best_x = x + (CARD_W - ui_text_width(best, 1)) / 2;
    display_draw_string(best_x, y + 106, best,
                        selected ? YELLOW : COL_DIM, 1);
}

static void draw_menu(int selected, uint32_t now)
{
    display_clear(BLACK);
    draw_stars(now);

    ui_text_centered(14, "SELECT GAME", COL_ACCENT, 3);
    display_draw_line(40, 42, SCREEN_WIDTH - 41, 42, COL_ACCENT_DIM);

    for (int i = 0; i < NUM_GAMES; i++)
        draw_card(i, i == selected, now);

    if ((now / 500) % 2 == 0)
        ui_text_centered(192, "CLICK TO PLAY", WHITE, 2);

    ui_text_centered(222, "JOYSTICK OR B D TO CHOOSE", COL_LABEL, 1);

    display_show();
}

// ==========================================
// MAIN LOOP
// ==========================================

void menu_run(void)
{
    int selected = 0;

    while (true)
    {
        // Ignore presses left over from the game we just left.
        buttons_clear();
        bool stick_armed = false;

        bool launch = false;

        while (!launch)
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());

            buttons_poll();

            int move = 0;
            if (button_just_pressed(BTN_D))
                move = -1;
            if (button_just_pressed(BTN_B))
                move = 1;

            // Joystick: one step per push, re-armed when centred.
            float jx = joystick_read_x();
            if (stick_armed && jx > JOY_ON)
            {
                move = 1;
                stick_armed = false;
            }
            else if (stick_armed && jx < -JOY_ON)
            {
                move = -1;
                stick_armed = false;
            }
            else if (jx > -JOY_OFF && jx < JOY_OFF)
            {
                stick_armed = true;
            }

            if (move != 0)
            {
                selected = (selected + move + NUM_GAMES) % NUM_GAMES;
                sound_sfx_play(&SFX_MOVE);
            }

            if (button_just_pressed(BTN_CLICK))
                launch = true;

            draw_menu(selected, now);
            sleep_ms(FRAME_MS);
        }

        sound_sfx_play(&SFX_SELECT);

        // Runs until the player chooses "menu" inside the game.
        games[selected].run();
    }
}
