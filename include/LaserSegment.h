#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct LaserSegment
{
    uint32_t flags;
    uint8_t reserved[48];
};
ASSERT_SIZE(LaserSegment, 0x34);

/* Proof of sizes in asm:
424841: b9 11 00      MOV       loopCounter,0x11
...
00424852 83 c0 34      ADD       laserSegment,0x34 ; size
00424855 83 e9 01      SUB       loopCounter,0x1
*/