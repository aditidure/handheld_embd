#include "space_shooter.h"

#include "pico/stdlib.h"
#include "display/display.h"
#include "input/joystick.h"
#include "input/buttons.h"
#include "sound/sound.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// ==========================================
// GAME CONSTANTS
// ==========================================

#define PLAYER_ACCEL             550.0f
#define PLAYER_MAX_SPEED         180.0f
#define FRICTION                 2.5f

#define BULLET_SPEED             300.0f
#define SHOT_INTERVAL            180

#define INITIAL_SPAWN_INTERVAL   900
#define MIN_SPAWN_INTERVAL       300

#define FRAME_TIME_MS            16
#define COLLISION_COOLDOWN       900
#define FOOD_SPAWN_INTERVAL      5000

#define BUTTON_CLICK             8

// ==========================================
// UI CONSTANTS
// ==========================================

#define HUD_HEIGHT               30
#define POINTS_PER_LEVEL         100
#define LEVEL_BANNER_MS          1500
#define HIT_FLASH_MS             250
#define RESTART_DELAY_MS         800

#define NUM_STARS                48
#define MAX_PARTICLES            64
#define MAX_POPUPS               6

#define PI_F                     3.14159265f

// Convert 8-bit RGB to RGB565.
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// UI palette.
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

#define COL_SHIP_HULL    RGB565(210, 220, 235)
#define COL_SHIP_WING    RGB565(40, 110, 220)

#define COL_ROCK_DARK    RGB565(90, 70, 55)
#define COL_ROCK_LIGHT   RGB565(170, 140, 110)
#define COL_ROCK_CRATER  RGB565(60, 45, 35)

#define COL_STAR_FAR     RGB565(70, 70, 90)
#define COL_STAR_MID     RGB565(140, 140, 170)
#define COL_STAR_NEAR    WHITE

// ==========================================
// GAME STATE
// ==========================================

static SpaceShooter game;

// ==========================================
// UI STATE (visual only, no gameplay effect)
// ==========================================

typedef enum
{
    STATE_TITLE,
    STATE_PLAYING,
    STATE_GAME_OVER
} ScreenState;

typedef struct
{
    float x;
    float y;
    float speed;
    uint16_t color;
} Star;

typedef struct
{
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float max_life;
    uint16_t color;
    bool active;
} Particle;

typedef struct
{
    float x;
    float y;
    float life;
    int value;
    bool active;
} Popup;

// ==========================================
// MUSIC (original compositions)
// ==========================================

// Title theme: A minor, calm and heroic. 8-bar loop.
static const MusicNote title_notes[] = {
    {NOTE_A4, 2}, {NOTE_C5, 2}, {NOTE_E5, 2}, {NOTE_A5, 2},
    {NOTE_G5, 3}, {NOTE_E5, 1}, {NOTE_D5, 2}, {NOTE_C5, 2},
    {NOTE_F5, 2}, {NOTE_A5, 2}, {NOTE_G5, 2}, {NOTE_F5, 2},
    {NOTE_E5, 6}, {REST, 2},

    {NOTE_A4, 2}, {NOTE_C5, 2}, {NOTE_E5, 2}, {NOTE_A5, 2},
    {NOTE_B5, 3}, {NOTE_A5, 1}, {NOTE_G5, 2}, {NOTE_E5, 2},
    {NOTE_F5, 2}, {NOTE_E5, 2}, {NOTE_D5, 2}, {NOTE_B4, 2},
    {NOTE_A4, 6}, {REST, 2},
};

// Gameplay theme: E minor, driving arpeggios then a lead melody.
static const MusicNote game_notes[] = {
    // Arpeggios: Em, C, D, B.
    {NOTE_E4, 1}, {NOTE_B4, 1}, {NOTE_E5, 1}, {NOTE_B4, 1},
    {NOTE_G5, 1}, {NOTE_B4, 1}, {NOTE_E5, 1}, {NOTE_B4, 1},
    {NOTE_C4, 1}, {NOTE_G4, 1}, {NOTE_C5, 1}, {NOTE_G4, 1},
    {NOTE_E5, 1}, {NOTE_G4, 1}, {NOTE_C5, 1}, {NOTE_G4, 1},
    {NOTE_D4, 1}, {NOTE_A4, 1}, {NOTE_D5, 1}, {NOTE_A4, 1},
    {NOTE_FS5, 1}, {NOTE_A4, 1}, {NOTE_D5, 1}, {NOTE_A4, 1},
    {NOTE_B4, 1}, {NOTE_FS5, 1}, {NOTE_B5, 1}, {NOTE_FS5, 1},
    {NOTE_DS6, 1}, {NOTE_B5, 1}, {NOTE_A5, 1}, {NOTE_FS5, 1},

    // Melody, first phrase.
    {NOTE_E5, 2}, {NOTE_G5, 2}, {NOTE_A5, 1}, {NOTE_B5, 1}, {NOTE_A5, 2},
    {NOTE_G5, 2}, {NOTE_FS5, 2}, {NOTE_E5, 2}, {NOTE_D5, 2},
    {NOTE_E5, 2}, {NOTE_G5, 2}, {NOTE_A5, 1}, {NOTE_B5, 1}, {NOTE_D6, 2},
    {NOTE_B5, 4}, {REST, 2}, {NOTE_B4, 1}, {NOTE_DS5, 1},

    // Arpeggios again.
    {NOTE_E4, 1}, {NOTE_B4, 1}, {NOTE_E5, 1}, {NOTE_B4, 1},
    {NOTE_G5, 1}, {NOTE_B4, 1}, {NOTE_E5, 1}, {NOTE_B4, 1},
    {NOTE_C4, 1}, {NOTE_G4, 1}, {NOTE_C5, 1}, {NOTE_G4, 1},
    {NOTE_E5, 1}, {NOTE_G4, 1}, {NOTE_C5, 1}, {NOTE_G4, 1},
    {NOTE_D4, 1}, {NOTE_A4, 1}, {NOTE_D5, 1}, {NOTE_A4, 1},
    {NOTE_FS5, 1}, {NOTE_A4, 1}, {NOTE_D5, 1}, {NOTE_A4, 1},
    {NOTE_B4, 1}, {NOTE_FS5, 1}, {NOTE_B5, 1}, {NOTE_FS5, 1},
    {NOTE_DS6, 1}, {NOTE_B5, 1}, {NOTE_A5, 1}, {NOTE_FS5, 1},

    // Melody, answering phrase (climbs higher, resolves to E).
    {NOTE_E5, 2}, {NOTE_G5, 2}, {NOTE_A5, 1}, {NOTE_B5, 1}, {NOTE_A5, 2},
    {NOTE_G5, 2}, {NOTE_A5, 2}, {NOTE_B5, 2}, {NOTE_D6, 2},
    {NOTE_E6, 3}, {NOTE_D6, 1}, {NOTE_B5, 2}, {NOTE_G5, 2},
    {NOTE_E5, 4}, {REST, 2}, {NOTE_DS5, 1}, {NOTE_FS5, 1},
};

