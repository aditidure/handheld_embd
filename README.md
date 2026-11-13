# Pico Handheld Game Console

A self-contained, USB-powered handheld game console built on the **Raspberry Pi Pico 2 (RP2350)** with a 320×240 SPI colour display, an analog joystick, buttons, and sound through a PAM class-D amplifier and speaker. It boots into a game-select menu with two games, **Space Shooter** and **Snake**.

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
| PAM8302 class-D amplifier module | Mono amplifier for the PWM audio signal; powered from the Pico's 3.3 V |
| Speaker, 8 Ω 0.5 W | Driven by the amplifier's bridged output (keep the amp's volume moderate) |
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
| GP15 | Amplifier audio input | PWM slice 7 B; amplifier input ground to Pico GND |
| 3V3 / GND | Amplifier VCC / GND | Speaker on amplifier OUT+ / OUT− (never OUT− to GND) |

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

| File | What it does |
|---|---|
| `handheld.c` | Entry point: initialises the hardware, loads saved scores, starts the menu |
| `menu.c/.h` | Game-select menu: game cards, FPS toggle (C), score reset (hold A), sleep (hold click) |
| `space_shooter.c/.h` | Space Shooter game |
| `snake.c/.h` | Snake game |
| `display/display.c/.h` | ST7789 driver: SPI + DMA transfer, framebuffers, dual-core hand-off, drawing primitives, 5×7 font, FPS statistics, panel sleep |
| `input/buttons.c/.h` | 1 kHz interrupt sampling, 5 ms debounce, press latching, idle tracking |
| `input/joystick.c/.h` | ADC reading, calibration, dead zone |
| `sound/sound.c/.h` | Non-blocking PWM sound effects (1 kHz interrupt, pitch slides, noise, priorities) |
| `sound/notes.h` | Musical note frequencies |
| `save/save.c/.h` | Best scores in flash: wear levelling over 16 pages, CRC32, safe writes with core 1 running |
| `power/power.c/.h` | Sleep and wake: display sleep, lower clock, WFI |
| `ui/ui.c/.h` | Shared drawing helpers (centred text, circles, triangles, panels, pause box) and colour palette |
| `test/*_test.c` | Stand-alone hardware test and benchmark programs (see below) |
| `report/` | Project report (`report.tex`, compiled `report.pdf`) |
| `CMakeLists.txt` | Build file: firmware + test targets, `HANDHELD_DUAL_CORE` option |
| `pico_sdk_import.cmake` | Standard Pico SDK locator (from the SDK) |
| `.vscode/` | Settings for the Raspberry Pi Pico VS Code extension |

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
| Flash used by firmware | 63.7 KB of 4 MB |
| RAM used | 310 KB of 520 KB (two framebuffers = 300 KB) |
| Frame rate, single-core | *to be measured* |
| Current: menu / gameplay / sleep | *to be measured* |

## Test programs

Each part of the hardware has a small stand-alone test, built alongside the main firmware:

| Target | Checks | Output |
|---|---|---|
| `display_test` | Wiring, orientation, colours, FPS | Border, colour bars, moving square on screen |
| `buttons_test` | Debouncing | Serial: state of each button and a press counter (+1 per press) |
| `joystick_test` | ADC and calibration | Serial: raw 0–4095 values and normalised −1…+1 |
| `sound_test` | PWM, amplifier, speaker | A C-major scale, then a rising/falling sweep |
| `benchmark_test` | Numbers for the report | Serial: clock and SPI rates, ADC and mux timing, drawing costs, frame time |

Build one with e.g. `cmake --build build --target buttons_test` and flash `build/buttons_test.uf2`.
