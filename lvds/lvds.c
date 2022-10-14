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

#define DMA_LO_CHANNEL_MASK        (1u << LVDS_LO_DMA_CHAN)
#define DMA_LO_CB_CHANNEL_MASK     (1u << LVDS_LO_DMA_CB_CHAN)
#define DMA_LO_CHANNELS_MASK       (DMA_LO_CHANNEL_MASK | DMA_LO_CB_CHANNEL_MASK)

#define DMA_HI_CHANNEL_MASK        (1u << LVDS_HI_DMA_CHAN)
#define DMA_HI_CB_CHANNEL_MASK     (1u << LVDS_HI_DMA_CB_CHAN)
#define DMA_HI_CHANNELS_MASK       (DMA_HI_CHANNEL_MASK | DMA_HI_CB_CHANNEL_MASK)

#define FILL_COLR  (0xf8) // Only the pinkest pink will do

// Decide what to do..
uint32_t visLine    [ VISLINEBUFFERS ] [ FULL_LINE ];
uint32_t vBlankLine [ FULL_LINE ];

uint8_t  screenBuf  [ yRES / 2 ] [ xRES / 2 ] __attribute__((aligned(4)));

static uintptr_t nextLinePtr = (uintptr_t)vBlankLine;

static lvdsData_t lvDat;

void genLineData(void)
{
    // Generate V blank line
    for (uint32_t i = 0; i < FULL_LINE; i++)
        vBlankLine[i] = ( 0b0000000000000000 << 16 |
                          0b0000000000000000 );

    for (uint32_t L = 0; L < VISLINEBUFFERS; L++)
    {
        // Generate visible data
        for (uint32_t i = 0; i < xRES; i++)
            visLine[L][i] = pixLUT[ FILL_COLR ];

        // Generate H blank pixels
        for (uint32_t i = xRES; i < FULL_LINE; i++)
            visLine[L][i] = ( 0b0000000000000000 << 16 |
                              0b0000000000000000 );
    }
}

static int32_t rqLutchg = -1;
static uint32_t *newLut;

void reqLutChange(const uint32_t *lut, uint16_t line)
{
    if (lut) {
        newLut = lut;
        rqLutchg = (int32_t)line;
    }
}

// High channel is the one responsible of interrupts
static void __isr __not_in_flash_func(lvdsDMATrigger)(void)
{
    dma_hw->ints0 = DMA_HI_CHANNEL_MASK;

    // (line & 3)
    // 0 -> 1 Render / tag buffer 0
    // 1 -> 2 Do nothing
    // 2 -> 3 Render / tag buffer 1
    // 3 -> 0 Do nothing

    if (lvDat.line < yRES) {
        if ((lvDat.line & 1) == 0) {
            uint32_t ticks = systick_hw->cvr;
            uint32_t idx = ((lvDat.line >> 1) & (VISLINEBUFFERS - 1));
            if (rqLutchg == (lvDat.line >> 1)) currentLUT = newLut;
            nextLinePtr = (uintptr_t)visLine[idx];
            drawLineASM2x(screenBuf[lvDat.line/2], visLine[idx], (xRES/2));
            lvDat.line++;
            sio_hw->fifo_wr = (uint32_t)&lvDat;
            __asm volatile ("sev");
            lvDat.rtime = (ticks - systick_hw->cvr)&0xffffff;
        } else {
            lvDat.line++;
        }
    } else {
        if (++lvDat.line < (yRES + vBLANK)) {
            nextLinePtr = (uintptr_t)vBlankLine;
        } else {
            uint32_t ticks = systick_hw->cvr;
            if (rqLutchg == 0) currentLUT = newLut;
            nextLinePtr = (uintptr_t)visLine[0];
            drawLineASM2x(screenBuf[0], visLine[0], (xRES/2));
            lvDat.line = 1;
            sio_hw->fifo_wr = (uint32_t)&lvDat;
            __asm volatile ("sev");
            lvDat.rtime = (ticks - systick_hw->cvr)&0xffffff;
        }
    }
}

static void dma_init(void)
{
    dma_claim_mask(DMA_HI_CHANNELS_MASK | DMA_LO_CHANNELS_MASK);

    /////////////////////////////////////////////////////////////////////////////////////
    // Low pairs
    dma_channel_config channel_config = dma_channel_get_default_config(LVDS_LO_DMA_CHAN);
    channel_config_set_dreq(&channel_config, pio_get_dreq(LVDS_PIO, LVDS_LO_SM, true));
    channel_config_set_chain_to(&channel_config, LVDS_LO_DMA_CB_CHAN);

    channel_config_set_high_priority(&channel_config, true);
    dma_channel_configure(LVDS_LO_DMA_CHAN,
                          &channel_config,
                          &LVDS_PIO->txf[LVDS_LO_SM],
                          NULL,
                          FULL_LINE,
                          false);

    dma_channel_config chain_config = dma_channel_get_default_config(LVDS_LO_DMA_CB_CHAN);
    channel_config_set_read_increment(&chain_config, false);
    dma_channel_configure(LVDS_LO_DMA_CB_CHAN,
                          &chain_config,
                          &dma_channel_hw_addr(LVDS_LO_DMA_CHAN)->al3_read_addr_trig,
                          NULL,
                          1,
                          false);

    /////////////////////////////////////////////////////////////////////////////////////
    // High pairs
    channel_config = dma_channel_get_default_config(LVDS_HI_DMA_CHAN);
    channel_config_set_dreq(&channel_config, pio_get_dreq(LVDS_PIO, LVDS_HI_SM, true));
    channel_config_set_chain_to(&channel_config, LVDS_HI_DMA_CB_CHAN);

    channel_config_set_high_priority(&channel_config, true);
    dma_channel_configure(LVDS_HI_DMA_CHAN,
                          &channel_config,
                          &LVDS_PIO->txf[LVDS_HI_SM],
                          NULL,
                          FULL_LINE,
                          false);

    chain_config = dma_channel_get_default_config(LVDS_HI_DMA_CB_CHAN);
    channel_config_set_read_increment(&chain_config, false);
    dma_channel_configure(LVDS_HI_DMA_CB_CHAN,
                          &chain_config,
                          &dma_channel_hw_addr(LVDS_HI_DMA_CHAN)->al3_read_addr_trig,
                          NULL,
                          1,
                          false);

    /////////////////////////////////////////////////////////////////////////////////////
    irq_set_exclusive_handler(DMA_IRQ_0, lvdsDMATrigger);
    dma_channel_set_irq0_enabled(LVDS_HI_DMA_CHAN, true);
    irq_set_enabled(DMA_IRQ_0, true);
}