// Game over: short descending "sad" jingle, plays once.
// Starts with a rest so the crash sound effect is heard first.
static const MusicNote game_over_notes[] = {
    {REST, 3},
    {NOTE_G5, 2}, {NOTE_DS5, 2}, {NOTE_C5, 2},
    {NOTE_B4, 1}, {NOTE_AS4, 1}, {NOTE_A4, 6},
};

static const Song SONG_TITLE = SONG(title_notes, 170, true);
static const Song SONG_GAME = SONG(game_notes, 130, true);
static const Song SONG_GAME_OVER = SONG(game_over_notes, 150, false);

// ==========================================
// SOUND EFFECTS
// ==========================================

static const SfxSegment shoot_segs[] = {
    {1800, 700, 45, false},             // "pew" slide down
};

static const SfxSegment explode_segs[] = {
    {900, 250, 160, true},              // noisy crunch
};

static const SfxSegment hit_segs[] = {
    {700, 250, 120, true},
    {300, 150, 160, true},              // heavy crash
};

static const SfxSegment pickup_segs[] = {
    {NOTE_A5, NOTE_A5, 45, false},
    {NOTE_E6, NOTE_E6, 45, false},
    {NOTE_A6, NOTE_A6, 70, false},      // rising chime
};

static const SfxSegment shield_on_segs[] = {
    {500, 1800, 160, false},            // power-up sweep
};

static const SfxSegment shield_block_segs[] = {
    {2000, 2000, 25, false},
    {1400, 1400, 50, false},            // metallic "ting"
};

static const SfxSegment level_up_segs[] = {
    {NOTE_C6, NOTE_C6, 70, false},
    {NOTE_E6, NOTE_E6, 70, false},
    {NOTE_G6, NOTE_G6, 70, false},
    {2093, 2093, 150, false},           // C7
};

static const SfxSegment start_segs[] = {
    {NOTE_G5, NOTE_G5, 80, false},
    {0, 0, 20, false},
    {NOTE_C6, NOTE_C6, 80, false},
    {NOTE_G6, NOTE_G6, 160, false},
};

static const SoundEffect SFX_SHOOT = SOUND_EFFECT(shoot_segs, 1);
static const SoundEffect SFX_EXPLODE = SOUND_EFFECT(explode_segs, 2);
static const SoundEffect SFX_SHIELD_BLOCK = SOUND_EFFECT(shield_block_segs, 2);
static const SoundEffect SFX_PICKUP = SOUND_EFFECT(pickup_segs, 3);
static const SoundEffect SFX_SHIELD_ON = SOUND_EFFECT(shield_on_segs, 3);
static const SoundEffect SFX_LEVEL_UP = SOUND_EFFECT(level_up_segs, 3);
static const SoundEffect SFX_HIT = SOUND_EFFECT(hit_segs, 4);
static const SoundEffect SFX_START = SOUND_EFFECT(start_segs, 5);

static ScreenState screen_state = STATE_TITLE;

static Star stars[NUM_STARS];
static bool stars_ready = false;

static Particle particles[MAX_PARTICLES];
static Popup popups[MAX_POPUPS];

static uint32_t hit_flash_until = 0;
static uint32_t level_banner_until = 0;
static uint32_t game_over_time = 0;
static int current_level = 1;
static bool new_best = false;

// ==========================================
// DRAWING HELPERS
// ==========================================

static int text_width(const char *text, int scale)
{
    int len = (int)strlen(text);
    return (len > 0) ? len * 6 * scale - scale : 0;
}

static void draw_text_centered(int y, const char *text,
                               uint16_t color, int scale)
{
    int x = (SCREEN_WIDTH - text_width(text, scale)) / 2;
    display_draw_string(x, y, text, color, scale);
}

