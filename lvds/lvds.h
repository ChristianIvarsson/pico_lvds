#ifndef __LVDS_H__
#define __LVDS_H__
#ifdef __cplusplus 
extern "C" {
#endif

#include "../config.h"
#include "../panel.h"

typedef struct {
    uint32_t frame;
    uint32_t line;
    uint32_t rtime;
} lvdsData_t;

extern void lvds_loop(void);
extern void genLineData(void);
extern void reqLutChange(uint32_t *lut, uint16_t line);

extern uint32_t visLine    [ VISLINEBUFFERS ] [ FULL_LINE ];
extern uint32_t vBlankLine [ FULL_LINE ];
extern uint8_t  screenBuf  [ yRES / 2 ] [ xRES / 2 ];

#ifdef __cplusplus 
}
#endif
#endif
