#ifndef __PANEL_H__
#define __PANEL_H__
#ifdef __cplusplus 
extern "C" {
#endif

/*
 8 - ck-
 9 - ck+
10 - d0-
11 - d0+
12 - d1-
13 - d1+
14 - d2-
15 - d2+
16 - d3-
17 - d3+

18 - sda
19 - scl
20 - bl pwm
*/

/*
de: Data Enable
vs: Vertical Sync
hs: Horizontal Sync

       _____________                      _____________
clk:                |____________________|
dat 0: < g0 > < r5 > < r4 > < r3 > < r2 > < r1 > < r0 >
dat 1: < b1 > < b0 > < g5 > < g4 > < g3 > < g2 > < g1 >
dat 2: < de > < vs > < hs > < b5 > < b4 > < b3 > < b2 >
dat 3: < -- > < b7 > < b6 > < g7 > < g6 > < r7 > < r6 >

    d3     d2     d1     d0
  < -- > < de > < b1 > < g0 >
  < b7 > < vs > < b0 > < r5 >
  < b6 > < hs > < g5 > < r4 >
  < g7 > < b5 > < g4 > < r3 >
  < g6 > < b4 > < g3 > < r2 >
  < r7 > < b3 > < g2 > < r1 >
  < r6 > < b2 > < g1 > < r0 >


Ratio 7:4
Each pixel transaction is 7 bytes, best you can easily do is 4 pixels spread over 7 32-bit pio transactions

< 3 > < 2 > < 1 > < 0 >    (1 > 2)
< 0 > < 6 > < 5 > < 4 >

< 4 > < 3 > < 2 > < 1 >    (2 > 3)
< 1 > < 0 > < 6 > < 5 >

< 5 > < 4 > < 3 > < 2 >    (3 > 4)
< 2 > < 1 > < 0 > < 6 >

< 6 > < 5 > < 4 > < 3 >    (4)
*/

// Each LCD data bit occupies two PIO bits
//  <+3  3-> <+2  2-> <+1  1-> <+0  0->
//     b3       b2       b1       b0
//  <  --  > <  de  > <  b1  > <  g0  >    Byte 0
//  <  b7  > <  vs  > <  b0  > <  r5  >    Byte 1
//  <  b6  > <  hs  > <  g5  > <  r4  >    Byte 2
//  <  g7  > <  b5  > <  g4  > <  r3  >    Byte 3
//  <  g6  > <  b4  > <  g3  > <  r2  >    Byte 4
//  <  r7  > <  b3  > <  g2  > <  r1  >    Byte 5
//  <  r6  > <  b2  > <  g1  > <  r0  >    Byte 6


// Line pixel counts must be in multiples of four!!
#define xRES    (1024)
#define yRES    ( 600)
#define hBLANK  ( 176) // 136 (176) 216
#define vBLANK  (  25) //  12 ( 25)  38

#define FULL_LINE (xRES + hBLANK)

#ifdef __cplusplus 
}
#endif
#endif