static void draw_number_centered(int y, int number,
                                 uint16_t color, int scale)
{
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", number);
    draw_text_centered(y, buffer, color, scale);
}

static void fill_circle(int cx, int cy, int r, uint16_t color)
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

static void draw_circle(int cx, int cy, int r, uint16_t color)
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

static void fill_triangle(int x0, int y0, int x1, int y1,
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

// 7x6 heart icon, one byte per row (bit 6 = leftmost pixel).
static const uint8_t heart_bitmap[6] = {
    0x36, // .XX.XX.
    0x7F, // XXXXXXX
    0x7F, // XXXXXXX
    0x3E, // .XXXXX.
    0x1C, // ..XXX..
    0x08  // ...X...
};

static void draw_heart(int x, int y, int scale, uint16_t color)
{
    for (int row = 0; row < 6; row++)
    {
        for (int col = 0; col < 7; col++)
        {
            if (heart_bitmap[row] & (0x40 >> col))
                display_fill_rect(x + col * scale, y + row * scale,
                                  scale, scale, color);
        }
    }
}

// ==========================================
// STARFIELD, PARTICLES AND POPUPS
// ==========================================

static void init_stars(void)
{
    for (int i = 0; i < NUM_STARS; i++)
    {
        int layer = i % 3;

        stars[i].x = (float)(rand() % SCREEN_WIDTH);
        stars[i].y = (float)(rand() % SCREEN_HEIGHT);

        if (layer == 0)
        {
            stars[i].speed = 12.0f;
            stars[i].color = COL_STAR_FAR;
        }
        else if (layer == 1)
        {
            stars[i].speed = 30.0f;
            stars[i].color = COL_STAR_MID;
        }
        else
        {
            stars[i].speed = 65.0f;
            stars[i].color = COL_STAR_NEAR;
        }
    }

    stars_ready = true;
}

static void update_stars(float dt)
{
    for (int i = 0; i < NUM_STARS; i++)
    {
        stars[i].y += stars[i].speed * dt;

        if (stars[i].y >= SCREEN_HEIGHT)
        {
            stars[i].y -= SCREEN_HEIGHT;
            stars[i].x = (float)(rand() % SCREEN_WIDTH);
        }
    }
}

static void draw_stars(void)
{
    for (int i = 0; i < NUM_STARS; i++)
    {
        int x = (int)stars[i].x;
        int y = (int)stars[i].y;

        display_draw_pixel(x, y, stars[i].color);

        // Near stars get a short motion streak.
        if (stars[i].speed > 50.0f)
            display_draw_pixel(x, y - 1, COL_STAR_MID);
    }
}

static void clear_effects(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].active = false;

    for (int i = 0; i < MAX_POPUPS; i++)
        popups[i].active = false;
}

static void spawn_explosion(float x, float y, int count,
                            uint16_t color, float speed)
{
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++)
    {
        if (particles[i].active)
            continue;

        float angle = (float)(rand() % 360) * (PI_F / 180.0f);
        float v = speed * (0.3f + (float)(rand() % 70) / 100.0f);

        particles[i].x = x;
        particles[i].y = y;
        particles[i].vx = cosf(angle) * v;
        particles[i].vy = sinf(angle) * v;
        particles[i].max_life = 0.35f + (float)(rand() % 35) / 100.0f;
        particles[i].life = particles[i].max_life;
        particles[i].color = color;
        particles[i].active = true;

        count--;
    }
}

static void update_particles(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active)
            continue;

        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].vx *= 0.96f;
        particles[i].vy *= 0.96f;
        particles[i].life -= dt;

        if (particles[i].life <= 0.0f)
            particles[i].active = false;
    }
}

static void draw_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active)
            continue;

        int x = (int)particles[i].x;
        int y = (int)particles[i].y;
        float t = particles[i].life / particles[i].max_life;

        // Hot white core -> base colour -> fading ember.
        if (t > 0.75f)
            display_fill_rect(x - 1, y - 1, 3, 3, YELLOW);
        else if (t > 0.35f)
            display_fill_rect(x, y, 2, 2, particles[i].color);
        else
            display_draw_pixel(x, y, COL_DARK_RED);
    }
}

static void add_popup(float x, float y, int value)
{
    for (int i = 0; i < MAX_POPUPS; i++)
    {
        if (!popups[i].active)
        {
            popups[i].x = x;
            popups[i].y = y;
            popups[i].life = 0.6f;
            popups[i].value = value;
            popups[i].active = true;
            return;
        }
    }
}

static void update_popups(float dt)
{
    for (int i = 0; i < MAX_POPUPS; i++)
    {
        if (!popups[i].active)
            continue;

        popups[i].y -= 35.0f * dt;
        popups[i].life -= dt;

        if (popups[i].life <= 0.0f)
            popups[i].active = false;
    }
}

static void draw_popups(void)
{
    for (int i = 0; i < MAX_POPUPS; i++)
    {
        if (!popups[i].active)
            continue;

        int x = (int)popups[i].x - 8;
        int y = (int)popups[i].y;

        // The font has no '+', so draw it by hand.
        display_draw_line(x, y + 3, x + 4, y + 3, YELLOW);
        display_draw_line(x + 2, y + 1, x + 2, y + 5, YELLOW);
        display_draw_number(x + 6, y, popups[i].value, YELLOW, 1);
    }
}

