#pragma once

#include <stdint.h>

// Basic Defines
#define REGISTER(ADDR) (*((volatile uint32_t*)(ADDR)))
#define PACKED __attribute__((__packed__))
#define ALIGNED __attribute__((aligned(4)))

// Masking Helpers
#define BIT_0  0x00000001
#define BIT_1  0x00000002
#define BIT_2  0x00000004
#define BIT_3  0x00000008
#define BIT_4  0x00000010
#define BIT_5  0x00000020
#define BIT_6  0x00000040
#define BIT_7  0x00000080
#define BIT_8  0x00000100
#define BIT_9  0x00000200
#define BIT_10 0x00000400
#define BIT_11 0x00000800
#define BIT_12 0x00001000
#define BIT_13 0x00002000
#define BIT_14 0x00004000
#define BIT_15 0x00008000
#define BIT_16 0x00010000
#define BIT_17 0x00020000
#define BIT_18 0x00040000
#define BIT_19 0x00080000
#define BIT_20 0x00100000
#define BIT_21 0x00200000
#define BIT_22 0x00400000
#define BIT_23 0x00800000
#define BIT_24 0x01000000
#define BIT_25 0x02000000
#define BIT_26 0x04000000
#define BIT_27 0x08000000
#define BIT_28 0x10000000
#define BIT_29 0x20000000
#define BIT_30 0x40000000
#define BIT_31 0x80000000

// Bit Postion Shifts
#define BIT0_POS   0
#define BIT1_POS   1
#define BIT2_POS   2
#define BIT3_POS   3
#define BIT4_POS   4
#define BIT5_POS   5
#define BIT6_POS   6
#define BIT7_POS   7
#define BIT8_POS   8
#define BIT9_POS   9
#define BIT10_POS  10
#define BIT11_POS  11
#define BIT12_POS  12
#define BIT13_POS  13
#define BIT14_POS  14
#define BIT15_POS  15
#define BIT16_POS  16
#define BIT17_POS  17
#define BIT18_POS  18
#define BIT19_POS  19
#define BIT20_POS  20
#define BIT21_POS  21
#define BIT22_POS  22
#define BIT23_POS  23
#define BIT24_POS  24
#define BIT25_POS  25
#define BIT26_POS  26
#define BIT27_POS  27
#define BIT28_POS  28
#define BIT29_POS  29
#define BIT30_POS  30
#define BIT31_POS  31
