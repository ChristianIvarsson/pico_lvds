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
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"

#include "../build/lvds.pio.h"
#include "../config.h"
#include "../panel.h"

#include "lut.h"
#include "lvds.h"
#include "../utils.h"

#define DMA_CHANNEL_MASK        (1u << LVDS_DMA_CHAN)
#define DMA_CB_CHANNEL_MASK     (1u << LVDS_DMA_CB_CHAN)
#define DMA_CHANNELS_MASK       (DMA_CHANNEL_MASK | LVDS_DMA_CB_CHAN)

#define PIOFREQ    (CPUFREQ / PIODIV)
#define FILL_COLR  (0xf8) // Only the pinkest pink will do

// 9.6k
uint32_t visLine    [ VISLINEBUFFERS ] [ hWORDS ];
uint32_t vBlankLine [ hWORDS ];
uint8_t  screenBuf  [ yRES / 2 ] [ xRES / 2 ] __attribute__((aligned(4)));

static uintptr_t nextLinePtr = (uintptr_t)vBlankLine;

void genLineData(void)
{
    // Generate V blank line
    for (uint32_t i = 0; i < hWORDS; i++)
        vBlankLine[i] = 0x55555555;

    for (uint32_t L = 0; L < VISLINEBUFFERS; L++)
    {
        // Generate visible data
        uint8_t *vPtr = (uint8_t *)&visLine[L][0];
        for (uint32_t i = 0; i < xRES; i++)
        {
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 0];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 1];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 2];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 3];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 4];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 5];
            *vPtr++ = pixLUT[(FILL_COLR * LUTMULTI) + 6];
            *vPtr++ = 0;
        }

        // Generate H blank pixels
        for (uint32_t i = visHWORDS; i < hWORDS; i++)
            visLine[L][i] = 0x55555555;
    }
}

static lvdsData_t lvDat;

// Timing critical so make the whole chain ram-only code
uint64_t __not_in_flash_func(time_us_64_ram)(void) {
    // Need to make sure that the upper 32 bits of the timer
    // don't change, so read that first
    uint32_t hi = timer_hw->timerawh;
    uint32_t lo;
    do {
        // Read the lower 32 bits
        lo = timer_hw->timerawl;
        // Now read the upper 32 bits again and
        // check that it hasn't incremented. If it has loop around
        // and read the lower 32 bits again to get an accurate value
        uint32_t next_hi = timer_hw->timerawh;
        if (hi == next_hi) break;
        hi = next_hi;
    } while (true);
    return ((uint64_t) hi << 32u) | lo;
}

// Fix this junk..
static void __isr __not_in_flash_func(lvdsDMATrigger)(void)
{
    dma_hw->ints0 = DMA_CHANNEL_MASK;

    if (lvDat.line < yRES)
    {
        uint64_t ticks = time_us_64_ram();
        uint32_t idx = (lvDat.line & (VISLINEBUFFERS - 1));
        nextLinePtr = (uintptr_t)visLine[idx];
        drawLineASM2x(screenBuf[lvDat.line/2], visLine[idx], (xRES/2));

        // Wasting less time but is rendering to an active buffer!
        // nextLinePtr = (uintptr_t)visLine[0];
        // if ((nextLineCnt & 1) == 0)
        //     drawLineASM2x(screenBuf[nextLineCnt/2], visLine[0], (xRES/2));
        lvDat.line++;
        sio_hw->fifo_wr = (uint32_t)&lvDat;
        __asm volatile ("sev");
        // Not interested in blanking so only measure drawing
        lvDat.rtime = (uint32_t)(time_us_64_ram() - ticks);
    }
    else
    {
        if (++lvDat.line < (yRES + vBLANK))
        {
            nextLinePtr = (uintptr_t)vBlankLine;
        }
        else
        {
            uint64_t ticks = time_us_64_ram();
            nextLinePtr = (uintptr_t)visLine[0];
            drawLineASM2x(screenBuf[0], visLine[0], (xRES/2));
            lvDat.line = 1;
            sio_hw->fifo_wr = (uint32_t)&lvDat;
            __asm volatile ("sev");
            lvDat.rtime = (uint32_t)(time_us_64_ram() - ticks);
        }
    }
}

