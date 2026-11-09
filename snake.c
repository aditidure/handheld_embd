#include "snake.h"

#include "pico/stdlib.h"
#include "display/display.h"
#include "input/buttons.h"
#include "input/joystick.h"
#include "sound/sound.h"
#include "game/score.h"
#include "ui/ui.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// ==========================================
// CONSTANTS
// ==========================================

#define CELL                 10
#define HUD_H                30
#define COLS                 (SCREEN_WIDTH / CELL)
#define ROWS                 ((SCREEN_HEIGHT - HUD_H) / CELL)
#define MAX_LEN              (COLS * ROWS)

#define START_INTERVAL_MS    150   // time between moves at the start
#define MIN_INTERVAL_MS      70    // fastest speed
#define SPEEDUP_MS           3     // faster by this much per apple
#define READY_MS             1200
#define RESTART_DELAY_MS     800
#define FRAME_MS             16

#define POINTS_PER_APPLE     10

#define BUTTON_A             2     // up
#define BUTTON_B             3     // right
#define BUTTON_C             4     // down
#define BUTTON_D             5     // left (and "menu" on game over)
#define BUTTON_CLICK         8

#define JOY_THRESHOLD        0.6f

#define COL_BOARD_A          RGB565(6, 20, 10)
#define COL_BOARD_B          RGB565(9, 27, 14)
#define COL_SNAKE_HEAD       RGB565(150, 255, 100)
#define COL_SNAKE_A          RGB565(50, 210, 80)
#define COL_SNAKE_B          RGB565(35, 175, 65)
#define COL_SNAKE_ACCENT     RGB565(60, 220, 90)
#define COL_APPLE            RGB565(235, 40, 50)
#define COL_LEAF             RGB565(60, 200, 60)

// ==========================================
// MUSIC AND SOUND EFFECTS (original)
// ==========================================

// Bouncy C-major loop.
static const MusicNote snake_notes[] = {
    {NOTE_C5, 1}, {NOTE_E5, 1}, {NOTE_G5, 1}, {NOTE_E5, 1},
    {NOTE_C5, 1}, {NOTE_E5, 1}, {NOTE_G5, 2},
    {NOTE_F5, 1}, {NOTE_A5, 1}, {NOTE_C6, 1}, {NOTE_A5, 1},
    {NOTE_F5, 1}, {NOTE_A5, 1}, {NOTE_C6, 2},
    {NOTE_G5, 1}, {NOTE_B5, 1}, {NOTE_D6, 1}, {NOTE_B5, 1},
    {NOTE_G5, 1}, {NOTE_F5, 1}, {NOTE_E5, 1}, {NOTE_D5, 1},
    {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_G4, 2}, {REST, 2},

    {NOTE_A4, 1}, {NOTE_C5, 1}, {NOTE_E5, 1}, {NOTE_C5, 1},
    {NOTE_A4, 1}, {NOTE_C5, 1}, {NOTE_E5, 2},
    {NOTE_F4, 1}, {NOTE_A4, 1}, {NOTE_C5, 1}, {NOTE_A4, 1},
    {NOTE_F4, 1}, {NOTE_A4, 1}, {NOTE_C5, 2},
    {NOTE_G4, 1}, {NOTE_B4, 1}, {NOTE_D5, 1}, {NOTE_F5, 1},
    {NOTE_E5, 1}, {NOTE_D5, 1}, {NOTE_C5, 1}, {NOTE_B4, 1},
    {NOTE_C5, 4}, {REST, 4},
};

static const MusicNote snake_over_notes[] = {
    {REST, 3},
    {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_A4, 2}, {NOTE_G4, 6},
};

static const Song SONG_SNAKE = SONG(snake_notes, 140, true);
static const Song SONG_SNAKE_OVER = SONG(snake_over_notes, 150, false);

static const SfxSegment eat_segs[] = {
    {NOTE_C6, NOTE_C6, 40, false},
    {NOTE_G6, NOTE_G6, 60, false},
};

static const SfxSegment crash_segs[] = {
    {800, 200, 250, true},
};

