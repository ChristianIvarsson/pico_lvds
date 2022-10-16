#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"

#include "../build/misc.pio.h"

#include "config.h"
#include "utils.h"

static uint32_t cntrOffs = 0;
void pio_init_counter(void)
{
    cntrOffs = pio_add_program(COUNTER_PIO, &count_pulse_program);
    pio_sm_config c = count_pulse_program_get_default_config(cntrOffs);

    sm_config_set_in_shift(&c,
                            true, // Shift right
                            true, // Autopush
                            32); // Every 32 bits, push
    sm_config_set_out_shift(&c,
                            true,
                            true,  // autopull
                            32);

    pio_sm_init(COUNTER_PIO, COUNTER_SM, cntrOffs, &c);
    pio_sm_set_clkdiv_int_frac(COUNTER_PIO, COUNTER_SM, 1, 0);
    pio_sm_set_enabled(COUNTER_PIO, COUNTER_SM, true);
}

double __time_critical_func (countTicks)(const uint32_t pin, const uint32_t count)
{
    if (count == 0) return 0.1;
    pio_sm_set_enabled(COUNTER_PIO, COUNTER_SM, false);
    pio_sm_clear_fifos(COUNTER_PIO, COUNTER_SM);
    pio_sm_exec(COUNTER_PIO, COUNTER_SM, pio_encode_jmp(cntrOffs));
    pio_sm_set_enabled(COUNTER_PIO, COUNTER_SM, true);

    COUNTER_PIO->sm[COUNTER_SM].execctrl =
        (COUNTER_PIO->sm[COUNTER_SM].execctrl & ~PIO_SM0_EXECCTRL_JMP_PIN_BITS) |
        (pin << PIO_SM0_EXECCTRL_JMP_PIN_LSB);

    COUNTER_PIO->txf[COUNTER_SM] = count - 1;

    // Capture current processor tick and time in micros.
    volatile uint32_t cTime = systick_hw->cvr;
    volatile uint32_t uTime = time_us_32();

    // Wait for state machine to push
    while (pio_sm_is_rx_fifo_empty(COUNTER_PIO, COUNTER_SM))  ;

    // Capture delta since
    cTime = (cTime - systick_hw->cvr) & 0xffffff;
    uTime = (time_us_32() - uTime);

    if (uTime == 0) uTime = 1;
    if (uTime < (16777215UL / (CPUFREQ / 1000000UL)))
        return (double)cTime / (CPUFREQ / 1000000UL);

    return uTime;
}
