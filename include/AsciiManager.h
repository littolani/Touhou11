#pragma once
#include "Chireiden.h"
#include "AnmVm.h"
#include "Chain.h"
#include "Macros.h"

struct AsciiString
{
    char text[256];
    D3DXVECTOR3 pos;
    D3DCOLOR color;
    D3DXVECTOR2 scale;
    int alignV;        //TODO: CHECK ALIGNMENT
    int alignH;        //TODO: CHECK ALIGNMENT
    int fontId;        //TODO: CHECK ALIGNMENT
    int drawShadows;   //TODO: CHECK ALIGNMENT
    int renderGroup;   //TODO: CHECK ALIGNMENT
    int remainingTime; //TODO: CHECK ALIGNMENT
};
ASSERT_SIZE(AsciiString, 0x130);

class AsciiManager
{
public:
    void* vtable; // Vtable referenced in ghidra but seems unused so far
    uint32_t flag;
    uint32_t a;
    ChainElem* onTick;
    ChainElem* draw24;
    AnmVm vm0;
    AnmVm vm1;
    AsciiString strings[320];
    int numStrings;
    D3DCOLOR color;
    D3DXVECTOR2 scale;
    int strField;
    int idk;
    int drawShadows;
    int fontId;
    int group;
    int duration;
    int alignmentModeH;
    int alignmentModeV;
    int onTickCounter;
    AnmLoaded* asciiAnm;
    int anmId;
    ChainElem* chain2b;
    
    AsciiManager();
};
ASSERT_SIZE(AsciiManager, 0x184bc);