// ==========================================
// INITIALIZATION
// ==========================================

void space_shooter_init(void)
{
    game.player.x = SCREEN_WIDTH / 2;
    game.player.y = SCREEN_HEIGHT - 45;
    game.player.vx = 0.0f;
    game.player.vy = 0.0f;

    game.lives = INITIAL_LIVES;
    game.score = 0;
    game.game_over = 0;

    // Shield charges are earned by collecting food.
    game.shield_charges = 0;
    game.shield_active = false;
    game.shield_end_time = 0;
    game.shield_button_was_pressed = false;

    // Food state.
    game.food_x = 0.0f;
    game.food_y = 0.0f;
    game.food_active = false;
    game.last_food_spawn_time = 0;

    for (int i = 0; i < 4; i++)
        game.last_shot_time[i] = 0;

    game.last_spawn_time = 0;
    game.last_collision_time = 0;

    for (int i = 0; i < MAX_BULLETS; i++)
        game.bullets[i].active = false;

    for (int i = 0; i < MAX_ASTEROIDS; i++)
        game.asteroids[i].active = false;

    clear_effects();

    hit_flash_until = 0;
    level_banner_until = 0;
    current_level = 1;
    new_best = false;
}

// ==========================================
// PLAYER PHYSICS
// ==========================================

static void update_player(float dt)
{
    // Both axes are inverted to correct the opposite joystick movement.
    // If testing shows only one axis is reversed, remove the '-' from
    // the correctly behaving axis.
    float joy_x = joystick_read_x();
    float joy_y = -joystick_read_y();

    game.player.vx += joy_x * PLAYER_ACCEL * dt;
    game.player.vy += joy_y * PLAYER_ACCEL * dt;

    float friction_factor = 1.0f - FRICTION * dt;
    if (friction_factor < 0.0f)
        friction_factor = 0.0f;

    game.player.vx *= friction_factor;
    game.player.vy *= friction_factor;

    if (game.player.vx > PLAYER_MAX_SPEED)
        game.player.vx = PLAYER_MAX_SPEED;
    if (game.player.vx < -PLAYER_MAX_SPEED)
        game.player.vx = -PLAYER_MAX_SPEED;

    if (game.player.vy > PLAYER_MAX_SPEED)
        game.player.vy = PLAYER_MAX_SPEED;
    if (game.player.vy < -PLAYER_MAX_SPEED)
        game.player.vy = -PLAYER_MAX_SPEED;

    game.player.x += game.player.vx * dt;
    game.player.y += game.player.vy * dt;

    if (game.player.x < PLAYER_SIZE)
    {
        game.player.x = PLAYER_SIZE;
        game.player.vx = 0.0f;
    }

    if (game.player.x > SCREEN_WIDTH - PLAYER_SIZE)
    {
        game.player.x = SCREEN_WIDTH - PLAYER_SIZE;
        game.player.vx = 0.0f;
    }

    if (game.player.y < 45)
    {
        game.player.y = 45;
        game.player.vy = 0.0f;
    }

    if (game.player.y > SCREEN_HEIGHT - PLAYER_SIZE)
    {
        game.player.y = SCREEN_HEIGHT - PLAYER_SIZE;
        game.player.vy = 0.0f;
    }
}

// ==========================================
// SHOOTING
// ==========================================

// direction: 0 = up, 1 = right, 2 = down, 3 = left
static void fire_bullet(int direction, uint32_t now)
{
    if (direction < 0 || direction > 3)
        return;

    if (now - game.last_shot_time[direction] < SHOT_INTERVAL)
        return;

    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
        {
            game.bullets[i].x = game.player.x;
            game.bullets[i].y = game.player.y;
            game.bullets[i].vx = 0.0f;
            game.bullets[i].vy = 0.0f;

            if (direction == 0)
                game.bullets[i].vy = -BULLET_SPEED;
            else if (direction == 1)
                game.bullets[i].vx = BULLET_SPEED;
            else if (direction == 2)
                game.bullets[i].vy = BULLET_SPEED;
            else
                game.bullets[i].vx = -BULLET_SPEED;

            game.bullets[i].active = true;
            game.last_shot_time[direction] = now;

            sound_sfx_play(&SFX_SHOOT);
            break;
        }
    }
}

static void handle_shooting(uint32_t now)
{
    // A = up, B = right, C = down, D = left.
    if (button_pressed(2))
        fire_bullet(0, now);

    if (button_pressed(3))
        fire_bullet(1, now);

    if (button_pressed(4))
        fire_bullet(2, now);

    if (button_pressed(5))
        fire_bullet(3, now);
}

// ==========================================
// FOOD AND SHIELD
// ==========================================

static void spawn_food(uint32_t now)
{
    if (game.food_active)
        return;

    if (now - game.last_food_spawn_time < FOOD_SPAWN_INTERVAL)
        return;

    // Keep food away from the HUD and screen edges.
    game.food_x = 15.0f + (float)(rand() % (SCREEN_WIDTH - 30));
    game.food_y = 45.0f + (float)(rand() % (SCREEN_HEIGHT - 90));

    game.food_active = true;
    game.last_food_spawn_time = now;
}

