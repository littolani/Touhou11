#pragma once
#include "Chireiden.h"
#include "AnmVm.h"
#include "Macros.h"
#include "LaserData.h"

struct LaserLine// : public LaserData
{
    LaserData laserData;
    uint8_t someData[484]; // <0x440>
    AnmVm vm1; // <0x624>
    AnmVm vm2; // <0xa58>
};
ASSERT_SIZE(LaserLine, 0xe8c);