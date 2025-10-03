#pragma once
#include "Chireiden.h"
#include "AnmLoaded.h"
#include "Timer.h"
#include "Interp.h"
#include "Chain.h"

struct Float3
{
    float x, y, z;
};

struct Float2
{
    float x, y;
};

struct Int3 
{
    int x, y, z;
};

class AnmVm;
struct AnmVmList
{
    AnmVm* entry;
    AnmVmList* next;
    AnmVmList* prev;
};

struct AnmRawInstruction
{
    uint16_t opcode;
    uint16_t offsetToNextInstr;
    short time;
    uint16_t varMask;
    int args[10];
};

struct RngContext
{
    uint16_t time;
    uint16_t padding;
    int32_t counter;
};
ASSERT_SIZE(RngContext, 0x8);

class AnmVm
{
public:
    int m_id;
    AnmVmList m_nodeInGlobalList;
    AnmVmList m_nodeAsFamilyMember;
    AnmVm* m_nextInLayerList;
    int m_layer;
    D3DXVECTOR3 m_rotation;
    D3DXVECTOR3 m_angularVelocity;
    D3DXVECTOR2 m_scale;
    D3DXVECTOR2 m_scaleGrowth;
    D3DXVECTOR2 m_spriteSize;
    D3DXVECTOR2 m_uvScrollPos;
    Timer m_timeInScript; // <0x5c>
    D3DXVECTOR2 m_spriteUvQuad[4];
    Interp<Float3> m_posInterp;
    Interp<Int3> m_rgbInterp;
    Interp<int> m_alphaInterp;
    Interp<Float3> m_rotationInterp;
    Interp<Float2> m_scaleInterp;
    Interp<Int3> m_rgb2Interp;
    Interp<int> m_alpha2Interp;
    Interp<float> m_uVelInterp;
    Interp<float> m_vVelInterp;
    D3DXVECTOR2 m_uvScrollVel;
    D3DXMATRIX m_baseScaleMatrix;
    D3DXMATRIX m_localTransformMatrix;
    D3DXMATRIX m_matrix37c;
    D3DCOLOR m_color1;
    D3DCOLOR m_color2;
    uint16_t m_pendingInterrupt;
    uint16_t m_unused;
    Timer m_interruptReturnTime;
    AnmRawInstruction* m_interruptReturnInstr;
    int m_timeOfLastSpriteSet;
    uint16_t m_spriteNumber;
    uint16_t m_anmFileIndex;
    uint16_t m_anotherSpriteNumber;
    uint16_t m_scriptNumber;
    AnmRawInstruction* m_beginningOfScript;
    AnmRawInstruction* m_currentInstruction;
    AnmLoadedSprite* m_sprite;
    AnmLoaded* m_anmLoaded;
    int m_intVars[4]; // Script variables
    float m_floatVars[4]; // Script variables
    int m_intVar8;
    int m_intVar9;
    D3DXVECTOR3 m_pos;
    D3DXVECTOR3 m_entityPos;
    D3DXVECTOR3 m_pos2;
    void* m_specialRenderData;
    uint32_t m_flagsLow;
    uint32_t m_flagsHigh;
    uint32_t m_unknown;
    ChainCallback* m_onTick; // Function pointers to chainCallback
    ChainCallback* m_onDraw; // Function pointers to chainCallback
    uint32_t m_unknown1;
    uint8_t m_fontDimensions[2];
    uint16_t m_probablyPadding;
    uint32_t m_j5;
    uint32_t m_j6;
    void* m_unknownFunc;
    void* (*m_spriteMappingFunc)(AnmVm* This, uint32_t param);
    void* m_dataForSpriteMappingFunc;

    AnmVm();
    void initialize();
    void run();
    void ApplyZRotationToQuadCorners(D3DXVECTOR3* bottomLeft, D3DXVECTOR3* bottomRight, D3DXVECTOR3* topRight, D3DXVECTOR3* topLeft);
    void writeSpriteCharacters(D3DXVECTOR3* topLeft, D3DXVECTOR3* bottomLeft, D3DXVECTOR3* topRight, D3DXVECTOR3* bottomRight);
    void writeSpriteCharactersWithoutRot(D3DXVECTOR3* bottomLeft, D3DXVECTOR3* bottomRight, D3DXVECTOR3* topRight, D3DXVECTOR3* topLeft);
    int setupTextureQuadAndMatrices(uint32_t spriteNumber, AnmLoaded* anmLoaded);
    static uint32_t rng(RngContext* data);
    int getIntVar(int id);
    int* getIntVarPtr(int* id);
    float getFloatVar(float id);
    float* getFloatVarPtr(float* id);
};
ASSERT_SIZE(AnmVm, 0x434);

/* Globals */

extern RngContext g_anmRngContext;
extern RngContext g_replayRngContext;
extern D3DXVECTOR3 g_bottomLeftDrawCorner;
extern D3DXVECTOR3 g_bottomRightDrawCorner;
extern D3DXVECTOR3 g_topLeftDrawCorner;
extern D3DXVECTOR3 g_topRightDrawCorner;
