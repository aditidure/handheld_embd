#ifndef SNAKE_TEST_H
#define SNAKE_TEST_H

#include <stdbool.h>
#include "game/score.h"

#define GRID_SIZE 10
#define HUD_HEIGHT 30

#define GRID_WIDTH 24
#define GRID_HEIGHT 29

#define MAX_SNAKE_LENGTH (GRID_WIDTH * GRID_HEIGHT)

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point body[MAX_SNAKE_LENGTH];

    int length;
    int dir_x;
    int dir_y;

    Point food;

    Score score;

    bool game_over;
} SnakeGame;

void snake_init(SnakeGame *game);
void snake_update(SnakeGame *game);
void snake_change_direction(SnakeGame *game, int dx, int dy);

#endif