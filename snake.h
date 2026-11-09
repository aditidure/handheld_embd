#ifndef SNAKE_H
#define SNAKE_H

// Snake game for the handheld menu (landscape).
// Returns when the player chooses D (menu) on the pause or
// game-over screen.
void snake_game_run(void);

// Best score stored in flash (shown on the menu card).
int snake_game_best_score(void);

#endif
