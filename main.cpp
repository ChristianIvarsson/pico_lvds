#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/structs/systick.h"

#include "font8x8_basic.h"

#include "config.h"
#include "panel.h"

#include "lvds/lut.h"
#include "lvds/lvds.h"

#define TEXT_CLR   (0xf8)
#define GRAPH_CLR  (0x3 << 5)
#define GRAPH_H    (52)

// Updated after EVERY visible line has been sent off to the pio engine
volatile static uint32_t lastRtime;
volatile static uint32_t rTimeMin  = 0xffffffff;
volatile static uint32_t rTimeMax;
volatile static uint32_t rTimeAvg;

// Enable backlight
static void backlight_init(const uint16_t level)
{
    gpio_set_function(20, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(20);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&config, 1);
    pwm_init(slice_num, &config, true);

    // "level". What is this garbage? Gief duty cycle and frequency :D
    pwm_set_gpio_level(20, level);
}

static void __isr __not_in_flash_func(core0_sio_irq)(void)
{
    static uint32_t avgCnt, avgT;

    // multicore_fifo_clear_irq();
    sio_hw->fifo_st = 0xff;

    lvdsData_t *tDat = (lvdsData_t*)multicore_fifo_pop_blocking();

    lastRtime = tDat->rtime;
    if (lastRtime > rTimeMax) rTimeMax = lastRtime;
    if (lastRtime < rTimeMin) rTimeMin = lastRtime;

    avgT += lastRtime;
    if (++avgCnt >= 2048) {
        rTimeAvg = (avgT >> 11);
        avgCnt = avgT = 0;
    }
}

void printLCD(const char *str, uint32_t x, uint32_t y, uint32_t transparent)
{
    if (!transparent) {
        while (*str) {
            for (uint32_t iY = 0; iY < 8; iY++)
            for (uint32_t iX = 0; iX < 8; iX++)
                screenBuf[((yRES/2)-1) - (y + iY)][((xRES/2)-1) - (iX + x)] = (font8x8_basic[*str][iY] & (1 << iX)) ? TEXT_CLR : 0x00;

            x += 8;
            str++;
        }
    } else {
        while (*str) {
            for (uint32_t iY = 0; iY < 8; iY++)
            for (uint32_t iX = 0; iX < 8; iX++)
                if (font8x8_basic[*str][iY] & (1 << iX))
                    screenBuf[((yRES/2)-1) - (y + iY)][((xRES/2)-1) - (iX + x)] = TEXT_CLR;
            x += 8;
            str++;
        }
    }
}

void printGraph(void)
{
    static uint8_t rtimes[(xRES/2) + 1];
    static uint32_t cntr = 512;
    char stats[(xRES/16) + 32];

    // Force capture here
    volatile uint32_t rTime = lastRtime;

    sprintf(stats, "rTime: %.1f  min: %.1f  max: %.1f  avg: %.1f  stat rst: %3u     ",
        (double)rTime   / 252.0,
        (double)rTimeMin / 252.0,
        (double)rTimeMax / 252.0,
        (double)rTimeAvg / 252.0,
        cntr);

    printLCD(stats, 0, 0, 0);

    rTime /= 252;
    if (rTime >= GRAPH_H) rTime = GRAPH_H - 1;
    rtimes[(xRES/2)] = (uint8_t)rTime;

    if (--cntr == 0) {
        cntr = 512;
        rTimeMin = 0xffffffff;
        rTimeMax = 0;
    }

    for (uint32_t i = 0; i < (xRES/2); i++) {
        screenBuf[((yRES/2) - (GRAPH_H + 8)) + (rtimes[i])][((xRES/2)-1) - i] = 0;
        screenBuf[((yRES/2) - (GRAPH_H + 8)) + (rtimes[i+1])][((xRES/2)-1) - i] = GRAPH_CLR;
        rtimes[i] = rtimes[i+1];
    }
}

int main(void)
{
    time_t ti;
    char stats[128];

    set_sys_clock_khz(CPUFREQ / 1000, true);
    stdio_init_all();
    // Set systick to processor clock
    systick_hw->csr = 0x5;

    srand((unsigned) time(&ti));

    // Generate LUT's and line data
    genPalette_3x8(rgb332Palette, 0);
    genLineData();

    // Prevent mayhem after flashing
    multicore_reset_core1();
    multicore_launch_core1(lvds_loop);

    irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_sio_irq);
    irq_set_enabled(SIO_IRQ_PROC0, true);

    // Enable backlight
    backlight_init(256 * 64);

    while (1)
    {
        uint32_t drawTime = systick_hw->cvr;

        // TV snow
        for (uint32_t y = 0; y < 240; y++) {
            for (uint32_t x = 0; x < 320; x++)
                screenBuf[y][x] = (uint8_t)rand();
        }

        printGraph();

        drawTime = (drawTime - systick_hw->cvr) & 0xffffff;
        sprintf(stats, "dTime: %.1f",
            (double)drawTime/252.0);
        printLCD(stats, 0, 150, 0);
    }

    return 0;
}
