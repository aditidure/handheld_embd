#ifndef POWER_H
#define POWER_H

#include <stdbool.h>

// ==========================================
// SLEEP MODE ("sleep on close")
// ==========================================
//
// The handheld has no lid, so sleep is entered either manually
// (hold the joystick click for 2 s on the menu) or automatically
// after POWER_IDLE_TIMEOUT_MS without any input on a calm screen
// (menu, title, pause, game over).
//
// While asleep:
//   - sound is stopped
//   - the display panel is switched off and put in sleep mode
//   - the system clock is lowered to POWER_SLEEP_CLOCK_KHZ
//   - core 0 waits in WFI (core 1 is already idle in WFE)
// Any button wakes it up; that press is not passed on to the game.

#define POWER_IDLE_TIMEOUT_MS   60000

// Clock while asleep (0 = keep the normal clock).
#define POWER_SLEEP_CLOCK_KHZ   48000

// Enter sleep now. Returns after the handheld has woken up; the
// caller should simply carry on and draw its next frame.
void power_sleep(void);

// Call once per frame on calm screens. Sleeps if there has been no
// input for POWER_IDLE_TIMEOUT_MS. Returns true if it slept.
bool power_sleep_if_idle(void);

#endif
