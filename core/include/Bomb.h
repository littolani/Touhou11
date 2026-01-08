#pragma once
#include "AnmVm.h"
#include "AnmId.h"
#include "Chain.h"
#include "DistortionMesh.h"

struct Bomb
{
    int idk0;
    int idk1;
    ChainElem* tickChainElem;
    ChainElem* drawChainElem;
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

    static ChainCallbackResult onTick(Bomb* This);
    static int onTickReimuA(Bomb* This);
    static int onTickReimuB(Bomb* This);
    static int onTickReimuC(Bomb* This);
    static int onTickMarisaA(Bomb* This);
    static int onTickMarisaB(Bomb* This);
    static int onTickMarisaC(Bomb* This);
};
ASSERT_SIZE(Bomb, 0x4a0);