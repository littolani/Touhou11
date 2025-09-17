#pragma once
#include "Chireiden.h"
#include "Macros.h"
#include "Chain.h"
#include "LaserData.h"
#include "LaserSegment.h"

class LaserManager
{
public:
    uint32_t unk[2];
    ChainElem* chainElem0;
    ChainElem* chainElem1;
    LaserData laserData;
    void* vfunc; // need to verify
    int someCountdown;
    int someLimit;
    uint32_t unk1[8];
};
ASSERT_SIZE(LaserManager, 0x47c);