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


#include "config.h"
#include "panel.h"
#include "lut.h"

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

static void core0_sio_irq(void)
{
    uint32_t line;
    while (multicore_fifo_rvalid())
        line = multicore_fifo_pop_blocking();

    // printf("core sio\n\r");
    multicore_fifo_clear_irq();
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
        ((uint8_t*)(screenBuf))[i] = 0xf8;

    // Prevent mayhem after flashing
    multicore_reset_core1();
    multicore_launch_core1(lvds_loop);

    irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_sio_irq);
    irq_set_enabled(SIO_IRQ_PROC0, true);

    // Enable backlight
    backlight_init(256 * 64);

    while (1)
    {
        for (uint32_t i = 0; i < ((xRES * yRES) / 4); i++)
        {
            ((uint8_t*)(screenBuf))[i] = (uint8_t) rand();
        }
    }

    return 0;
}
