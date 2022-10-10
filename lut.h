#ifndef __LUT_H__
#define __LUT_H__

// Wasting 256 bytes to prevent having to calculate in multiples of 7. (almost double performance)
#define LUTMULTI (8)

// lut.c
extern uint8_t pixLUT[256 * LUTMULTI];
extern const uint8_t defaultPalette[256 * 3];

extern void genPalette_3x8(const uint8_t *p);
extern void genPalette_32(const uint32_t *p);
extern void drawLine(const uint8_t *src, uint32_t *dst, uint32_t srcCount);


// lut_low.S
// Count must be in multiples of 4!
extern void drawLineASM(const uint8_t *src, uint32_t *dst, uint32_t srcCount);
// Count must be in multiples of 8!
extern void drawLineASM2x(const uint8_t *src, uint32_t *dst, uint32_t srcCount);

#endif
