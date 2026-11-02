#include <stdio.h>
#include "pico/stdlib.h"
#include "game/physics.h"
#include "input/buttons.h"
#include "input/joystick.h"

#define BUTTON_A 2

int main(void)
{
stdio_init_all();
sleep_ms(2000);

buttons_init();
joystick_init();

Player player;
physics_init(&player);

absolute_time_t previous_time = get_absolute_time();

printf("\n===== PHYSICS TEST =====\n");

while (true)
{
    absolute_time_t current_time = get_absolute_time();

    float dt = absolute_time_diff_us(previous_time, current_time) / 1000000.0f;
    previous_time = current_time;

    float input_x = joystick_read_x();

    if (button_pressed(BUTTON_A))
    {
        player_jump(&player);
    }

    physics_update(&player, dt, input_x);

    printf("X: %.2f | Y: %.2f | VX: %.2f | VY: %.2f | Grounded: %d\n",
           player.x, player.y,
           player.vx, player.vy,
           player.grounded);

    sleep_ms(20);
}

return 0;

}