static void pio_init(void)
{
    /////////////////////////////////////////////////////////////////////////////////////
    // Low pairs
    uint prog_offs = pio_add_program(LVDS_PIO, &lvds_dat_program);
    pio_sm_config c = lvds_dat_program_get_default_config(prog_offs);

    sm_config_set_sideset_pins(&c, PINBASE_DAT_LO);

    sm_config_set_out_shift(&c,
                            true, // Shift right (high takes high 14 bits, low takes lower 14)
                            true, // autopull
                            14);
    
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_set_pindirs_with_mask(LVDS_PIO, LVDS_LO_SM,
                                 (0xf << PINBASE_DAT_LO),
                                 (0xf << PINBASE_DAT_LO));

    for (uint32_t i = 0; i < 4; i++)
        pio_gpio_init(LVDS_PIO, i + PINBASE_DAT_LO);

    pio_sm_init(LVDS_PIO, LVDS_LO_SM, prog_offs, &c);
    pio_sm_set_clkdiv_int_frac(LVDS_PIO, LVDS_LO_SM, PIODIV, 0);

    /////////////////////////////////////////////////////////////////////////////////////
    // High pairs
    c = lvds_dat_program_get_default_config(prog_offs);

    sm_config_set_sideset_pins(&c, PINBASE_DAT_HI);

    sm_config_set_out_shift(&c,
                            false, // Shift right (high takes high 14 bits, low takes lower 14)
                            true,  // autopull
                            14);
    
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_set_pindirs_with_mask(LVDS_PIO, LVDS_HI_SM,
                                 (0xf << PINBASE_DAT_HI),
                                 (0xf << PINBASE_DAT_HI));

    for (uint32_t i = 0; i < 4; i++)
        pio_gpio_init(LVDS_PIO, i + PINBASE_DAT_HI);

    pio_sm_init(LVDS_PIO, LVDS_HI_SM, prog_offs, &c);
    pio_sm_set_clkdiv_int_frac(LVDS_PIO, LVDS_HI_SM, PIODIV, 0);

    /////////////////////////////////////////////////////////////////////////////////////
    // Clock pair
    prog_offs = pio_add_program(LVDS_PIO, &lvds_clk_program);
    c = lvds_clk_program_get_default_config(prog_offs);

    sm_config_set_sideset_pins(&c, PINBASE_CLK);
    pio_sm_set_pindirs_with_mask(LVDS_PIO, LVDS_CK_SM,
                                 (0x3 << PINBASE_CLK),
                                 (0x3 << PINBASE_CLK));

    for (uint32_t i = 0; i < 2; i++)
        pio_gpio_init(LVDS_PIO, i + PINBASE_CLK);

    pio_sm_init(LVDS_PIO, LVDS_CK_SM, prog_offs, &c);
    pio_sm_set_clkdiv_int_frac(LVDS_PIO, LVDS_CK_SM, PIODIV, 0);
}

static void dvi_configure_pad(uint gpio) {
	// 2 mA drive, enable slew rate limiting (this seems fine even at 720p30, and
	// the 3V3 LDO doesn't get warm like when turning all the GPIOs up to 11).
	// Also disable digital receiver.
	hw_write_masked(
		&padsbank0_hw->io[gpio],
		(0 << PADS_BANK0_GPIO0_DRIVE_LSB),
		PADS_BANK0_GPIO0_DRIVE_BITS | PADS_BANK0_GPIO0_SLEWFAST_BITS | PADS_BANK0_GPIO0_IE_BITS
	);
	// gpio_set_outover(gpio, invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
}

void lvds_loop(void)
{
    // Set systick to processor clock
    systick_hw->csr = 0x5;





    pio_init();

    for (uint32_t i = 0; i < 10; i++)
    {
        dvi_configure_pad(i+8);
    }

    dma_init();

    // Kickstart DMA
    dma_channel_hw_addr(LVDS_LO_DMA_CB_CHAN)->al3_read_addr_trig = (uintptr_t)&nextLinePtr;
    dma_channel_hw_addr(LVDS_HI_DMA_CB_CHAN)->al3_read_addr_trig = (uintptr_t)&nextLinePtr;

    // Make sure all fifos are filled
    while (!pio_sm_is_rx_fifo_full(LVDS_PIO, LVDS_LO_SM))  { }
    while (!pio_sm_is_rx_fifo_full(LVDS_PIO, LVDS_HI_SM))  { }

    // ..and GO!
    pio_enable_sm_mask_in_sync(LVDS_PIO,
                                    (1 << LVDS_LO_SM) |
                                    (1 << LVDS_HI_SM) |
                                    (1 << LVDS_CK_SM));

    while (1) { }
}
