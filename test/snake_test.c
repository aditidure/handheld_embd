#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "input/buttons.h"
#include "display/display.h"
#include "test/snake_test.h"
#include "game/score.h"

#define MOVE_INTERVAL_MS 150
#define RESTART_DELAY_MS 1000

#define BUTTON_A_GPIO 2
#define BUTTON_B_GPIO 3
#define BUTTON_C_GPIO 4
#define BUTTON_D_GPIO 5


// --------------------------------------------------
// CHECK WHETHER A POINT IS ON THE SNAKE
// --------------------------------------------------

static bool point_on_snake(const SnakeGame *game, Point point)
{
    for (int i = 0; i < game->length; i++) {

        if (game->body[i].x == point.x &&
            game->body[i].y == point.y) {

            return true;
        }
    }

    return false;
}


// --------------------------------------------------
// SPAWN FOOD
// --------------------------------------------------

static void spawn_food(SnakeGame *game)
{
    for (int attempts = 0;
         attempts < GRID_WIDTH * GRID_HEIGHT;
         attempts++) {

        Point candidate = {
            .x = rand() % GRID_WIDTH,
            .y = rand() % GRID_HEIGHT
        };

        if (!point_on_snake(game, candidate)) {

            game->food = candidate;
            return;
        }
    }

    game->game_over = true;
}


// --------------------------------------------------
// INITIALIZE SNAKE
// --------------------------------------------------

void snake_init(SnakeGame *game)
{
    game->length = 3;

    game->dir_x = 1;
    game->dir_y = 0;

    // Reset current score but preserve best score.
    score_reset(&game->score);

    game->game_over = false;

    int start_x = GRID_WIDTH / 2;
    int start_y = GRID_HEIGHT / 2;

    for (int i = 0; i < game->length; i++) {

        game->body[i].x = start_x - i;
        game->body[i].y = start_y;
    }

    spawn_food(game);
}


// --------------------------------------------------
// CHANGE DIRECTION
// --------------------------------------------------

void snake_change_direction(SnakeGame *game, int dx, int dy)
{
    if (game->game_over) {
        return;
    }

    // Prevent direct reversal.
    if (dx == -game->dir_x && dy == -game->dir_y) {
        return;
    }

    if ((dx == 0 && (dy == 1 || dy == -1)) ||
        (dy == 0 && (dx == 1 || dx == -1))) {

        game->dir_x = dx;
        game->dir_y = dy;
    }
}


// --------------------------------------------------
// UPDATE SNAKE
// --------------------------------------------------

void snake_update(SnakeGame *game)
{
    if (game->game_over) {
        return;
    }

    Point next = {
        .x = game->body[0].x + game->dir_x,
        .y = game->body[0].y + game->dir_y
    };

    // Wall collision.
    if (next.x < 0 || next.x >= GRID_WIDTH ||
        next.y < 0 || next.y >= GRID_HEIGHT) {

        game->game_over = true;
        return;
    }

    bool eating = (next.x == game->food.x &&
                   next.y == game->food.y);

    // Check collision with snake body.
    int collision_limit = game->length - (eating ? 0 : 1);

    for (int i = 0; i < collision_limit; i++) {

        if (game->body[i].x == next.x &&
            game->body[i].y == next.y) {

            game->game_over = true;
            return;
        }
    }

    // Grow and increase score when food is eaten.
    if (eating && game->length < MAX_SNAKE_LENGTH) {

        game->length++;

        score_add(&game->score, 10);
    }

    // Move snake.
    for (int i = game->length - 1; i > 0; i--) {

        game->body[i] = game->body[i - 1];
    }

    game->body[0] = next;

    if (eating) {
        spawn_food(game);
    }
}


// --------------------------------------------------
// DRAW GRID CELL
// --------------------------------------------------

static void draw_cell(int grid_x, int grid_y, uint16_t color)
{
    display_fill_rect(
        grid_x * GRID_SIZE,
        grid_y * GRID_SIZE + HUD_HEIGHT,
        GRID_SIZE,
        GRID_SIZE,
        color
    );
}