static void check_food_collection(void)
{
    if (!game.food_active)
        return;

    // Do not consume food while the shield inventory is already full.
    if (game.shield_charges >= MAX_SHIELD_CHARGES)
        return;

    float dx = game.player.x - game.food_x;
    float dy = game.player.y - game.food_y;

    if ((dx * dx + dy * dy) < (18.0f * 18.0f))
    {
        game.shield_charges++;
        game.food_active = false;
        spawn_explosion(game.food_x, game.food_y, 12, COL_CYAN, 70.0f);
        sound_sfx_play(&SFX_PICKUP);
    }
}

static void update_shield(uint32_t now)
{
    bool pressed = button_pressed(BUTTON_CLICK);

    // Activate only on a fresh press, not continuously while held.
    if (pressed && !game.shield_button_was_pressed)
    {
        if (game.shield_charges > 0 && !game.shield_active)
        {
            game.shield_charges--;
            game.shield_active = true;
            game.shield_end_time = now + SHIELD_DURATION_MS;
            sound_sfx_play(&SFX_SHIELD_ON);
        }
    }

    game.shield_button_was_pressed = pressed;

    if (game.shield_active &&
        (int32_t)(now - game.shield_end_time) >= 0)
    {
        game.shield_active = false;
    }
}

// ==========================================
// ASTEROID GENERATION AND UPDATE
// ==========================================

static void spawn_asteroid(uint32_t now)
{
    int score_for_difficulty = (game.score > 300U) ? 300 : (int)game.score;
    int spawn_interval = INITIAL_SPAWN_INTERVAL - score_for_difficulty * 2;

    if (spawn_interval < MIN_SPAWN_INTERVAL)
        spawn_interval = MIN_SPAWN_INTERVAL;

    if (now - game.last_spawn_time < (uint32_t)spawn_interval)
        return;

    for (int i = 0; i < MAX_ASTEROIDS; i++)
    {
        if (!game.asteroids[i].active)
        {
            Asteroid *a = &game.asteroids[i];

            a->x = (float)(15 + rand() % (SCREEN_WIDTH - 30));
            a->y = -15.0f;
            a->radius = 7 + rand() % 7;
            a->speed = 65.0f + (float)(rand() % 65);
            a->active = true;

            // Random jagged outline and spin (visual only).
            a->angle = (float)(rand() % 628) / 100.0f;
            a->spin = (float)(rand() % 400 - 200) / 100.0f;
            for (int k = 0; k < 8; k++)
                a->shape[k] = (int8_t)(rand() % (a->radius / 2 + 1) -
                                       a->radius / 4);

            game.last_spawn_time = now;
            break;
        }
    }
}

static void update_asteroids(float dt)
{
    for (int i = 0; i < MAX_ASTEROIDS; i++)
    {
        if (!game.asteroids[i].active)
            continue;

        game.asteroids[i].y += game.asteroids[i].speed * dt;
        game.asteroids[i].angle += game.asteroids[i].spin * dt;

        if (game.asteroids[i].y > SCREEN_HEIGHT + 15)
            game.asteroids[i].active = false;
    }
}

// ==========================================
// BULLET UPDATE
// ==========================================

static void update_bullets(float dt)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
            continue;

        game.bullets[i].x += game.bullets[i].vx * dt;
        game.bullets[i].y += game.bullets[i].vy * dt;

        if (game.bullets[i].x < 0 ||
            game.bullets[i].x > SCREEN_WIDTH ||
            game.bullets[i].y < 0 ||
            game.bullets[i].y > SCREEN_HEIGHT)
        {
            game.bullets[i].active = false;
        }
    }
}

// ==========================================
// COLLISION DETECTION
// ==========================================

static void check_collisions(uint32_t now)
{
    // Bullet versus asteroid.
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
            continue;

        for (int j = 0; j < MAX_ASTEROIDS; j++)
        {
            if (!game.asteroids[j].active)
                continue;

            float dx = game.bullets[i].x - game.asteroids[j].x;
            float dy = game.bullets[i].y - game.asteroids[j].y;
            float radius = (float)game.asteroids[j].radius + 3.0f;

            if ((dx * dx + dy * dy) < (radius * radius))
            {
                game.bullets[i].active = false;
                game.asteroids[j].active = false;
                game.score += 10U;

                spawn_explosion(game.asteroids[j].x, game.asteroids[j].y,
                                6 + game.asteroids[j].radius,
                                COL_ORANGE, 90.0f);
                add_popup(game.asteroids[j].x, game.asteroids[j].y, 10);
                sound_sfx_play(&SFX_EXPLODE);
                break;
            }
        }
    }

    // Player versus asteroid.
    // Shield collisions are checked even during the life-loss cooldown.
    for (int i = 0; i < MAX_ASTEROIDS; i++)
    {
        if (!game.asteroids[i].active)
            continue;

        float dx = game.player.x - game.asteroids[i].x;
        float dy = game.player.y - game.asteroids[i].y;
        float radius = (float)game.asteroids[i].radius +
                       (float)PLAYER_SIZE / 2.0f;

        if ((dx * dx + dy * dy) < (radius * radius))
        {
            if (game.shield_active)
            {
                game.asteroids[i].active = false;
                spawn_explosion(game.asteroids[i].x, game.asteroids[i].y,
                                10, COL_CYAN, 90.0f);
                sound_sfx_play(&SFX_SHIELD_BLOCK);
                break;
            }

            // During cooldown, leave the asteroid active and do not
            // subtract another life.
            if (now - game.last_collision_time < COLLISION_COOLDOWN)
                continue;

            game.asteroids[i].active = false;
            game.lives--;
            game.last_collision_time = now;

            hit_flash_until = now + HIT_FLASH_MS;
            spawn_explosion(game.asteroids[i].x, game.asteroids[i].y,
                            16, RED, 110.0f);

            sound_sfx_play(&SFX_HIT);

            if (game.lives <= 0)
                game.game_over = 1;

            break;
        }
    }
}