static const SoundEffect SFX_EAT = SOUND_EFFECT(eat_segs, 2);
static const SoundEffect SFX_CRASH = SOUND_EFFECT(crash_segs, 4);

// ==========================================
// GAME STATE
// ==========================================

typedef struct
{
    int8_t x;
    int8_t y;
} Cell;

typedef enum
{
    SNAKE_READY,
    SNAKE_PLAYING,
    SNAKE_OVER
} SnakeState;

static Cell body[MAX_LEN];
static int length;

// Requested direction, and the direction of the last actual move.
// Turning is checked against the last move so two quick presses
// can't reverse the snake into itself.
static int dir_x, dir_y;
static int moved_x, moved_y;

static Cell food;
static Score score;
static bool score_ready = false;
static bool new_best = false;

static uint32_t move_interval;
static uint32_t state_time;
static SnakeState state;

// ==========================================
// GAME LOGIC
// ==========================================

static bool on_snake(int x, int y)
{
    for (int i = 0; i < length; i++)
    {
        if (body[i].x == x && body[i].y == y)
            return true;
    }
    return false;
}

static bool spawn_food(void)
{
    // Try random cells first, then scan for any free cell.
    for (int attempt = 0; attempt < 100; attempt++)
    {
        int x = rand() % COLS;
        int y = rand() % ROWS;

        if (!on_snake(x, y))
        {
            food.x = (int8_t)x;
            food.y = (int8_t)y;
            return true;
        }
    }

    for (int y = 0; y < ROWS; y++)
    {
        for (int x = 0; x < COLS; x++)
        {
            if (!on_snake(x, y))
            {
                food.x = (int8_t)x;
                food.y = (int8_t)y;
                return true;
            }
        }
    }

    return false;   // board is full
}

static void reset_game(uint32_t now)
{
    length = 3;

    for (int i = 0; i < length; i++)
    {
        body[i].x = (int8_t)(COLS / 2 - i);
        body[i].y = (int8_t)(ROWS / 2);
    }

    dir_x = moved_x = 1;
    dir_y = moved_y = 0;

    score_reset(&score);
    new_best = false;

    move_interval = START_INTERVAL_MS;
    spawn_food();

    state = SNAKE_READY;
    state_time = now;
}

static void set_direction(int dx, int dy)
{
    if (dx == -moved_x && dy == -moved_y)
        return;

    dir_x = dx;
    dir_y = dy;
}

static void read_direction_input(void)
{
    if (button_pressed(BUTTON_A))
        set_direction(0, -1);
    else if (button_pressed(BUTTON_B))
        set_direction(1, 0);
    else if (button_pressed(BUTTON_C))
        set_direction(0, 1);
    else if (button_pressed(BUTTON_D))
        set_direction(-1, 0);

    // Joystick also steers. Y is inverted to match the space shooter.
    float jx = joystick_read_x();
    float jy = -joystick_read_y();
    float ax = (jx < 0.0f) ? -jx : jx;
    float ay = (jy < 0.0f) ? -jy : jy;

    if (ax > JOY_THRESHOLD && ax >= ay)
        set_direction((jx > 0.0f) ? 1 : -1, 0);
    else if (ay > JOY_THRESHOLD)
        set_direction(0, (jy > 0.0f) ? 1 : -1);
}

static void game_over(uint32_t now)
{
    new_best = score_get_current(&score) > score_get_best(&score);
    score_update_best(&score);

    state = SNAKE_OVER;
    state_time = now;

    sound_sfx_play(&SFX_CRASH);
    sound_music_play(&SONG_SNAKE_OVER);
}

