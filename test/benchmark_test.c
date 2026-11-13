// ==========================================
// BENCHMARK (numbers for the report)
// ==========================================
//
// Measures clock frequencies, the real SPI speed, ADC conversion and
// multiplexer-switching time, GPIO read time and the cost of the main
// drawing operations, then prints a table over USB serial.
//
// Each operation is repeated many times and timed with the 1 MHz
// hardware timer, so results are averages in nanoseconds or
// microseconds. The 1 kHz button interrupt keeps running, so results
// include its small (< 1 %) background load, as in the real games.

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "display/display.h"
#include "input/joystick.h"
#include "input/buttons.h"
#include "ui/ui.h"

#define ADC_LOOPS     20000
#define GPIO_LOOPS    100000
#define DRAW_LOOPS    200
#define SMALL_LOOPS   2000
#define FRAME_LOOPS   100

// Results are added here so the compiler cannot optimise loops away.
static volatile uint32_t sink;

static uint32_t now_us(void)
{
    return time_us_32();
}

static float ns_each(uint32_t elapsed_us, int loops)
{
    return (float)elapsed_us * 1000.0f / (float)loops;
}

static float us_each(uint32_t elapsed_us, int loops)
{
    return (float)elapsed_us / (float)loops;
}

// "configured" = the value the SDK set up (clock_get_hz).
// "measured"   = counted by the RP2350's hardware frequency counter,
//                which counts the clock's edges against the 12 MHz
//                crystal, so it confirms the clock really runs at that
//                speed.
static void print_clock(const char *name, clock_num_t clk, uint fc_src)
{
    printf("%-22s: configured %9lu Hz | measured %9lu kHz\n", name,
           (unsigned long)clock_get_hz(clk),
           (unsigned long)frequency_count_khz(fc_src));
}

