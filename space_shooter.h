#ifndef SPACE_SHOOTER_H
#define SPACE_SHOOTER_H

#include <stdint.h>

// ==========================================
// SPACE SHOOTER
// ==========================================
//
// Fly a ship with the joystick, shoot falling asteroids (A up,
// B right, D left), collect orbs for shield charges and raise a
// shield with C. Joystick click pauses.

// Runs the game: title screen, play, pause and game over. Returns
// when the player chooses D (menu) on the title, pause or
// game-over screen.
void space_shooter_run(void);

// Best score stored in flash (shown on the menu card).
uint32_t space_shooter_best_score(void);

#endif