// ==========================================
// DRAW FUNCTIONS: GAME OBJECTS
// ==========================================

static void draw_ship(int x, int y, uint32_t now, bool thrust)
{
    // Engine flame (flickers).
    if (thrust)
    {
        int flame = 5 + (int)((now / 40) % 4);
        fill_triangle(x - 3, y + 9, x + 3, y + 9, x, y + 9 + flame,
                      COL_ORANGE);
        fill_triangle(x - 1, y + 9, x + 1, y + 9, x, y + 6 + flame,
                      YELLOW);
    }

    // Wings.
    fill_triangle(x - 3, y - 2, x - PLAYER_SIZE, y + 10, x - 3, y + 8,
                  COL_SHIP_WING);
    fill_triangle(x + 3, y - 2, x + PLAYER_SIZE, y + 10, x + 3, y + 8,
                  COL_SHIP_WING);

    // Hull.
    fill_triangle(x, y - PLAYER_SIZE, x - 5, y + 9, x + 5, y + 9,
                  COL_SHIP_HULL);

    // Wing-tip lights.
    display_fill_rect(x - PLAYER_SIZE, y + 9, 2, 2, COL_PINK);
    display_fill_rect(x + PLAYER_SIZE - 1, y + 9, 2, 2, COL_PINK);

    // Cockpit.
    fill_circle(x, y - 1, 2, COL_ACCENT);
    display_draw_pixel(x - 1, y - 2, WHITE);
}

static void draw_player(uint32_t now)
{
    // Blink while invulnerable after taking a hit.
    bool recovering = game.lives < INITIAL_LIVES &&
                      (now - game.last_collision_time) < COLLISION_COOLDOWN;

    if (recovering && ((now / 80) % 2) == 0)
        return;

    draw_ship((int)game.player.x, (int)game.player.y, now, true);
}

static void draw_asteroids(void)
{
    for (int i = 0; i < MAX_ASTEROIDS; i++)
    {
        const Asteroid *a = &game.asteroids[i];

        if (!a->active)
            continue;

        int x = (int)a->x;
        int y = (int)a->y;
        int r = a->radius;

        float c = cosf(a->angle);
        float s = sinf(a->angle);

        // Rocky body.
        fill_circle(x, y, r - 1, COL_ROCK_DARK);

        // Jagged rotating outline.
        int px[8];
        int py[8];

        for (int k = 0; k < 8; k++)
        {
            float base = (float)k * (PI_F / 4.0f);
            float vr = (float)(r + a->shape[k]);
            float ux = cosf(base);
            float uy = sinf(base);

            px[k] = x + (int)((ux * c - uy * s) * vr);
            py[k] = y + (int)((ux * s + uy * c) * vr);
        }

        for (int k = 0; k < 8; k++)
        {
            int n = (k + 1) % 8;
            display_draw_line(px[k], py[k], px[n], py[n], COL_ROCK_LIGHT);
        }

        // Craters rotate with the rock.
        float cr = (float)r * 0.4f;
        int cx1 = x + (int)(c * cr);
        int cy1 = y + (int)(s * cr);
        int cx2 = x - (int)(s * cr * 0.8f);
        int cy2 = y + (int)(c * cr * 0.8f);

        fill_circle(cx1, cy1, r / 4, COL_ROCK_CRATER);
        fill_circle(cx2, cy2, r / 5, COL_ROCK_CRATER);
    }
}

static void draw_bullets(void)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
            continue;

        int x = (int)game.bullets[i].x;
        int y = (int)game.bullets[i].y;

        // Laser bolt aligned with its direction of travel.
        if (game.bullets[i].vx != 0.0f)
        {
            display_fill_rect(x - 4, y - 1, 8, 3, COL_ORANGE);
            display_fill_rect(x - 3, y, 6, 1, YELLOW);
        }
        else
        {
            display_fill_rect(x - 1, y - 4, 3, 8, COL_ORANGE);
            display_fill_rect(x, y - 3, 1, 6, YELLOW);
        }
    }
}

static void draw_food(uint32_t now)
{
    if (!game.food_active)
        return;

    int x = (int)game.food_x;
    int y = (int)game.food_y;

    // Dim the orb when it can't be collected (shield inventory full).
    bool collectable = game.shield_charges < MAX_SHIELD_CHARGES;
    uint16_t ring = collectable ? COL_ACCENT : COL_DIM;
    uint16_t core = collectable ? COL_CYAN : COL_DIM;

    int pulse = (int)((now / 120) % 4);
    if (pulse == 3)
        pulse = 1;

    draw_circle(x, y, 7 + pulse, ring);
    fill_circle(x, y, 4, core);
    display_fill_rect(x - 1, y - 2, 2, 2, WHITE);
}

