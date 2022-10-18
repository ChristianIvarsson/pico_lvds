#ifndef __LUT_H__
#define __LUT_H__
#ifdef __cplusplus 
extern "C" {
#endif

// lut.c
extern uint32_t pixLUT[256];
extern uint32_t *currentLUT;
extern const uint8_t defaultPalette[256 * 3];
extern const uint8_t rgb332Palette[256 * 3];

extern void genPalette_3x8(const uint8_t *p, uint32_t *lut);
extern void genPalette_32(const uint32_t *p, uint32_t *lut);
extern void gen565lut(void);
extern void drawLine(const uint8_t *src, uint32_t *dst, uint32_t srcCount);

// lut_low.S
// Count must be in multiples of 4!
extern void drawLineASM(const uint8_t *src, uint32_t *dst, uint32_t srcCount);
// Count must be in multiples of 8!
extern void drawLineASM2x(const uint8_t *src, uint32_t *dst, uint32_t srcCount);
extern void draw565LineASM2x(const uint16_t *src, uint32_t *dst, uint32_t srcCount);

#ifdef __cplusplus 
}
#endif
#endif
