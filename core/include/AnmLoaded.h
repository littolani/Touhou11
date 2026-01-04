#pragma once
#include "Chireiden.h"
#include "Supervisor.h"
#include "Globals.h"

struct AnmHeader
{
    uint32_t version;
    uint16_t numSprites;
    uint16_t numScripts;
    uint16_t zero1;	/* Actually zero. */
    uint16_t w;
    uint16_t h;
    uint16_t formatIndex;
    uint32_t nameOffset;
    uint16_t x;
    uint16_t y;
    uint32_t memoryPriority;
    uint32_t thtxOffset;
    uint16_t hasData;
    uint16_t lowresscale;
    uint32_t nextOffset;
    uint32_t zero2[6];
};
ASSERT_SIZE(AnmHeader, 0x40);

struct AnmLoadedD3D
{
    IDirect3DTexture9* m_texture;
    void* m_srcData;
    uint32_t m_srcDataSize;
    int m_bytesPerPixel;
    int m_flags;

    /**
     * 0x4540f0
     * @brief
     * @param This ESI:4
     */
    static void createTextureFromAtR(AnmLoadedD3D* This);

    static int createTextureFromAt(AnmLoadedD3D* This, uint32_t width, uint32_t height, int formatIndex);
    static int createTextureFromThtx(AnmLoadedD3D* This, uint32_t texWidth, uint32_t texHeight, int formatIndex, void* thtxData);
    static int createTextureFromImage(AnmLoadedD3D* This, int textureWidthOffset, uint32_t width, uint32_t height, int formatIndex);
};
ASSERT_SIZE(AnmLoadedD3D, 0x14);

struct AnmLoadedSprite
{
    int anmSlot;
    int spriteNumber;
    AnmLoadedD3D* anmLoadedD3D;
    D3DXVECTOR2 startPixelInclusive;
    D3DXVECTOR2 endPixelExclusive;
    float bitmapHeight;
    float bitmapWidth;
    D3DXVECTOR2 uvStart;
    D3DXVECTOR2 uvEnd;
    float spriteHeight;
    float spriteWidth;
    D3DXVECTOR2 bitmapScale;
    uint32_t m_idk;
};
ASSERT_SIZE(AnmLoadedSprite, 0x48);

struct AnmLoaded
{
    int m_anmSlotIndex;
    char m_filePath[260];
    AnmHeader* m_header;
    int m_numAnmLoadedD3Ds;
    int m_numScripts;
    int m_numSprites;
    AnmLoadedSprite* m_keyframeData;
    uint32_t* m_spriteData;
    AnmLoadedD3D* m_anmLoadedD3D;
    int m_anmsLoading;
    int m_unknown;
    int m_texturesCreated;
    void* m_unknownHeapAllocated;

    static void setupTextures(AnmLoaded* This);
    static int createTextureForEntry(AnmLoaded* This, int i, int numSprites, int numScripts, AnmHeader* anmHeader);
};
ASSERT_SIZE(AnmLoaded, 0x134);

struct SpriteData
{
    float x, y, w, h;
};