static void step(uint32_t now)
{
    int nx = body[0].x + dir_x;
    int ny = body[0].y + dir_y;

    // Wall collision.
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS)
    {
        game_over(now);
        return;
    }

    bool eating = (nx == food.x && ny == food.y);

    // Self collision. The tail moves away this step unless eating.
    int limit = length - (eating ? 0 : 1);
    for (int i = 0; i < limit; i++)
    {
        if (body[i].x == nx && body[i].y == ny)
        {
            game_over(now);
            return;
        }
    }

    if (eating && length < MAX_LEN)
        length++;

    for (int i = length - 1; i > 0; i--)
        body[i] = body[i - 1];

    body[0].x = (int8_t)nx;
    body[0].y = (int8_t)ny;

    moved_x = dir_x;
    moved_y = dir_y;

    if (eating)
    {
        score_add(&score, POINTS_PER_APPLE);
        sound_sfx_play(&SFX_EAT);

        if (move_interval > MIN_INTERVAL_MS + SPEEDUP_MS)
            move_interval -= SPEEDUP_MS;
        else
            move_interval = MIN_INTERVAL_MS;

        if (!spawn_food())
            game_over(now);   // board filled: you win, game ends
    }
}

// ==========================================
// DRAWING
// ==========================================

static int cell_px(int x) { return x * CELL; }
static int cell_py(int y) { return HUD_H + y * CELL; }

static void draw_board(void)
{
    for (int y = 0; y < ROWS; y++)
    {
        for (int x = 0; x < COLS; x++)
        {
            uint16_t color = ((x + y) & 1) ? COL_BOARD_B : COL_BOARD_A;
            display_fill_rect(cell_px(x), cell_py(y), CELL, CELL, color);
        }
    }
}

static void draw_food(uint32_t now)
{
    int cx = cell_px(food.x) + CELL / 2;
    int cy = cell_py(food.y) + CELL / 2;
    int r = ((now / 250) % 2) ? 4 : 3;

    ui_fill_circle(cx, cy + 1, r, COL_APPLE);
    display_fill_rect(cx, cy - 4, 3, 2, COL_LEAF);
    display_draw_pixel(cx - 2, cy - 1, WHITE);
}

static void draw_snake(void)
{
    // Draw tail first so the head ends up on top.
    for (int i = length - 1; i >= 0; i--)
    {
        int px = cell_px(body[i].x);
        int py = cell_py(body[i].y);

        uint16_t color;
        if (i == 0)
            color = (state == SNAKE_OVER) ? COL_PINK : COL_SNAKE_HEAD;
        else
            color = (i % 2) ? COL_SNAKE_A : COL_SNAKE_B;

        display_fill_rect(px + 1, py + 1, CELL - 2, CELL - 2, color);

        // Fill the gap towards the next segment so the body looks
        // continuous.
        if (i > 0)
        {
            int qx = cell_px(body[i - 1].x);
            int qy = cell_py(body[i - 1].y);

            if (qx == px)
            {
                int y = (qy > py) ? qy : py;
                display_fill_rect(px + 1, y - 1, CELL - 2, 2, color);
            }
            else if (qy == py)
            {
                int x = (qx > px) ? qx : px;
                display_fill_rect(x - 1, py + 1, 2, CELL - 2, color);
            }
        }
    }

    // Eyes, looking in the direction of travel.
    int cx = cell_px(body[0].x) + CELL / 2;
    int cy = cell_py(body[0].y) + CELL / 2;
    int fx = moved_x * 2;
    int fy = moved_y * 2;
    int sx = -moved_y * 2;
    int sy = moved_x * 2;

    display_fill_rect(cx + fx + sx - 1, cy + fy + sy - 1, 2, 2, BLACK);
    display_fill_rect(cx + fx - sx - 1, cy + fy - sy - 1, 2, 2, BLACK);
}

static void draw_hud(void)
{
    display_fill_rect(0, 0, SCREEN_WIDTH, HUD_H, COL_HUD_BG);
    display_draw_line(0, HUD_H - 1, SCREEN_WIDTH - 1, HUD_H - 1,
                      COL_SNAKE_ACCENT);

    display_draw_string(6, 3, "SCORE", COL_LABEL, 1);
    display_draw_number(6, 13, score_get_current(&score), WHITE, 2);

    int best_x = SCREEN_WIDTH / 2 - 20;
    display_draw_string(best_x, 3, "BEST", COL_LABEL, 1);
    display_draw_number(best_x, 13, score_get_best(&score), YELLOW, 2);

    int len_x = SCREEN_WIDTH - 50;
    display_draw_string(len_x, 3, "LENGTH", COL_LABEL, 1);
    display_draw_number(len_x, 13, length, COL_SNAKE_ACCENT, 2);
}