static void dma_init(void)
{
    dma_claim_mask(DMA_CHANNELS_MASK);
    dma_channel_config channel_config = dma_channel_get_default_config(LVDS_DMA_CHAN);
    channel_config_set_dreq(&channel_config, pio_get_dreq(LVDS_PIO, LVDS_PIO_SM, true));
    channel_config_set_chain_to(&channel_config, LVDS_DMA_CB_CHAN);
    // 0 is never encountered so must respond on every retrigger
    // channel_config_set_irq_quiet(&channel_config, true);
    channel_config_set_high_priority(&channel_config, true);
    dma_channel_configure(LVDS_DMA_CHAN,
                          &channel_config,
                          &LVDS_PIO->txf[LVDS_PIO_SM],
                          NULL,
                          hWORDS,
                          false);

    dma_channel_config chain_config = dma_channel_get_default_config(LVDS_DMA_CB_CHAN);
    channel_config_set_read_increment(&chain_config, false);
    dma_channel_configure(LVDS_DMA_CB_CHAN,
                          &chain_config,
                          &dma_channel_hw_addr(LVDS_DMA_CHAN)->al3_read_addr_trig,
                          NULL,
                          1,
                          false);

    irq_set_exclusive_handler(DMA_IRQ_0, lvdsDMATrigger);
    dma_channel_set_irq0_enabled(LVDS_DMA_CHAN, true);
    irq_set_enabled(DMA_IRQ_0, true);
}

static void pio_init(void)
{
    uint prog_offs = pio_add_program(LVDS_PIO, &lvds_program);
    pio_sm_config c = lvds_program_get_default_config(prog_offs);

    // Using pins (PINBASE_DAT) to (PINBASE_DAT + 7) for data
    sm_config_set_out_pins(&c, PINBASE_DAT, 8);

    // Set base of clock side-set pins
    sm_config_set_sideset_pins(&c, PINBASE_CLK);

    sm_config_set_out_shift(&c,
                            true, // Shift right
                            true, // autopull
                            32);
    
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_set_pindirs_with_mask(LVDS_PIO, LVDS_PIO_SM,
                                 (0xff << PINBASE_DAT) | (0x03 << PINBASE_CLK),
                                 (0xff << PINBASE_DAT) | (0x03 << PINBASE_CLK));

    for (unsigned int i = PINBASE_CLK; i < (PINBASE_CLK + 2); i++)
        pio_gpio_init(LVDS_PIO, i);

    for (unsigned int i = PINBASE_DAT; i < (PINBASE_DAT + 8); i++)
        pio_gpio_init(LVDS_PIO, i);

    pio_sm_init(LVDS_PIO, LVDS_PIO_SM, prog_offs, &c);

    pio_sm_set_clkdiv_int_frac(LVDS_PIO, LVDS_PIO_SM, PIODIV, 0);

    pio_sm_set_enabled(LVDS_PIO, LVDS_PIO_SM, true);
}

/*
static void lvdsPrintMemLoc(void)
{
    printf("lvds func mem locations:\n\r");
    for (uint32_t i = 0; i < (sizeof(locVerif) / sizeof(locVerif[0])); i++)
        printf("%08x: %s\n\r", locVerif[i].address, locVerif[i].text);
}

static const locVerif_t locVerif[] = {
    {(uintptr_t)&lvdsDMATrigger  , "dma trigger func"    },
    {(uintptr_t)&time_us_64_ram  , "Timer capture func"  },
    {(uintptr_t)&drawLineASM     , "Draw func, asm"      },
    {(uintptr_t)&drawLineASM2x   , "Draw func 2x, asm"   },
};
*/

void __not_in_flash_func(lvds_loop)(void)
{
    // lvdsPrintMemLoc();
    pio_init();
    dma_init();

    // Kickstart DMA
    dma_channel_hw_addr(LVDS_DMA_CB_CHAN)->al3_read_addr_trig = (uintptr_t)&nextLinePtr;

    // Even tho the buffers use 8 bytes for a single pixel, it's still only serializing 7 of those so the delays are correct
    double expFramerate = (PIOFREQ / 7.0) / ((xRES + hBLANK) * (yRES + vBLANK));
    printf("Expected framerate %.3f (lvds clock frequency: %.3f MHz)\n\r", expFramerate, (double)PIOFREQ / 7000000.0);
    printf("One line (including blank) should take %.3f microsec\n\r", (double)(xRES + hBLANK) / (PIOFREQ / 7000000.0));

    uint32_t tMax = 0;
    uint32_t uCnt = 0;

    while (1)
    {
    }
}