static void draw_shield(uint32_t now)
{
    if (!game.shield_active)
        return;

    int32_t remaining = (int32_t)(game.shield_end_time - now);

    // Flicker as a warning when the shield is about to expire.
    if (remaining < 800 && ((now / 70) % 2) == 0)
        return;

    int x = (int)game.player.x;
    int y = (int)game.player.y;
    int wobble = (int)((now / 100) % 2);

    draw_circle(x, y, 19 + wobble, COL_ACCENT);
    draw_circle(x, y, 21 + wobble, COL_ACCENT_DIM);
}

// ==========================================
// DRAW FUNCTIONS: HUD AND OVERLAYS
// ==========================================

static void draw_hud(uint32_t now)
{
    display_fill_rect(0, 0, SCREEN_WIDTH, HUD_HEIGHT, COL_HUD_BG);
    display_draw_line(0, HUD_HEIGHT - 1, SCREEN_WIDTH - 1,
                      HUD_HEIGHT - 1, COL_ACCENT);

    // Score.
    display_draw_string(6, 3, "SCORE", COL_LABEL, 1);
    display_draw_number(6, 13, (int)game.score, WHITE, 2);

    // Lives as hearts, centred.
    int lives_x = SCREEN_WIDTH / 2 - 23;
    display_draw_string(lives_x, 3, "LIVES", COL_LABEL, 1);
    for (int i = 0; i < INITIAL_LIVES; i++)
    {
        uint16_t color = (i < game.lives) ? COL_PINK : COL_DIM;
        draw_heart(lives_x + i * 16, 14, 2, color);
    }

    // Shield charges as pips, right-aligned.
    int shield_x = SCREEN_WIDTH - 44;
    display_draw_string(shield_x, 3, "SHIELD", COL_LABEL, 1);
    for (int i = 0; i < MAX_SHIELD_CHARGES; i++)
    {
        int px = shield_x + i * 13;

        if (i < game.shield_charges)
            display_fill_rect(px, 14, 10, 11, COL_ACCENT);
        else
            display_draw_rect(px, 14, 10, 11, COL_ACCENT_DIM);
    }

    // Remaining shield time as a bar under the HUD.
    if (game.shield_active)
    {
        int32_t remaining = (int32_t)(game.shield_end_time - now);
        if (remaining < 0)
            remaining = 0;

        int width = (int)((int64_t)SCREEN_WIDTH * remaining /
                          SHIELD_DURATION_MS);
        display_fill_rect(0, HUD_HEIGHT, width, 3, COL_ACCENT);
    }
}

static void draw_level_banner(uint32_t now)
{
    if ((int32_t)(level_banner_until - now) <= 0)
        return;

    char text[16];
    snprintf(text, sizeof(text), "LEVEL %d", current_level);

    uint16_t color = ((now / 150) % 2) ? YELLOW : WHITE;
    draw_text_centered(SCREEN_HEIGHT / 2 - 10, text, color, 3);
}

static void draw_hit_flash(uint32_t now)
{
    if ((int32_t)(hit_flash_until - now) <= 0)
        return;

    // Red border around the whole screen.
    display_fill_rect(0, 0, SCREEN_WIDTH, 4, RED);
    display_fill_rect(0, SCREEN_HEIGHT - 4, SCREEN_WIDTH, 4, RED);
    display_fill_rect(0, 0, 4, SCREEN_HEIGHT, RED);
    display_fill_rect(SCREEN_WIDTH - 4, 0, 4, SCREEN_HEIGHT, RED);
}

static void draw_game(uint32_t now)
{
    display_clear(BLACK);

    draw_stars();
    draw_food(now);
    draw_asteroids();
    draw_bullets();
    draw_particles();
    draw_player(now);
    draw_shield(now);
    draw_popups();

    // HUD last so falling asteroids slide in from behind it.
    draw_hud(now);
    draw_level_banner(now);
    draw_hit_flash(now);

    display_show();
}

static void draw_panel(int x, int y, int w, int h)
{
    display_fill_rect(x, y, w, h, COL_PANEL_BG);
    display_draw_rect(x, y, w, h, COL_ACCENT);
    display_draw_rect(x + 2, y + 2, w - 4, h - 4, COL_ACCENT_DIM);
}

static void draw_title(uint32_t now)
{
    display_clear(BLACK);
    draw_stars();

    // Logo on one line: "SPACE" in cyan, "SHOOTER" in yellow.
    // "SPACE SHOOTER" is 13 characters at scale 3 (18 px each).
    int logo_x = (SCREEN_WIDTH - text_width("SPACE SHOOTER", 3)) / 2;
    display_draw_string(logo_x, 16, "SPACE", COL_ACCENT, 3);
    display_draw_string(logo_x + 6 * 18, 16, "SHOOTER", YELLOW, 3);

    // Bobbing ship.
    int bob = (int)(sinf((float)now / 300.0f) * 3.0f);
    draw_ship(SCREEN_WIDTH / 2, 66 + bob, now, true);

    // Controls card.
    const int panel_w = 220;
    const int panel_h = 88;
    const int panel_x = (SCREEN_WIDTH - panel_w) / 2;
    const int panel_y = 96;
    const int label_x = panel_x + 16;
    const int value_x = panel_x + 84;

    draw_panel(panel_x, panel_y, panel_w, panel_h);

    display_draw_string(label_x, panel_y + 12, "MOVE", COL_LABEL, 1);
    display_draw_string(value_x, panel_y + 12, "JOYSTICK", WHITE, 1);

    display_draw_string(label_x, panel_y + 28, "FIRE", COL_LABEL, 1);
    display_draw_string(value_x, panel_y + 28, "A B C D", WHITE, 1);

    display_draw_string(label_x, panel_y + 44, "SHIELD", COL_LABEL, 1);
    display_draw_string(value_x, panel_y + 44, "CLICK", WHITE, 1);

    display_draw_string(label_x, panel_y + 64, "COLLECT", COL_LABEL, 1);
    fill_circle(value_x + 5, panel_y + 67, 3, COL_CYAN);
    draw_circle(value_x + 5, panel_y + 67, 5, COL_ACCENT);
    display_draw_string(value_x + 16, panel_y + 64, "FOR SHIELD", WHITE, 1);

    if (game.best_score > 0)
    {
        char text[24];
        snprintf(text, sizeof(text), "BEST %lu",
                 (unsigned long)game.best_score);
        draw_text_centered(panel_y + panel_h + 6, text, COL_LABEL, 1);
    }

    if ((now / 500) % 2 == 0)
        draw_text_centered(SCREEN_HEIGHT - 24, "CLICK TO START", WHITE, 2);

    display_show();
}

