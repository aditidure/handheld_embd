#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include "pico/stdlib.h"

// ==========================================
// DEBOUNCED BUTTON INPUT
// ==========================================
//
// A 1 ms timer interrupt samples every button. A button only changes
// state after its pin has read the same value for BUTTON_DEBOUNCE_MS
// in a row, so contact bounce is filtered out. Every debounced press
// is latched, so even a tap shorter than one frame is never missed.
//
// Usage in a game loop:
//
//     while (true) {
//         buttons_poll();                      // once per frame
//         if (button_just_pressed(BTN_CLICK))  // true for 1 frame
//             ...
//         if (button_down(BTN_A))              // true while held
//             ...
//     }

typedef enum
{
    BTN_A = 0,      // GP2
    BTN_B,          // GP3
    BTN_C,          // GP4
    BTN_D,          // GP5
    BTN_CLICK,      // GP8 (joystick press)
    BTN_COUNT
} Button;

#define BUTTON_DEBOUNCE_MS  5

// Configure the pins and start the 1 ms sampling timer.
void buttons_init(void);

// Take a snapshot of the presses since the previous call.
// Call exactly once at the start of every frame.
void buttons_poll(void);

// True while the button is held (debounced).
bool button_down(Button button);

// True on the frame (snapshot) in which the button was pressed.
bool button_just_pressed(Button button);

// Forget any presses that have not been polled yet, e.g. when
// switching screens so a stale press does not carry over.
void buttons_clear(void);

// True if any button is currently held (debounced).
bool buttons_any_down(void);

// Time (ms since boot) of the most recent debounced press of any
// button. Used to detect inactivity for auto-sleep.
uint32_t buttons_last_press_ms(void);

// Raw, NOT debounced pin read. Kept for the hardware test programs.
// Returns 1 if the button on `gpio` is pressed, 0 otherwise.
int button_pressed(uint gpio);

#endif // BUTTONS_H