static void draw_ready(uint32_t now)
{
    uint16_t color = ((now / 200) % 2) ? YELLOW : WHITE;
    ui_text_centered(SCREEN_HEIGHT / 2 - 20, "GET READY", color, 3);
    ui_text_centered(SCREEN_HEIGHT / 2 + 14,
                     "STEER WITH JOYSTICK OR A B C D", COL_LABEL, 1);
}

static void draw_game_over_panel(uint32_t now)
{
    const int panel_w = 208;
    const int panel_h = 170;
    const int panel_x = (SCREEN_WIDTH - panel_w) / 2;
    const int panel_y = 20;

    ui_draw_panel(panel_x, panel_y, panel_w, panel_h);

    ui_text_centered(panel_y + 16, "GAME OVER", RED, 3);
    display_draw_line(panel_x + 20, panel_y + 46,
                      panel_x + panel_w - 21, panel_y + 46,
                      COL_ACCENT_DIM);

    ui_text_centered(panel_y + 58, "SCORE", COL_LABEL, 1);
    ui_number_centered(panel_y + 70, score_get_current(&score), YELLOW, 3);

    ui_text_centered(panel_y + 106, "BEST", COL_LABEL, 1);
    ui_number_centered(panel_y + 118, score_get_best(&score), GREEN, 2);

    if (new_best && ((now / 250) % 2 == 0))
        ui_text_centered(panel_y + 144, "NEW BEST", YELLOW, 2);

    if (now - state_time > RESTART_DELAY_MS)
    {
        if ((now / 500) % 2 == 0)
            ui_text_centered(SCREEN_HEIGHT - 32, "CLICK TO PLAY AGAIN",
                             WHITE, 2);

        display_draw_string(SCREEN_WIDTH - 48, SCREEN_HEIGHT - 10,
                            "D: MENU", COL_LABEL, 1);
    }
}

static void draw(uint32_t now)
{
    display_clear(BLACK);

    draw_board();
    draw_food(now);
    draw_snake();
    draw_hud();

    if (state == SNAKE_READY)
        draw_ready(now);
    else if (state == SNAKE_OVER)
        draw_game_over_panel(now);

    display_show();
}

// ==========================================
// MAIN LOOP
// ==========================================

int snake_game_best_score(void)
{
    return score_ready ? score_get_best(&score) : 0;
}

void snake_game_run(void)
{
    if (!score_ready)
    {
        score_init(&score);
        score_ready = true;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t last_move = now;

    reset_game(now);
    sound_music_play(&SONG_SNAKE);

    bool click_was_pressed = button_pressed(BUTTON_CLICK);
    bool menu_was_pressed = button_pressed(BUTTON_D);

    while (true)
    {
        now = to_ms_since_boot(get_absolute_time());

        bool click = button_pressed(BUTTON_CLICK);
        bool click_edge = click && !click_was_pressed;
        click_was_pressed = click;

        bool menu = button_pressed(BUTTON_D);
        bool menu_edge = menu && !menu_was_pressed;
        menu_was_pressed = menu;

        if (state == SNAKE_READY)
        {
            read_direction_input();

            if (now - state_time >= READY_MS)
            {
                state = SNAKE_PLAYING;
                last_move = now;
            }
        }
        else if (state == SNAKE_PLAYING)
        {
            read_direction_input();

            if (now - last_move >= move_interval)
            {
                last_move = now;
                step(now);
            }
        }
        else
        {
            bool ready = now - state_time > RESTART_DELAY_MS;

            // After the game-over jingle, bring the main tune back.
            if (!sound_music_is_playing())
                sound_music_play(&SONG_SNAKE);

            if (menu_edge && ready)
            {
                sound_stop();
                return;
            }

            if (click_edge && ready)
            {
                reset_game(now);
                sound_music_play(&SONG_SNAKE);
            }
        }

        draw(now);
        sleep_ms(FRAME_MS);
    }
}
