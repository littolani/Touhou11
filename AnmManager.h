#pragma once

#include "AnmVm.h"
#include "AnmLoaded.h"
#include "Chireiden.h"
#include "Chain.h"
#include "FileAbstrction.h"
#include "Macros.h"

struct RenderVertex144
{
    D3DXVECTOR4 transformedPos;
    D3DCOLOR diffuseColor;
    D3DXVECTOR2 textureUv;
};
ASSERT_SIZE(RenderVertex144, 0x1c);

struct AnmVertexBuffers
{
    int leftoverSpriteCount;
    RenderVertex144 spriteVertexData[0x20000];
    RenderVertex144* spriteWriteCursor;
    RenderVertex144* spriteRenderCursor;
};
ASSERT_SIZE(AnmVertexBuffers, 0x38000c);

struct BlitParams
{
    int anmLoadedIndex;
    int anmLoadedD3dIndex;
    RECT srcRect;
    RECT destRect;
};
ASSERT_SIZE(BlitParams, 0x28);

class AnmManager
{
public:
    BlitParams blitParamsArray[4];         // <0x0>
    int allocatedVmCountMaybe;             // <0xa0>
    int idk;                               // <0xa4>
    int someCounter;                       // <0xa8>
    int refreshCounter;                    // <0xac>
    int globalRenderQuadOffsetX;           // <0xb0>
    int globalRenderQuadOffsetY;           // <0xb4>
    int unk3;                              // <0xb8>
    AnmVm bulkVms[4096];                   // <0xbc>
    uint8_t bulkVmsIsAlive[4096];          // <0x4340bc>
    int nextBulkVmIndex;                   // <0x4350bc>
    AnmLoaded* loadedAnms[32];             // <0x4350c0>
    D3DXMATRIX m_currentWorldMatrix;       // <0x435140>
    AnmVm primaryVm;                       // <0x435180>
    uint32_t u0;                           // <0x4355bc>
    uint32_t color;                        // <0x4355b8>
    IDirect3DTexture9** m_tex;             // <0x4355bc>
    uint8_t renderStateMode;               // <0x4355c0>
    uint8_t l;                             // <0x4355c1>
    uint8_t m_haveFlushedSprites;          // <0x4355c2>
    uint8_t stuff[3];
    uint8_t usePointFilter;                // <0x4355c7>
    uint8_t stuff2[5];
    IDirect3DVertexBuffer9* vertexBuffer;  // <0x4355cc>
    D3DXVECTOR3 primitive0Position;        // <0x4355d0>
    D3DXVECTOR2 primitive0Uv;              // <0x4355dc>
    D3DXVECTOR3 primitive1Position;        // <0x4355e4>
    D3DXVECTOR2 primitive1Uv;              // <0x4355f0>
    D3DXVECTOR3 primitive2Position;        // <0x4355f8>
    D3DXVECTOR2 primitive2Uv;              // <0x435604>
    D3DXVECTOR3 primitive3Position;        // <0x43560c>
    D3DXVECTOR2 primitive3Uv;              // <0x435618>
    AnmVertexBuffers m_vertexBuffers;      // <0x435620>
    AnmVmList* primaryGlobalNext;          // <0x7b562c>
    AnmVmList* primaryGlobalPrev;          // <0x7b5630>
    AnmVmList* secondaryGlobalNext;        // <0x7b5634>
    AnmVmList* secondaryGlobalPrev;        // <0x7b5638>
    AnmVm vmLayers[31];                    // <0x7b563c>
    int id;                                // <0x7bd888>
    uint8_t scaleB;                        // <0x7bd88c>
    uint8_t scaleG;                        // <0x7bd88d>
    uint8_t scaleR;                        // <0x7bd88e>
    uint8_t scaleA;                        // <0x7bd88f>
    uint32_t m_rebuildColorFlag;           // <0x7bd890>

    void createD3DTextures();
    void markAnmLoadedAsReleasedInVmList(AnmLoaded* anmLoaded);
    void releaseTextures();
    void flushSprites();
    void blitTextureToSurface(BlitParams* blitParams);
    void setupRenderStateForVm(AnmVm* vm);
    void applyRenderStateForVm(AnmVm* vm);
    void drawPrimitiveImmediate(AnmVm* vm, void* specialRenderData, uint32_t vertexCount);
    int updateWorldMatrixAndProjectQuadCorners(AnmVm* vm);
    int writeSprite(RenderVertex144* someVertices);
    void drawVmSprite2D(uint32_t layer, AnmVm* anmVm);
    int drawVmWithFog(AnmVm* vm);
    AnmLoaded* preloadAnm(int anmIdx, const char* anmFileName);
    AnmLoaded* preloadAnmFromMemory(const char* anmFilePath, int anmSlotIndex);
    AnmVm* allocateVm();
private:
    static constexpr int NUM_BULK_VMS = 4096;
    static constexpr int NUM_ANM_LOADEDS = 32;
    static uint32_t modulateColorComponent(uint16_t base, uint16_t factor);
    inline int getNextBulkVmIndex(int current) { return (current + 1) & (NUM_BULK_VMS - 1); }
};
ASSERT_SIZE(AnmManager, 0x7bd894);

/* Globals */

extern AnmManager* g_anmManager;

void blitTextures();
