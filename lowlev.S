.syntax unified
.cpu cortex-m0plus
.thumb

# It's faster to just use a for loop on smaller datasets.
.global memclr          /* memclr ( *buf, nBytes ) */

.macro func_t   name
.align 2
.section .scratch_x.\name, "ax"
.type \name,%function
.thumb_func
\name:
.endm

func_t  memclr
    eors    r2, r2, r2  /* (  1  )                      */
moreBytes:
    stmia   r0!, { r2 } /* (1 + 1) Clear data           */
    subs    r1,  #4     /* (  1  ) Decrement bytes left */
    bgt moreBytes       /* (3 or 1)                     */
bx lr
