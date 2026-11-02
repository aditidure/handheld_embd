#include <stdio.h>
#include "pico/stdlib.h"
#include "input/buttons.h"

#define BUTTON_A   2
#define BUTTON_B   3
#define BUTTON_C   4
#define BUTTON_D   5
#define BUTTON_JOY 8

int main(void)
{
stdio_init_all();
sleep_ms(2000);

buttons_init();

printf("\n=============================\n");
printf("        BUTTON TEST\n");
printf("=============================\n");

while (true)
{
    printf("A: %d | B: %d | C: %d | D: %d | JOY: %d\n",
           button_pressed(BUTTON_A),
           button_pressed(BUTTON_B),
           button_pressed(BUTTON_C),
           button_pressed(BUTTON_D),
           button_pressed(BUTTON_JOY));

    sleep_ms(200);
}

return 0;


}
