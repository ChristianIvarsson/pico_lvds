#include "../panel.h"

.syntax unified
.cpu cortex-m0plus
.thumb

.global drawLineASM     /* drawLineASM    ( *src, *dst, srcCount ) */
.global drawLineASM2x   /* drawLineASM2x  ( *src, *dst, srcCount ) */

.macro func_t   name
.align 2
# .section .time_critical.\name, "ax"
.section .scratch_x.\name, "ax"
.type \name,%function
.thumb_func
\name:
.endm

# # # 1024 + H-blank at 48 Hz panel refresh
# 7168 cycles / visible line  (7.0  cycles / pixel)   (28.0000 ticks for 4)  (28.444.. microsec)
# 8400 cycles / total line    (8.2~ cycles / pixel)   (32.8125 ticks for 4)  (33.333.. microsec)

# 10 ticks / 1 panel pixels
.macro drawPixel    srcOffset    srcReg  dstReg  lutReg    scrtch1  scrtch2  scrtch3
    ldrb    \scrtch1 , [ \srcReg, #\srcOffset ]  /* (1 + 1) Load pixel                                */
    lsls    \scrtch1 , \scrtch1, #3              /* (  1  ) Times 8 since it's to be used as an index */
    add     \scrtch1 , \scrtch1, \lutReg         /* (  1  ) Calculate offset in LUT                   */
    ldmia   \scrtch1!, { \scrtch2, \scrtch3 }    /* (1 + 2) Load data.    ___REG IS TRASHED!___       */
    stmia   \dstReg! , { \scrtch2, \scrtch3 }    /* (1 + 2) Store data                                */
.endm


func_t  drawLineASM

    push    { r4 - r6 } /* (1 + 3) */

# r0: Source pointer
# r1: Destination pointer
# r2: Pixel count
# r3: LUT
    ldr     r3, =pixLUT /* (1 + 1) */

morePixels:

    drawPixel   0x00,   r0, r1, r3,   r4, r5, r6   /* (10) */
    drawPixel   0x01,   r0, r1, r3,   r4, r5, r6   /* (10) */
    drawPixel   0x02,   r0, r1, r3,   r4, r5, r6   /* (10) */
    drawPixel   0x03,   r0, r1, r3,   r4, r5, r6   /* (10) */

    adds    r0,  #4     /* (1) Next four pixels      */
    subs    r2,  #4     /* (1) Decrement pixels left */
    bne morePixels      /* (3 or 1)                  */

    pop     { r4 - r6 } /* (1 + 3)                   */
bx lr                   /* (3)                       */




# 13 ticks / 2 panel pixels
.macro drawDoublePixel    srcOffset    srcReg  dstReg  lutReg    scrtch1  scrtch2  scrtch3
    ldrb    \scrtch1 , [ \srcReg, #\srcOffset ]  /* (1 + 1) Load pixel                                */
    lsls    \scrtch1 , \scrtch1, #3              /* (  1  ) Times 8 since it's to be used as an index */
    add     \scrtch1 , \scrtch1, \lutReg         /* (  1  ) Calculate offset in LUT                   */
    ldmia   \scrtch1!, { \scrtch2, \scrtch3 }    /* (1 + 2) Load data.    ___REG IS TRASHED!___       */
    stmia   \dstReg! , { \scrtch2, \scrtch3 }    /* (1 + 2) Store data                                */
    stmia   \dstReg! , { \scrtch2, \scrtch3 }    /* (1 + 2) Store data                                */
.endm

func_t  drawLineASM2x

    push    { r4 - r6 } /* (1 + 3) */

# r0: Source pointer
# r1: Destination pointer
# r2: Pixel count
# r3: LUT
    ldr     r3, =pixLUT /* (1 + 1) */

morePixels2x:

    drawDoublePixel   0x00,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x01,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x02,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x03,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x04,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x05,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x06,   r0, r1, r3,   r4, r5, r6   /* (13) */
    drawDoublePixel   0x07,   r0, r1, r3,   r4, r5, r6   /* (13) */

    adds    r0,  #8     /* (1) Next eight pixels     */
    subs    r2,  #8     /* (1) Decrement pixels left */
    bne morePixels2x    /* (3 or 1)                  */

    pop     { r4 - r6 } /* (1 + 3)                   */
bx lr                   /* (3)                       */
