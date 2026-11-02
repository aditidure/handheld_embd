#ifndef PHYSICS_H
#define PHYSICS_H

typedef struct
{
    float x;
    float y;

    float vx;
    float vy;

    float width;
    float height;

    int grounded;

} Player;

void physics_init(Player *player);

void physics_update(Player *player, float dt, float input_x);

void player_jump(Player *player);

#endif