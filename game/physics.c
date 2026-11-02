#include "physics.h"

#define GRAVITY       900.0f
#define GROUND_Y      200.0f
#define MOVE_ACCEL    500.0f
#define FRICTION      0.85f
#define MAX_SPEED     200.0f
#define JUMP_SPEED    400.0f

void physics_init(Player *player)
{
    player->x = 50.0f;
    player->y = 180.0f;

    player->vx = 0.0f;
    player->vy = 0.0f;

    player->width = 20.0f;
    player->height = 20.0f;

    player->grounded = 1;
}

void physics_update(Player *player, float dt, float input_x)
{
    // Gravity
    player->vy += GRAVITY * dt;

    // Vertical movement
    player->y += player->vy * dt;

    // Ground collision
    if (player->y + player->height >= GROUND_Y)
    {
        player->y = GROUND_Y - player->height;
        player->vy = 0.0f;
        player->grounded = 1;
    }
    else
    {
        player->grounded = 0;
    }

    // Horizontal acceleration
    player->vx += input_x * MOVE_ACCEL * dt;

    // Friction when joystick is released
    if (input_x == 0.0f)
    {
        player->vx *= 0.90f;
    }

    // Limit speed
    if (player->vx > MAX_SPEED)
        player->vx = MAX_SPEED;

    if (player->vx < -MAX_SPEED)
        player->vx = -MAX_SPEED;

    // Update position
    player->x += player->vx * dt;

    // Screen boundaries (temporary)
    if (player->x < 0)
    {
        player->x = 0;
        player->vx = 0;
    }

    if (player->x + player->width > 240)
    {
        player->x = 240 - player->width;
        player->vx = 0;
    }
}

void player_jump(Player *player)
{
    if (player->grounded)
    {
        player->vy = -JUMP_SPEED;
        player->grounded = 0;
    }
}