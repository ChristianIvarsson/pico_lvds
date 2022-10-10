#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/structs/systick.h"

#include "hardware/interp.h"

#include "lvds/lut.h"
#include "lvds/lvds.h"

#include "config.h"
#include "panel.h"


#include "font8x8_basic.h"

extern void lvds_loop(void);
extern void genLineData(void);

extern uint32_t visLine    [ VISLINEBUFFERS ] [ hWORDS ];
extern uint32_t vBlankLine [ hWORDS ];
extern uint8_t  screenBuf  [ yRES / 2 ] [ xRES / 2 ];


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

static uint16_t rtimes[(xRES/2)];

volatile static uint32_t lastRtime = 0;
volatile static uint32_t rTimeMin  = 0xffffffff;
volatile static uint32_t rTimeMax  = 0;
volatile static uint32_t rTimeAvg  = 0;

static void __isr __not_in_flash_func(core0_sio_irq)(void)
{
    static uint32_t avgCnt = 0;
    static uint32_t avgT = 0;

    // multicore_fifo_clear_irq();
    sio_hw->fifo_st = 0xff;

    lvdsData_t *tDat = (lvdsData_t*)multicore_fifo_pop_blocking();

    lastRtime = tDat->rtime;
    if (lastRtime > rTimeMax) rTimeMax = lastRtime;
    if (lastRtime < rTimeMin) rTimeMin = lastRtime;

    avgT += lastRtime;

    if (++avgCnt >= 512) {
        rTimeAvg = (avgT >> 9);
        avgCnt = 0;
        avgT = 0;
    }
}

#define TEXT_CLR   (0xf8)
#define GRAPH_CLR  (0xf8)
#define GRAPH_H    (66)

// Ideally you'd want to just reverse the stupid buffer but it came at a significant performance loss (bus contention??)
// Haven't checked closely but it REALLY didn't like reversal when I tried
void printLCD(const uint8_t *str, uint32_t x, uint32_t y, uint32_t transparent)
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

void printGraph(uint32_t yOffs)
{
    static uint32_t cntr = 512;
    uint8_t stats[(xRES/16)];
    sprintf(stats, "rTime: %2u  min: %2u  max: %2u  avg: %2u  stat rst: %4u     ", rtimes[(xRES/2) - 1]&0x3ff, rTimeMin&0x3ff, rTimeMax&0x3ff, rTimeAvg&0x3ff, cntr);
    printLCD(stats, 0, 0, 0);

    // Remove old data
    for (uint32_t i = 0; i < (xRES/2); i++) {
        uint32_t val = rtimes[i];
        if (val >= GRAPH_H) val = GRAPH_H - 1;
        screenBuf[((yRES/2) - (GRAPH_H + yOffs)) + (val)][((xRES/2)-1) - i] = 0;
    }

    for (uint32_t i = 0; i < ((xRES/2) - 1); i++)
        rtimes[i] = rtimes[i + 1];
    rtimes[(xRES/2) - 1] = (uint16_t)lastRtime;

    if (--cntr == 0)
    {
        cntr = 512;
        rTimeMin = 0xffffffff;
        rTimeMax = 0;
    }

    // memset(&screenBuf[((yRES/2) - GRAPH_H) - yOffs][0], 0x00, (xRES/2) * GRAPH_H);
    for (uint32_t i = 0; i < (xRES/2); i++) {
        uint32_t val = rtimes[i];
        if (val >= GRAPH_H) val = GRAPH_H - 1;
        screenBuf[((yRES/2) - (GRAPH_H + yOffs)) + (val)][((xRES/2)-1) - i] = GRAPH_CLR;
    }
}

int main(void)
{
    set_sys_clock_khz(CPUFREQ / 1000, true);
    stdio_init_all();

    time_t ti;
    srand((unsigned) time(&ti));

    systick_hw->csr = 0x5;

    uint32_t fcpu = clock_get_hz(clk_sys);
    printf("\n\nProcessor freq: %.3f MHz\n", (double)fcpu / 1000000.0);

    // Generate LUT's and line data
    genPalette_3x8(defaultPalette);
    genLineData();

    for (uint32_t i = 0; i < ((xRES * yRES) / 4); i++)
        ((uint8_t*)(screenBuf))[i] = 0x00;

    // Prevent mayhem after flashing
    multicore_reset_core1();
    multicore_launch_core1(lvds_loop);

    irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_sio_irq);
    irq_set_enabled(SIO_IRQ_PROC0, true);

    // Enable backlight
    backlight_init(256 * 64);

    while (1)
    {
        for (uint32_t i = 0; i < ((xRES * (yRES - ((GRAPH_H + 8) * 2))) / 4); i++)
        {
            ((uint8_t*)(screenBuf))[i] = (uint8_t) rand();
        }

        printGraph(8);
    }

    return 0;
}
