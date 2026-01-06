#pragma once
#include "AnmVm.h"
#include "AnmId.h"
#include "Chain.h"
#include "DistortionMesh.h"

struct Bomb
{
    int idk0;
    int idk1;
    ChainElem* onTick;
    ChainElem* onDraw;
    int idk2;
    Timer timer0;
    Timer timer1;
    BOOL isUsingBomb;
    AnmId vmId;
    int playerAnmSlotIndex;
    AnmVm vm;
    void* unknownHeapAllocated;
    DistortionMesh* distortionMesh;
    D3DXVECTOR3 playerPos;
    D3DXVECTOR2 someVec2;
    uint32_t someIndicator;
    int idk3;
};
ASSERT_SIZE(Bomb, 0x4a0);