#pragma once
#include "Chireiden.h"
#include "AnmManager.h"
#include "Chain.h"
#include "Macros.h"

struct AsciiString
{
    char text[256];
    D3DXVECTOR3 pos;
    D3DCOLOR color;
    D3DXVECTOR2 scale;
    int alignV;
    int alignH;
    int fontId;
    int drawShadows;
    int renderGroup;
    int remainingTime;
};
ASSERT_SIZE(AsciiString, 0x130);

class AsciiManager
{
public:
    static constexpr size_t STRING_ARRAY_SIZE = 320;

    void* vtable;
    uint32_t flag;
    uint32_t a;
    ChainElem* onTick;
    ChainElem* onDraw;
    AnmVm vm0;
    AnmVm vm1;
    AsciiString m_strings[STRING_ARRAY_SIZE];
    int m_numStrings;
    D3DCOLOR m_color;
    D3DXVECTOR2 scale;
    int alignH;
    int idk;
    int drawShadows;
    int fontId;
    int renderGroup;
    int duration;
    int alignmentModeH;
    int alignmentModeV;
    AnmLoaded* asciiAnm;
    int onTickCounter;
    int anmId;
    ChainElem* chain2b;
    
    //AsciiManager();
    void loadAsciiStrings(const char* str, D3DXVECTOR3* position);
    void spawnAnm(AnmLoaded* anmLoaded, AnmId* outAnmId, int scriptNumber, int vmLayer, D3DXVECTOR3* spawnPosition);
    void spawnUnknownEffect(float x, float y);
};
ASSERT_SIZE(AsciiManager, 0x184bc);