static void draw_game_over(uint32_t now)
{
    display_clear(BLACK);

    draw_stars();
    draw_particles();

    const int panel_w = 208;
    const int panel_h = 170;
    const int panel_x = (SCREEN_WIDTH - panel_w) / 2;
    const int panel_y = 20;

    draw_panel(panel_x, panel_y, panel_w, panel_h);

    draw_text_centered(panel_y + 16, "GAME OVER", RED, 3);
    display_draw_line(panel_x + 20, panel_y + 46,
                      panel_x + panel_w - 21, panel_y + 46,
                      COL_ACCENT_DIM);

    draw_text_centered(panel_y + 58, "SCORE", COL_LABEL, 1);
    draw_number_centered(panel_y + 70, (int)game.score, YELLOW, 3);

    draw_text_centered(panel_y + 106, "BEST", COL_LABEL, 1);
    draw_number_centered(panel_y + 118, (int)game.best_score, GREEN, 2);

    if (new_best && ((now / 250) % 2 == 0))
        draw_text_centered(panel_y + 144, "NEW BEST", YELLOW, 2);

    // Only offer restart after a short pause so a held button
    // doesn't skip straight past this screen.
    if (now - game_over_time > RESTART_DELAY_MS && (now / 500) % 2 == 0)
        draw_text_centered(SCREEN_HEIGHT - 32, "CLICK TO RESTART",
                           WHITE, 2);

    display_show();
}

// ==========================================
// MAIN GAME LOOP
// ==========================================

static void start_game(uint32_t now)
{
    uint32_t best = game.best_score;
    space_shooter_init();
    game.best_score = best;

    // The click that started the game must not also fire the shield.
    game.shield_button_was_pressed = true;

    current_level = 1;
    level_banner_until = now + LEVEL_BANNER_MS;
    screen_state = STATE_PLAYING;

    sound_music_play(&SONG_GAME);
    sound_sfx_play(&SFX_START);
}

static void update_level(uint32_t now)
{
    int level = (int)(game.score / POINTS_PER_LEVEL) + 1;

    if (level > current_level)
    {
        current_level = level;
        level_banner_until = now + LEVEL_BANNER_MS;
        sound_sfx_play(&SFX_LEVEL_UP);
    }
}

void space_shooter_run(void)
{
    if (!stars_ready)
        init_stars();

    uint32_t previous_time =
        to_ms_since_boot(get_absolute_time());

    bool click_was_pressed = button_pressed(BUTTON_CLICK);

    if (screen_state == STATE_TITLE)
        sound_music_play(&SONG_TITLE);

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        float dt = (float)(now - previous_time) / 1000.0f;
        previous_time = now;

        // Prevent unusually large physics updates.
        if (dt > 0.05f)
            dt = 0.05f;

        bool click = button_pressed(BUTTON_CLICK);
        bool click_edge = click && !click_was_pressed;
        click_was_pressed = click;

        update_stars(dt);
        update_particles(dt);
        update_popups(dt);

        if (screen_state == STATE_TITLE)
        {
            draw_title(now);

            if (click_edge)
                start_game(now);
        }
        else if (screen_state == STATE_GAME_OVER)
        {
            draw_game_over(now);

            // Once the game-over jingle has finished, bring back
            // the title theme while waiting for a restart.
            if (!sound_music_is_playing())
                sound_music_play(&SONG_TITLE);

            if (click_edge && now - game_over_time > RESTART_DELAY_MS)
                start_game(now);
        }
        else
        {
            update_player(dt);
            update_shield(now);

            handle_shooting(now);

            spawn_asteroid(now);
            spawn_food(now);

            check_food_collection();

            update_asteroids(dt);
            update_bullets(dt);

            check_collisions(now);
            update_level(now);

            if (game.game_over)
            {
                new_best = game.score > game.best_score;
                if (new_best)
                    game.best_score = game.score;

                // Big explosion where the ship was.
                spawn_explosion(game.player.x, game.player.y, 40,
                                COL_ORANGE, 140.0f);

                game_over_time = now;
                screen_state = STATE_GAME_OVER;

                sound_music_play(&SONG_GAME_OVER);
                continue;
            }

            draw_game(now);
        }

        sleep_ms(FRAME_TIME_MS);
    }
}
