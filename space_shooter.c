#include "space_shooter.h"

#include "pico/stdlib.h"
#include "display/display.h"
#include "input/joystick.h"
#include "input/buttons.h"
#include "sound/sound.h"

#include <stdlib.h>
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

// ==========================================
// GAME STATE
// ==========================================

static SpaceShooter game;

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

            sound_play_tone(1200, 12);
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
        sound_play_tone(1500, 60);
    }
}

static void update_shield(uint32_t now)
{
    bool pressed = button_pressed(8);

    // Activate only on a fresh press, not continuously while held.
    if (pressed && !game.shield_button_was_pressed)
    {
        if (game.shield_charges > 0 && !game.shield_active)
        {
            game.shield_charges--;
            game.shield_active = true;
            game.shield_end_time = now + SHIELD_DURATION_MS;
            sound_play_tone(1000, 60);
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
            game.asteroids[i].x =
                (float)(15 + rand() % (SCREEN_WIDTH - 30));
            game.asteroids[i].y = -15.0f;
            game.asteroids[i].radius = 7 + rand() % 7;
            game.asteroids[i].speed = 65.0f + (float)(rand() % 65);
            game.asteroids[i].active = true;

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
                sound_play_tone(1000, 40);
                break;
            }

            // During cooldown, leave the asteroid active and do not
            // subtract another life.
            if (now - game.last_collision_time < COLLISION_COOLDOWN)
                continue;

            game.asteroids[i].active = false;
            game.lives--;
            game.last_collision_time = now;

            sound_play_tone(300, 150);

            if (game.lives <= 0)
                game.game_over = 1;

            break;
        }
    }
}

// ==========================================
// DRAW FUNCTIONS
// ==========================================

static void draw_player(void)
{
    int x = (int)game.player.x;
    int y = (int)game.player.y;

    // Spaceship triangle.
    display_draw_line(x, y - PLAYER_SIZE,
                      x - PLAYER_SIZE, y + PLAYER_SIZE, GREEN);
    display_draw_line(x, y - PLAYER_SIZE,
                      x + PLAYER_SIZE, y + PLAYER_SIZE, GREEN);
    display_draw_line(x - PLAYER_SIZE, y + PLAYER_SIZE,
                      x + PLAYER_SIZE, y + PLAYER_SIZE, GREEN);

    // Cockpit.
    display_fill_rect(x - 2, y - 2, 4, 5, WHITE);
}

static void draw_asteroids(void)
{
    for (int i = 0; i < MAX_ASTEROIDS; i++)
    {
        if (!game.asteroids[i].active)
            continue;

        int x = (int)game.asteroids[i].x;
        int y = (int)game.asteroids[i].y;
        int r = game.asteroids[i].radius;

        display_draw_line(x, y - r, x + r, y, RED);
        display_draw_line(x + r, y, x, y + r, RED);
        display_draw_line(x, y + r, x - r, y, RED);
        display_draw_line(x - r, y, x, y - r, RED);
    }
}

static void draw_bullets(void)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!game.bullets[i].active)
            continue;

        display_fill_rect((int)game.bullets[i].x - 2,
                          (int)game.bullets[i].y - 2,
                          BULLET_SIZE, BULLET_SIZE, YELLOW);
    }
}

static void draw_food(void)
{
    if (!game.food_active)
        return;

    int x = (int)game.food_x;
    int y = (int)game.food_y;

    // Yellow food pickup.
    display_fill_rect(x - 5, y - 5, 10, 10, YELLOW);
    display_draw_line(x - 3, y, x + 3, y, GREEN);
    display_draw_line(x, y - 3, x, y + 3, GREEN);
}

static void draw_shield(void)
{
    if (!game.shield_active)
        return;

    int x = (int)game.player.x;
    int y = (int)game.player.y;

    // Blue protective diamond around the spaceship.
    display_draw_line(x, y - 20, x + 20, y, BLUE);
    display_draw_line(x + 20, y, x, y + 20, BLUE);
    display_draw_line(x, y + 20, x - 20, y, BLUE);
    display_draw_line(x - 20, y, x, y - 20, BLUE);
}

static void draw_hud(void)
{
    display_draw_string(5, 5, "S:", WHITE, 1);
    display_draw_number(20, 5, (int)game.score, YELLOW, 1);

    display_draw_string(105, 5, "L:", WHITE, 1);
    display_draw_number(120, 5, game.lives, GREEN, 1);

    display_draw_string(160, 5, "SH:", WHITE, 1);
    display_draw_number(181, 5, game.shield_charges, BLUE, 1);

    display_draw_line(0, 30, SCREEN_WIDTH, 30, WHITE);
}

static void draw_game(void)
{
    display_clear(BLACK);

    draw_hud();
    draw_food();
    draw_asteroids();
    draw_bullets();
    draw_player();
    draw_shield();

    display_show();
}

static void draw_game_over(void)
{
    display_clear(BLACK);

    display_draw_string(55, 100, "GAME OVER", RED, 2);

    display_draw_string(65, 150, "SCORE:", WHITE, 1);
    display_draw_number(115, 150, (int)game.score, YELLOW, 1);

    display_draw_string(45, 190, "BEST:", WHITE, 1);
    display_draw_number(85, 190, (int)game.best_score, GREEN, 1);

    display_draw_string(25, 250, "CLICK TO RESTART", WHITE, 1);

    display_show();
}

// ==========================================
// MAIN GAME LOOP
// ==========================================

void space_shooter_run(void)
{
    uint32_t previous_time =
        to_ms_since_boot(get_absolute_time());

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        float dt = (float)(now - previous_time) / 1000.0f;
        previous_time = now;

        // Prevent unusually large physics updates.
        if (dt > 0.05f)
            dt = 0.05f;

        if (game.game_over)
        {
            draw_game_over();

            if (button_pressed(8))
            {
                // Wait for the click to be released before restarting.
                while (button_pressed(8))
                    sleep_ms(10);

                uint32_t best = game.best_score;
                space_shooter_init();
                game.best_score = best;

                previous_time =
                    to_ms_since_boot(get_absolute_time());
            }

            sleep_ms(FRAME_TIME_MS);
            continue;
        }

        update_player(dt);
        update_shield(now);

        handle_shooting(now);

        spawn_asteroid(now);
        spawn_food(now);

        check_food_collection();

        update_asteroids(dt);
        update_bullets(dt);

        check_collisions(now);

        if (game.game_over)
        {
            if (game.score > game.best_score)
                game.best_score = game.score;

            continue;
        }

        draw_game();
        sleep_ms(FRAME_TIME_MS);
    }
}