// --------------------------------------------------
// DRAW SCORE DISPLAY
// --------------------------------------------------

static void draw_score(const SnakeGame *game)
{
    char score_text[20];
    char best_text[20];

    snprintf(score_text, sizeof(score_text),
             "S:%d", score_get_current(&game->score));

    snprintf(best_text, sizeof(best_text),
             "BEST:%d", score_get_best(&game->score));

    display_draw_string(
        8, 7,
        score_text,
        WHITE,
        2
    );

    display_draw_string(
        120, 7,
        best_text,
        YELLOW,
        2
    );

    // Divider between HUD and game area.
    display_draw_line(
        0, HUD_HEIGHT - 1,
        SCREEN_WIDTH - 1, HUD_HEIGHT - 1,
        WHITE
    );
}


// --------------------------------------------------
// DRAW GAME
// --------------------------------------------------

static void draw_game(const SnakeGame *game)
{
    display_clear(BLACK);

    // Draw score and best score.
    draw_score(game);

    // Draw food.
    draw_cell(
        game->food.x,
        game->food.y,
        RED
    );

    // Draw snake.
    for (int i = game->length - 1; i >= 0; i--) {

        draw_cell(
            game->body[i].x,
            game->body[i].y,
            (i == 0) ? YELLOW : GREEN
        );
    }

    display_show();
}


// --------------------------------------------------
// GAME OVER SCREEN
// --------------------------------------------------

static void draw_game_over(const SnakeGame *game)
{
    char score_text[20];
    char best_text[20];

    snprintf(score_text, sizeof(score_text),
             "SCORE:%d", score_get_current(&game->score));

    snprintf(best_text, sizeof(best_text),
             "BEST:%d", score_get_best(&game->score));

    display_clear(BLACK);

    display_draw_string(
        66, 90,
        "GAME OVER",
        RED,
        2
    );

    display_draw_string(
        60, 140,
        score_text,
        WHITE,
        2
    );

    display_draw_string(
        60, 180,
        best_text,
        YELLOW,
        2
    );

    display_show();
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(void)
{
    stdio_init_all();

    buttons_init();
    display_init();

    srand((unsigned int)time_us_32());

    SnakeGame game;

    // Initialize score only once.
    score_init(&game.score);

    // Initialize Snake.
    snake_init(&game);

    uint32_t last_move =
        to_ms_since_boot(get_absolute_time());

    bool game_over_screen_shown = false;

    uint32_t game_over_time = 0;

    draw_game(&game);

    while (true) {

        // ------------------------------------------
        // BUTTON CONTROLS
        // ------------------------------------------

        // A = Down
        if (button_pressed(BUTTON_A_GPIO)) {

            snake_change_direction(&game, 0, -1);
        }

        // B = Up
        else if (button_pressed(BUTTON_B_GPIO)) {

            snake_change_direction(&game, 1, 0);
        }

        // C = Right
        else if (button_pressed(BUTTON_C_GPIO)) {

            snake_change_direction(&game, 0, 1);
        }

        // D = Left
        else if (button_pressed(BUTTON_D_GPIO)) {

            snake_change_direction(&game, -1, 0);
        }


        uint32_t now =
            to_ms_since_boot(get_absolute_time());


        // ------------------------------------------
        // GAME UPDATE
        // ------------------------------------------

        if (!game.game_over &&
            (now - last_move >= MOVE_INTERVAL_MS)) {

            snake_update(&game);

            last_move = now;

            draw_game(&game);
        }


        // ------------------------------------------
        // GAME OVER HANDLING
        // ------------------------------------------

        if (game.game_over) {

            if (!game_over_screen_shown) {

                // Update best score.
                score_update_best(&game.score);

                draw_game_over(&game);

                game_over_screen_shown = true;

                game_over_time = now;
            }


            // Restart after one second.
            if (now - game_over_time >= RESTART_DELAY_MS) {

                snake_init(&game);

                last_move = now;

                game_over_screen_shown = false;

                draw_game(&game);
            }
        }

        sleep_ms(10);
    }

    return 0;
}