static void print_clocks(void)
{
    printf("\n--- Clocks ---\n");
    print_clock("clk_sys  (CPU cores)", clk_sys, CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    print_clock("clk_peri (SPI)", clk_peri, CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    print_clock("clk_adc  (ADC)", clk_adc, CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    print_clock("clk_usb  (USB)", clk_usb, CLOCKS_FC0_SRC_VALUE_CLK_USB);
    print_clock("clk_ref  (reference)", clk_ref, CLOCKS_FC0_SRC_VALUE_CLK_REF);

    // The SPI hardware can only divide clk_peri by certain values, so the
    // rate it actually runs at differs from the requested 80 MHz.
    printf("SPI0 actual baud rate : %lu Hz (requested 80000000)\n",
           (unsigned long)spi_get_baudrate(spi0));
}

static void bench_adc(void)
{
    printf("\n--- ADC / joystick ---\n");

    // Same channel every time: pure conversion time.
    adc_select_input(0);
    uint32_t t = now_us();
    for (int i = 0; i < ADC_LOOPS; i++)
        sink += adc_read();
    uint32_t same = now_us() - t;

    // Switch the multiplexer before every conversion.
    t = now_us();
    for (int i = 0; i < ADC_LOOPS; i++)
    {
        adc_select_input((uint)(i & 1));
        sink += adc_read();
    }
    uint32_t alternating = now_us() - t;

    // Multiplexer register write on its own.
    t = now_us();
    for (int i = 0; i < ADC_LOOPS; i++)
        adc_select_input((uint)(i & 1));
    uint32_t select_only = now_us() - t;

    // Full joystick read: select + convert + normalise (float maths).
    t = now_us();
    for (int i = 0; i < ADC_LOOPS; i++)
        sink += (uint32_t)(joystick_read_x() * 1000.0f);
    uint32_t joy = now_us() - t;

    float conv_ns = ns_each(same, ADC_LOOPS);
    float alt_ns = ns_each(alternating, ADC_LOOPS);

    printf("adc_read, same channel        : %.0f ns  (%.0f k samples/s)\n",
           conv_ns, 1e6f / conv_ns);
    printf("select + adc_read, alternating: %.0f ns\n", alt_ns);
    printf("mux switching overhead        : %.0f ns per switch\n",
           alt_ns - conv_ns);
    printf("adc_select_input alone        : %.0f ns\n",
           ns_each(select_only, ADC_LOOPS));
    printf("joystick_read_x (full)        : %.0f ns\n", ns_each(joy, ADC_LOOPS));

    // Crosstalk check: does the previous channel's voltage leak into
    // the first sample after switching? Compare X read right after Y
    // with X read after X (stick at rest).
    int64_t diff_sum = 0;
    for (int i = 0; i < 1000; i++)
    {
        adc_select_input(0);
        uint16_t x_settled = adc_read();
        adc_select_input(1);
        (void)adc_read();
        adc_select_input(0);
        uint16_t x_after_switch = adc_read();
        diff_sum += (int)x_after_switch - (int)x_settled;
    }
    printf("first sample after switch     : %+.1f counts vs settled "
           "(keep stick still)\n", (float)diff_sum / 1000.0f);
}

static void bench_gpio(void)
{
    printf("\n--- GPIO (SIO) ---\n");

    const uint pins[5] = { 2, 3, 4, 5, 8 };

    uint32_t t = now_us();
    for (int i = 0; i < GPIO_LOOPS; i++)
        sink += gpio_get(pins[i % 5]);
    uint32_t elapsed = now_us() - t;

    printf("gpio_get                      : %.1f ns (incl. loop)\n",
           ns_each(elapsed, GPIO_LOOPS));
}

static void bench_drawing(void)
{
    printf("\n--- Drawing into the framebuffer (CPU, core 0) ---\n");

    uint32_t t = now_us();
    for (int i = 0; i < DRAW_LOOPS; i++)
        display_clear(BLACK);
    printf("display_clear (full screen)   : %.1f us\n",
           us_each(now_us() - t, DRAW_LOOPS));

    t = now_us();
    for (int i = 0; i < DRAW_LOOPS; i++)
        display_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLUE);
    printf("fill_rect (full screen)       : %.1f us\n",
           us_each(now_us() - t, DRAW_LOOPS));

    t = now_us();
    for (int i = 0; i < SMALL_LOOPS; i++)
        display_draw_string(10, 10, "SPACE SHOOTER 12345", WHITE, 1);
    printf("draw_string (19 chars, x1)    : %.1f us\n",
           us_each(now_us() - t, SMALL_LOOPS));

    t = now_us();
    for (int i = 0; i < SMALL_LOOPS; i++)
        display_draw_line(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, RED);
    printf("draw_line (full diagonal)     : %.1f us\n",
           us_each(now_us() - t, SMALL_LOOPS));

    t = now_us();
    for (int i = 0; i < SMALL_LOOPS; i++)
        ui_fill_circle(160, 120, 10, GREEN);
    printf("fill_circle (r = 10)          : %.1f us\n",
           us_each(now_us() - t, SMALL_LOOPS));

    t = now_us();
    for (int i = 0; i < SMALL_LOOPS; i++)
        ui_fill_triangle(160, 100, 148, 124, 172, 124, YELLOW);
    printf("fill_triangle (ship hull)     : %.1f us\n",
           us_each(now_us() - t, SMALL_LOOPS));
}

static void bench_frames(void)
{
    printf("\n--- Full frames (clear + show) ---\n");

    // Warm up so both buffers are in use.
    for (int i = 0; i < 5; i++)
    {
        display_clear(BLACK);
        display_show();
    }

    uint32_t t = now_us();
    for (int i = 0; i < FRAME_LOOPS; i++)
    {
        display_clear((i & 1) ? BLACK : WHITE);
        display_show();
    }
    float frame_us = us_each(now_us() - t, FRAME_LOOPS);

    DisplayStats s = display_get_stats();
    printf("frame time                    : %.0f us (%.1f FPS)\n",
           frame_us, 1e6f / frame_us);
    printf("DMA transfer per frame        : %.1f ms\n", s.transfer_ms);
    printf("core 0 wait per frame         : %.1f ms\n", s.wait_ms);
    printf("mode                          : %s\n",
           DISPLAY_DUAL_CORE ? "dual-core" : "single-core");
}

static void run_all(void)
{
    printf("\n========== HANDHELD BENCHMARK ==========\n");
    printf("(keep the joystick at rest during the ADC test)\n");

    print_clocks();
    bench_adc();
    bench_gpio();
    bench_drawing();
    bench_frames();

    printf("\n========== DONE ==========\n");
    printf("Press the joystick click to run again.\n");
}

int main(void)
{
    stdio_init_all();

    display_init();
    joystick_init();
    buttons_init();

    // Wait (as long as needed) until a serial monitor is opened, so no
    // output is lost. The screen says what is happening meanwhile.
    display_clear(BLACK);
    ui_text_centered(100, "BENCHMARK", COL_ACCENT, 3);
    ui_text_centered(140, "OPEN THE SERIAL MONITOR", WHITE, 1);
    display_show();

    while (!stdio_usb_connected())
        sleep_ms(100);

    sleep_ms(500);   // let the terminal settle
    run_all();

    // Press the joystick click to run the benchmark again.
    while (true)
    {
        buttons_poll();
        if (button_just_pressed(BTN_CLICK))
            run_all();
        sleep_ms(20);
    }
}
