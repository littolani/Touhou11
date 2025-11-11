#pragma once
#include "Chireiden.h"
#include "LaserSegment.h"
#include "Timer.h"
#include "Macros.h"

struct LaserData
{
    // <0x0>: LaserData_vtable* laserDataVtable
    uint32_t laserDataVtable; // Temporary Filler
    uint32_t a; // unknown
    // <0x8> LaserLine_vftable* // laserLineVtable, probably incorrectly placed
    uint32_t laserLineVtable; // Temporary Filler
    uint32_t c; // unknown
    uint32_t u1; // unknown
    Timer timer;
    uint32_t reserved1[22]; // unknown
    LaserSegment laserSegments[17];
    uint8_t reserved2[64]; // unknown
    uint32_t finalFlag;
    uint8_t reserved3[8]; // unknown
};
ASSERT_SIZE(LaserData, 0x440);

/* Proof of sizes in asm:
424841: b9 11 00      MOV       loopCounter,0x11 ; 17 segments
...
00424852 83 c0 34      ADD       laserSegment,0x34
00424855 83 e9 01      SUB       loopCounter,0x1
*/