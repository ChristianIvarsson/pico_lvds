#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CPUFREQ (252UL * 1000000UL)

// There's only one way to do it, pedal to the metal
#define PIODIV  (1)

// Number of line buffers
// Hardcoded atm so don't change
#define VISLINEBUFFERS (2)


// 2 pins for clock. Starting from
#define PINBASE_CLK  (8)

// 8 pins for data. Starting from
#define PINBASE_DAT  (10)




#define LVDS_PIO          (pio1)
#define LVDS_PIO_SM       (0)

#define LVDS_DMA_CHAN     (0)
#define LVDS_DMA_CB_CHAN  (1)



#endif
