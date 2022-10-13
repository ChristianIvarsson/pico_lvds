#ifndef __CONFIG_H__
#define __CONFIG_H__
#ifdef __cplusplus 
extern "C" {
#endif

#define CPUFREQ (252UL * 1000000UL)

// There's only one way to do it, pedal to the metal
#define PIODIV  (1)

// Number of line buffers
// Hardcoded atm so don't change
#define VISLINEBUFFERS (2)


// 2 pins for clock. Starting from
#define PINBASE_CLK    (8)

// 4 pins for data low (Pair 0/1). Starting from
#define PINBASE_DAT_LO (10)

// 4 pins for data high (Pair 2/3). Starting from
#define PINBASE_DAT_HI (14)

#define LVDS_PIO   (pio1)
#define LVDS_LO_SM (0)
#define LVDS_HI_SM (1)
#define LVDS_CK_SM (2)

#define LVDS_LO_DMA_CHAN    (0)
#define LVDS_LO_DMA_CB_CHAN (1)
#define LVDS_HI_DMA_CHAN    (2)
#define LVDS_HI_DMA_CB_CHAN (3)

#ifdef __cplusplus 
}
#endif
#endif
