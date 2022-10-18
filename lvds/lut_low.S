#include "../panel.h"

.syntax unified
.cpu cortex-m0plus
.thumb

.global drawLineASM       /* drawLineASM       ( *src, *dst, srcCount ) */
.global drawLineASM2x     /* drawLineASM2x     ( *src, *dst, srcCount ) */
.global draw565LineASM2x  /* draw565LineASM2x  ( *src, *dst, srcCount ) */


.macro func_t   name
.align 2
.section .scratch_x.\name, "ax"
.type \name,%function
.thumb_func
\name:
.endm

# # # 1024 + H-blank at 48 Hz panel refresh
# 7168 cycles / visible line  (7.0  cycles / pixel)   (28.0000 ticks for 4)  (28.444.. microsec)
# 8400 cycles / total line    (8.2~ cycles / pixel)   (32.8125 ticks for 4)  (33.333.. microsec)

# 7 ticks / panel pixel
.macro drawPixel    srcOffset    srcReg  dstReg  lutReg    scrtch
    ldrb    \scrtch , [ \srcReg, #\srcOffset ] /* (1 + 1) Load pixel                                */
    lsls    \scrtch , \scrtch, #2              /* (  1  ) Times 4 since it's to be used as an index */
    ldr     \scrtch , [ \lutReg, \scrtch ]     /* (1 + 1) Load data.                                */
    stmia   \dstReg , { \scrtch }              /* (1 + 1) Store data                                */
.endm

func_t  drawLineASM

    push    { r4 } /* (1 + 1) */

# r0: Source pointer
# r1: Destination pointer
# r2: Pixel count
# r3: LUT
    ldr     r3, =currentLUT /* (1 + 1) */
    ldr     r3, [ r3 ]      /* (1 + 1) */ 

morePixels:

    drawPixel   0x00,   r0, r1, r3,   r4   /* (7) */
    drawPixel   0x01,   r0, r1, r3,   r4   /* (7) */
    drawPixel   0x02,   r0, r1, r3,   r4   /* (7) */
    drawPixel   0x03,   r0, r1, r3,   r4   /* (7) */

    adds    r0,  #4     /* (1) Next four pixels      */
    subs    r2,  #4     /* (1) Decrement pixels left */
    bne morePixels      /* (3 or 1)                  */

    pop     { r4 }      /* (1 + 1)                   */
bx lr                   /* (3)                       */

# 9 ticks / 2 panel pixels
.macro drawDoublePixel    srcOffset    srcReg  dstReg  lutReg    scrtch
    ldrb    \scrtch , [ \srcReg, #\srcOffset ] /* (1 + 1) Load pixel                                */
    lsls    \scrtch , \scrtch, #2              /* (  1  ) Times 4 since it's to be used as an index */
    ldr     \scrtch , [ \lutReg, \scrtch ]     /* (1 + 1) Load data.                                */
    stmia   \dstReg! , { \scrtch }             /* (1 + 1) Store data                                */
    stmia   \dstReg! , { \scrtch }             /* (1 + 1) Store data                                */
.endm

func_t  drawLineASM2x

    push    { r4 } /* (1 + 1) */

# r0: Source pointer
# r1: Destination pointer
# r2: Pixel count
# r3: LUT
    ldr     r3, =currentLUT /* (1 + 1) */
    ldr     r3, [ r3 ]      /* (1 + 1) */ 

morePixels2x:

    drawDoublePixel   0x00,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x01,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x02,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x03,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x04,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x05,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x06,   r0, r1, r3,   r4   /* (9) */
    drawDoublePixel   0x07,   r0, r1, r3,   r4   /* (9) */

    adds    r0,  #8     /* (1) Next eight pixels     */
    subs    r2,  #8     /* (1) Decrement pixels left */
    bne morePixels2x    /* (3 or 1)                  */

    pop     { r4 }      /* (1 + 1)                   */
bx lr                   /* (3)                       */

# 15 ticks / 2 panel pixels
.macro draw565DoublePixel    srcOffset    srcReg  dstReg  lutRegEven lutRegOdd      scrtch1  scrtch2
    # First half
    ldrb    \scrtch1 , [ \srcReg, #\srcOffset + 0]  /* (1 + 1) Load pixel                                */
    lsls    \scrtch1 , \scrtch1, #2                 /* (  1  ) Times 4 since it's to be used as an index */
    ldr     \scrtch1 , [ \lutRegEven, \scrtch1 ]    /* (1 + 1) Load data.                                */
    # Second half
    ldrb    \scrtch2 , [ \srcReg, #\srcOffset + 1]  /* (1 + 1) Load pixel                                */
    lsls    \scrtch2 , \scrtch2, #2                 /* (  1  ) Times 4 since it's to be used as an index */
    ldr     \scrtch2 , [ \lutRegOdd, \scrtch2 ]    /* (1 + 1) Load data.                                */
    orrs    \scrtch1 , \scrtch1 , \scrtch2          /* (  1  ) OR both results                           */
    stmia   \dstReg! , { \scrtch1 }                 /* (1 + 1) Store data                                */
    stmia   \dstReg! , { \scrtch1 }                 /* (1 + 1) Store data                                */
.endm

func_t  draw565LineASM2x

    push    { r4 - r6 } /* (1 + 3) */

# r0: Source pointer
# r1: Destination pointer
# r2: Pixel count
# r3: LUT
    lsls    r2, r2, #1       /* (  1  ) N pixels to N bytes */
    ldr     r3, =lut565_even /* (1 + 1) */
    ldr     r4, =lut565_odd  /* (1 + 1) */

more565Pixels2x:

    draw565DoublePixel   0x00,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x02,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x04,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x06,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x08,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x0a,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x0c,   r0, r1, r3, r4,   r5, r6   /* (15) */
    draw565DoublePixel   0x0e,   r0, r1, r3, r4,   r5, r6   /* (15) */

    adds    r0,  #16    /* (1) Next eight pixels     */
    subs    r2,  #16    /* (1) Decrement pixels left */
    bne more565Pixels2x /* (3 or 1)                  */

    pop     { r4 - r6 } /* (1 + 3)                   */
bx lr                   /* (3)                       */
