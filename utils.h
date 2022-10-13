#ifndef __UTILS_H__
#define __UTILS_H__
#ifdef __cplusplus 
extern "C" {
#endif

extern uint64_t __not_in_flash_func(time_us_64_ram)(void);

// lowlev.S
// Buffer must be aligned and nBytes must be in multiples of four!
extern void memclr(void *buf, uint32_t nBytes);

#ifdef __cplusplus 
}
#endif
#endif
