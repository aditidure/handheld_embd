# Pico Handheld Game Console

A self-contained, USB-powered handheld game console built on the **Raspberry Pi Pico 2 (RP2350)** with a 320×240 SPI colour display, an analog joystick, buttons and a piezo buzzer. It boots into a game-select menu with two games, **Space Shooter** and **Snake**.

<!-- Add a photo of the device here: ![Handheld](docs/photo.jpg) -->

## Features

| Feature | How it is done |
|---|---|
| Framebuffer rendering | Every frame is drawn into a 320×240 RGB565 framebuffer in RAM, then sent to the panel in one go |
| DMA | Frames are sent to the ST7789 over SPI by DMA |
| Dual-core | Core 1 sends the finished frame to the display while core 0 runs the game and draws the next frame into a second buffer |
| FPS measurement | Frame time, FPS, DMA time and core 0 load are measured every second, shown on screen (C on the menu) and printed over USB serial |
| Clean input | Buttons are sampled every 1 ms in a timer interrupt, debounced (5 ms) and every press is latched, so presses are neither missed nor doubled |
| Persistent scores | Best scores are saved in the last 4 KB of flash, with wear levelling across 16 pages and a CRC32 check on every record |
| Sound | Non-blocking sound effects (pitch slides and noise) driven by a 1 ms timer and PWM |
| Physics | The space ship has acceleration, friction and a speed limit |
| Sleep | Hold the click for 2 s on the menu, or leave it idle for 60 s: the display sleeps, the CPU drops to 48 MHz and waits in WFI. Any button wakes it |
| Pause | Joystick click pauses either game; game timers are shifted on resume so nothing expires while paused |

## Games

**Space Shooter**: fly the ship with the joystick and shoot falling asteroids. Collect cyan orbs to earn shield charges (up to 3), then raise a 3 s shield. Asteroids spawn faster as your score rises, and a level banner appears every 100 points.

**Snake**: steer the snake to eat apples on a 32×21 board. Each apple scores 10 points and makes the snake slightly faster.

## Controls

| Input | Menu | Space Shooter | Snake |
|---|---|---|---|
| Joystick | Choose game (left/right) | Move ship | Steer |
| A | Hold 3 s: reset best scores | Fire up | Up |
| B | Next game | Fire right | Right |
| C | Show/hide FPS counter | Shield | Down |
| D | Previous game | Fire left / **menu** on title, pause and game-over screens | Left / **menu** on pause and game-over screens |
| Joystick click | Tap: play · Hold 2 s: sleep | Start · **Pause / resume** | **Pause / resume** · Play again |
| Any button | Wake from sleep | | |

## Hardware

| Part | Notes |
|---|---|
| Raspberry Pi Pico 2 (RP2350) | 520 KB SRAM, 4 MB flash |
| ST7789 SPI display, 320×240 | Used in landscape |
| Joystick shield | Analog stick with push switch + buttons A–D |
| Piezo buzzer | Driven directly by PWM |
| USB cable | Power and serial output |

### Pin connections

| Pico pin | Connected to | Notes |
|---|---|---|
| GP16 | Display DC | |
| GP17 | Display CS | |
| GP18 | Display SCK | SPI0 |
| GP19 | Display MOSI (SDA) | SPI0 |
| GP20 | Display RST | |
| 3V3 / GND | Display VCC, BL / GND | Backlight is always on |
| GP2 | Button A | Active-low, internal pull-up |
| GP3 | Button B | Active-low, internal pull-up |
| GP4 | Button C | Active-low, internal pull-up |
| GP5 | Button D | Active-low, internal pull-up |
| GP8 | Joystick click | Active-low, internal pull-up |
| GP26 (ADC0) | Joystick X | |
| GP27 (ADC1) | Joystick Y | |
| GP15 | Buzzer + | PWM slice 7 B; buzzer − to GND |

<!-- Add the schematic here: ![Schematic](docs/schematic.png) -->

## Software design

### How a frame is produced (dual-core)

```
           frame N                frame N+1              frame N+2
core 0:  [logic + draw → buf A] [logic + draw → buf B] [logic + draw → buf A] ...
core 1:                         [DMA buf A → display ] [DMA buf B → display ] ...
```

`display_show()` hands the finished buffer to core 1 and immediately swaps to the other buffer, so drawing and transferring happen at the same time. The two cores signal each other through shared flags with memory barriers, and the waiting core sleeps with `WFE` until the other signals with `SEV`. Core 1 also registers for flash lockout, so it is safely paused while core 0 saves a best score.

### Interrupt-driven background tasks

| Task | Rate | Purpose |
|---|---|---|
| Button sampler | 1 kHz | Debounce and latch presses |
| Sound engine | 1 kHz | Advance sound effects, update the PWM frequency |

### Project structure

```
handheld.c           Entry point: hardware init, load scores, start menu
menu.c / .h          Game-select menu (cards, FPS toggle, score reset, sleep)
space_shooter.c / .h Space Shooter game
snake.c / .h         Snake game
display/             ST7789 driver: framebuffer, drawing, font, DMA,
                     dual-core transfer, FPS statistics, panel sleep
input/               Debounced buttons (timer interrupt), analog joystick
sound/               Non-blocking PWM sound effects, note frequencies
save/                Best scores in flash (wear levelling, CRC32)
power/               Sleep mode (manual and idle timeout)
ui/                  Shared drawing helpers, colours, pause box
game/score.c         Current/best score helper
test/                Stand-alone hardware test programs
```

### Adding a game

Write a `mygame_run()` function that returns when the player quits, then add one line to the `games[]` table in `menu.c` with its name, icon drawing function and best-score function.

## Building and flashing

Requirements: [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x (developed with 2.3.1), the ARM GCC toolchain, CMake and Ninja. The Raspberry Pi Pico VS Code extension installs all of these.

```bash
cmake -S . -B build                 # board is set to pico2 in CMakeLists.txt
cmake --build build --target handheld
```

Flash, either:
- **picotool**: `picotool load -f build/handheld.uf2 && picotool reboot`
- **Drag and drop**: hold BOOTSEL while plugging in the Pico, then copy `build/handheld.uf2` to the `RP2350` drive.

Single-core build, for comparing performance:

```bash
cmake -S . -B build -DHANDHELD_DUAL_CORE=OFF
cmake --build build --target handheld
```

### Serial output

Open the Pico's USB serial port in any terminal (`/dev/ttyACM0` on Linux, a COM port on Windows; the baud rate does not matter for USB) to see one line per second:

```
[dual-core] FPS 51.4 | frame 19.5 ms (worst 19.5) | core0 render 3.0 ms, wait 16.5 ms | DMA 19.5 ms
```

## Measurements

| Measurement | Value |
|---|---|
| Frame rate (dual-core, menu) | 51.4 FPS |
| DMA transfer per frame | 19.5 ms (SPI bandwidth is the limit) |
| Core 0 time per frame (logic + drawing) | 3.0 ms (~15 % load) |
| Flash used by firmware | 65.3 KB of 4 MB |
| RAM used | 318 KB of 520 KB (two framebuffers = 300 KB) |
| Frame rate, single-core | *to be measured* |
| Current: menu / gameplay / sleep | *to be measured* |

## Test programs

Each part of the hardware has a small stand-alone test, built alongside the main firmware:

| Target | Tests |
|---|---|
| `display_test` | Display, text rendering |
| `buttons_test` | Buttons |
| `joystick_test` | Joystick ADC (raw values for calibration) |
| `sound_test` | Buzzer tones |
| `physics_test` | Physics module |
| `snake_test` | Early portrait Snake (buttons + display + sound) |
