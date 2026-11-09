#ifndef SPACE_SHOOTER_H
#define SPACE_SHOOTER_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_BULLETS 12
#define MAX_ASTEROIDS 10

#define PLAYER_SIZE 12
#define BULLET_SIZE 4

#define INITIAL_LIVES 3

#define MAX_SHIELD_CHARGES 3
#define SHIELD_DURATION_MS 3000

typedef struct
{
    float x;
    float y;
    float vx;
    float vy;
} Player;

typedef struct
{
    float x;
    float y;
    float speed;
    int radius;
    bool active;

    // Visual-only fields: rotation and a jagged outline shape.
    float angle;
    float spin;
    int8_t shape[8];
} Asteroid;

typedef struct
{
    float x;
    float y;
    float vx;
    float vy;
    bool active;
} Bullet;

typedef struct
{
    Player player;

    Asteroid asteroids[MAX_ASTEROIDS];
    Bullet bullets[MAX_BULLETS];

    int lives;
    int game_over;

    uint32_t score;
    uint32_t best_score;

    uint32_t last_shot_time[4];
    uint32_t last_spawn_time;
    uint32_t last_collision_time;

    int shield_charges;
    bool shield_active;
    uint32_t shield_end_time;
    bool shield_button_was_pressed;

    float food_x;
    float food_y;
    bool food_active;
    uint32_t last_food_spawn_time;
} SpaceShooter;

void space_shooter_init(void);
void space_shooter_run(void);

#endif
