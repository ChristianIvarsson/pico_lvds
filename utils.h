#ifndef __UTILS_H__
#define __UTILS_H__

typedef struct {
    const uint32_t address;
    const char *text;
} locVerif_t;

extern uint64_t __not_in_flash_func(time_us_64_ram)(void);

#endif
