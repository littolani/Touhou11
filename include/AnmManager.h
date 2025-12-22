#pragma once

#include "Chireiden.h"
#include "AnmVm.h"
#include "FileAbstrction.h"

struct AnmLoaded;
struct AnmLoadedD3D;

struct BlitParams
{
    int anmLoadedIndex;
    int anmLoadedD3dIndex;
    RECT srcRect;
    RECT destRect;
};
ASSERT_SIZE(BlitParams, 0x28);

struct AnmVertexBuffers
{
    int leftoverSpriteCount;
    RenderVertex144 spriteVertexData[0x20000];
    RenderVertex144* spriteWriteCursor;
    RenderVertex144* spriteRenderCursor;
};
ASSERT_SIZE(AnmVertexBuffers, 0x38000c);

class AnmManager
{
public:
    BlitParams m_blitParamsArray[4];            // <0x0>
    int m_allocatedVmCountMaybe;                // <0xa0>
    int m_idk;                                  // <0xa4>
    int m_someCounter;                          // <0xa8>
    int m_refreshCounter;                       // <0xac>
    int m_globalRenderQuadOffsetX;              // <0xb0>
    int m_globalRenderQuadOffsetY;              // <0xb4>
    int m_unk3;                                 // <0xb8>
    AnmVm m_bulkVms[4096];                      // <0xbc>
    uint8_t m_bulkVmsIsAlive[4096];             // <0x4340bc>
    int m_nextBulkVmIndex;                      // <0x4350bc>
    AnmLoaded* m_loadedAnms[32];                // <0x4350c0>
    D3DXMATRIX m_currentWorldMatrix;            // <0x435140>
    AnmVm m_primaryVm;                          // <0x435180>
    uint32_t m_u0;                              // <0x4355bc>
    D3DCOLOR m_color;                           // <0x4355b8>
    AnmLoadedD3D* m_tex;                        // <0x4355bc>
    uint8_t m_renderStateMode;                  // <0x4355c0>
    uint8_t m_l;                                // <0x4355c1>
    uint8_t m_haveFlushedSprites;               // <0x4355c2>
    uint8_t m_stuff[3];
    bool m_usePointFilter;                      // <0x4355c6>
    uint8_t m_idk2;                             // <0x4355c7>
    AnmLoadedSprite* m_cachedSprite;            // <0x4355c8>
    IDirect3DVertexBuffer9* m_d3dVertexBuffer;  // <0x4355cc>
    D3DXVECTOR3 m_primitive0Position;           // <0x4355d0>
    D3DXVECTOR2 m_primitive0Uv;                 // <0x4355dc>
    D3DXVECTOR3 m_primitive1Position;           // <0x4355e4>
    D3DXVECTOR2 m_primitive1Uv;                 // <0x4355f0>
    D3DXVECTOR3 m_primitive2Position;           // <0x4355f8>
    D3DXVECTOR2 m_primitive2Uv;                 // <0x435604>
    D3DXVECTOR3 m_primitive3Position;           // <0x43560c>
    D3DXVECTOR2 m_primitive3Uv;                 // <0x435618>
    AnmVertexBuffers m_anmVertexBuffers;        // <0x435620>
    AnmVmList* m_primaryGlobalNext;             // <0x7b562c>
    AnmVmList* m_primaryGlobalPrev;             // <0x7b5630>
    AnmVmList* m_secondaryGlobalNext;           // <0x7b5634>
    AnmVmList* m_secondaryGlobalPrev;           // <0x7b5638>
    AnmVm m_vmLayers[31];                       // <0x7b563c>
    int m_id;                                   // <0x7bd888>
    uint8_t m_scaleB;                           // <0x7bd88c>
    uint8_t m_scaleG;                           // <0x7bd88d>
    uint8_t m_scaleR;                           // <0x7bd88e>
    uint8_t m_scaleA;                           // <0x7bd88f>
    uint32_t m_rebuildColorFlag;                // <0x7bd890>

    static void createD3DTextures(AnmManager* This);
    static void markAnmLoadedAsReleasedInVmList(AnmManager* This, AnmLoaded* anmLoaded);
    static void releaseTextures(AnmManager* This);
    static void flushSprites(AnmManager* This);
    static void blitTextureToSurface(AnmManager* This, BlitParams* blitParams);
    static void setupRenderStateForVm(AnmManager* This, AnmVm* vm);
    static void applyRenderStateForVm(AnmManager* This, AnmVm* vm);
    static void drawVmSprite2D(AnmManager* This, uint32_t layer, AnmVm* anmVm);
    static void drawVm(AnmManager* This, AnmVm* vm);
    static void transformAndDraw(AnmManager* This, AnmVm* vm);
    static void loadIntoAnmVm(AnmVm* vm, AnmLoaded* anmLoaded, int scriptNumber);
    static void putInVmList(AnmManager* This, AnmVm* vm, AnmId* anmId);
    static void releaseAnmLoaded(AnmManager* This, AnmLoaded* anmLoaded);
    static int drawVmWithTextureTransform(AnmManager* This, AnmVm* vm);
    static int updateWorldMatrixAndProjectQuadCorners(AnmManager* This, AnmVm* vm);
    static int writeSprite(AnmManager* This, RenderVertex144* vertexBuffer);
    static int drawVmWithFog(AnmManager* This, AnmVm* vm);
    static int drawVmTriangleStrip(AnmManager* This, AnmVm* vm, SpecialRenderData* specialRenderData, uint32_t vertexCount);
    static int drawVmTriangleFan(AnmManager* This, AnmVm* vm, SpecialRenderData* specialRenderData, uint32_t vertexCount);
    static AnmLoaded* preloadAnm(AnmManager* This, int anmIdx, const char* anmFileName);
    static AnmLoaded* preloadAnmFromMemory(AnmManager* This, const char* anmFilePath, int m_anmSlotIndex);
    static AnmVm* allocateVm(AnmManager* This);
    static AnmVm* getVmWithId(AnmManager* This, int anmId);
    static void blitTextures(AnmManager* This);
private:
    static constexpr int NUM_BULK_VMS = 4096;
    static constexpr int NUM_ANM_LOADEDS = 32;
    static uint32_t modulateColorComponent(uint16_t base, uint16_t factor);

    static inline int getNextBulkVmIndex(int current)
    {
        return (current + 1) & (NUM_BULK_VMS - 1);
    }

    static inline uint8_t scaleChannel(uint32_t scale, uint8_t v)
    {
        uint32_t x = (scale * v) >> 7;
        x = std::min(x, uint32_t{ 255 });
        return static_cast<uint8_t>(x);
    }
};
ASSERT_SIZE(AnmManager, 0x7bd894);
