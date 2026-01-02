#include "AnmVm.h"
#include "Globals.h"
#include "AnmLoaded.h"
#include <iomanip>

// 0x402270
AnmVm::AnmVm()
{
    m_spriteNumber = -1;
}

// 0x402170
AnmVm::~AnmVm()
{
    if (m_specialRenderData)
        free(m_specialRenderData);
    m_specialRenderData = nullptr;
}

// 0x401fd0
void AnmVm::initialize(AnmVm* This)
{
    D3DXVECTOR3 entityPos = This->m_entityPos;
    int layer = This->m_layer;
    memset(This,0,0x434);
    This->m_scale.x = 1.0;
    This->m_scale.y = 1.0;
    This->m_entityPos = entityPos;
    This->m_layer = layer;
    This->m_color0 = 0xffffffff;
    D3DXMatrixIdentity(&This->m_baseScaleMatrix);
    This->m_flagsLow = 7;
    This->m_timeInScript.m_current = 0;
    This->m_timeInScript.m_currentF = 0.0;
    This->m_timeInScript.m_previous = -999999;
    This->m_posInterp.m_endTime = 0;
    This->m_rgbInterp.m_endTime = 0;
    This->m_alphaInterp.m_endTime = 0;
    This->m_rotationInterp.m_endTime = 0;
    This->m_scaleInterp.m_endTime = 0;
    This->m_rgb2Interp.m_endTime = 0;
    This->m_alpha2Interp.m_endTime = 0;
    This->m_uVelInterp.m_endTime = 0;
    This->m_vVelInterp.m_endTime = 0;
    This->m_nodeInGlobalList.next = nullptr;
    This->m_nodeInGlobalList.prev = nullptr;
    This->m_nodeInGlobalList.entry = This;
    This->m_nodeAsFamilyMember.next = nullptr;
    This->m_nodeAsFamilyMember.prev = nullptr;
    This->m_nodeAsFamilyMember.entry = This;
}

int AnmVm::setupTextureQuadAndMatrices(AnmVm* This, uint32_t spriteNumber, AnmLoaded* anmLoaded)
{
    if (anmLoaded->m_header == nullptr || anmLoaded->m_anmsLoading != 0)
        return -1;

    This->m_spriteNumber = static_cast<uint16_t>(spriteNumber);
    This->m_anmLoaded = anmLoaded;

    AnmLoadedSprite* sprite = &anmLoaded->m_keyframeData[spriteNumber];
    This->m_sprite = sprite;

    This->m_spriteUvQuad[0].x = sprite->uvStart.x;
    This->m_spriteUvQuad[0].y = sprite->uvStart.y;
    This->m_spriteUvQuad[1].x = sprite->uvEnd.x;
    This->m_spriteUvQuad[1].y = sprite->uvStart.y;
    This->m_spriteUvQuad[2].x = sprite->uvEnd.x;
    This->m_spriteUvQuad[2].y = sprite->uvEnd.y;
    This->m_spriteUvQuad[3].x = sprite->uvStart.x;
    This->m_spriteUvQuad[3].y = sprite->uvEnd.y;

    This->m_spriteSize.x = sprite->spriteWidth;
    This->m_spriteSize.y = sprite->spriteHeight;

    D3DXMatrixIdentity(&This->m_baseScaleMatrix);
    D3DXMatrixIdentity(&This->m_textureMatrix);

    This->m_baseScaleMatrix._11 = This->m_spriteSize.x * (1.f / 256.f);
    This->m_baseScaleMatrix._22 = This->m_spriteSize.y * (1.f / 256.f);

    This->m_localTransformMatrix = This->m_baseScaleMatrix;

    This->m_textureMatrix._11 = (This->m_spriteSize.x / sprite->bitmapWidth) * sprite->bitmapScale.x;
    This->m_textureMatrix._22 = (This->m_spriteSize.y / sprite->bitmapHeight) * sprite->bitmapScale.y;

    return 0;
}

// 0x44b2a0
int AnmVm::getIntVar(AnmVm* This, int id)
{
    switch (id)
    {
    case 10000:
        return This->m_intVars[0];
    case 10001:
        return This->m_intVars[1];
    case 10002:
        return This->m_intVars[2];
    case 10003:
        return This->m_intVars[3];
    case 10004:
        return static_cast<int>(This->m_floatVars[0]);
    case 10005:
        return static_cast<int>(This->m_floatVars[1]);
    case 10006:
        return static_cast<int>(This->m_floatVars[2]);
    case 10007:
        return static_cast<int>(This->m_floatVars[3]);
    case 10008:
        return This->m_intVar8;
    case 10009:
        return This->m_intVar9;
    case 10022:
        RngContext * ctx = &g_anmRngContext;
        if ((This->m_flagsLow & 0x40000000) == 0)
            ctx = &g_replayRngContext; // One of these is ANM RNG, while the other is replay RNG
        return rng(ctx);
    }
    return id;
}

// 0x44b400
int* AnmVm::getIntVarPtr(AnmVm* This, int* id)
{
    switch (*id)
    {
    case 10000:
        return &This->m_intVars[0];
    case 10001:
        return &This->m_intVars[1];
    case 10002:
        return &This->m_intVars[2];
    case 10003:
        return &This->m_intVars[3];
    case 10008:
        return &This->m_intVar8;
    case 10009:
        return &This->m_intVar9;
    }
    return id;
}

// 0x44b380
float* AnmVm::getFloatVarPtr(AnmVm* This, float* id)
{
    switch (static_cast<int>(*id))
    {
    case 10004:
        return &This->m_floatVars[0];
    case 10005:
        return &This->m_floatVars[1];
    case 10006:
        return &This->m_floatVars[2];
    case 10007:
        return &This->m_floatVars[3];
    case 10013:
        return &This->m_pos.x;
    case 10014:
        return &This->m_pos.y;
    case 10015:
        return &This->m_pos.z;
    default:
        return id;
    }
}

// 0x458c30
uint32_t AnmVm::rng(RngContext* RngContext)
{
    g_supervisor.enterCriticalSection(10);
    RngContext->counter += 2;
    uint16_t x = (RngContext->time ^ 0x9630) + 0x9aad;
    uint16_t y = ((x >> 0xe) + x * 4 ^ 0x9630) + 0x9aad;
    RngContext->time = (y >> 0xe) + y * 4;
    g_supervisor.leaveCriticalSection(10);
    return ((uint32_t)x << 16) | y;
}

/* 0x4070a0
 * Converts a signed int timestamp to float.
 * Handles wraparound by adding 2^32.
 * Normalizes to [-1, 1] via x * (1.0 / 2^32) - 1.0.
 * Scales result by angle.
 */
float AnmVm::normalizeToAngle(float angle, RngContext* rngContext)
{
    int rng = AnmVm::rng(rngContext);
    float fRng = static_cast<float>(rng);

    // Check for overflow
    if (rng < 0)
        fRng += (1ULL << 32);

    return (fRng * (1.0f / (1ULL << 32)) - 1.0f) * angle;
}

/* 0x458da0
 * Converts signed int timestamp to float.
 * If negative, adds 2^32.
 * Multiplies by 1 / 2^32.
 * Returns float in [0.0, 1.0).
 */
float AnmVm::normalizeUnsigned(RngContext* rngContext)
{
    int rng = AnmVm::rng(rngContext);
    float fRng = (float)rng;

    // Check for overflow
    if (rng < 0)
        fRng += (1ULL << 32);

    return fRng * (1.0f / (1ULL << 32));
}

/* 0x458dd0
 * Converts signed int timestamp to float.
 * Adds 2^32 if negative.
 * Multiplies by 1 / 2^31.
 * Subtracts 1.0.
 * Returns float in [-1.0, 1.0).
 */
float AnmVm::normalizeSigned(RngContext* rngContext)
{
    int rng = AnmVm::rng(rngContext);
    float fRng = (float)rng;

    // Check for overflow
    if (rng < 0)
        fRng += (1ULL << 32);

    return fRng * (1.0f / (1ULL << 31)) - 1.0f;
}

/* 0x40b9f0
 * normalize a 32-bit integer RNG output into a float in [0,1)
 */
float randScale(float f, RngContext* ctx)
{
    uint32_t u = AnmVm::rng(ctx);
    const float invTwo32 = 1.0f / static_cast<float>(1ULL << 32);
    return static_cast<float>(u) * invTwo32 * f;
}


// 0x44b080
float AnmVm::getFloatVar(AnmVm* This, float id)
{
    RngContext* rngContext;
    uint32_t rngValue;
    int roundedId = static_cast<int>(id);
    int index = roundedId - 10000;
    float fRng;

    switch (index)
    {
    case 0:
        return static_cast<float>(This->m_intVars[0]);
    case 1:
        return static_cast<float>(This->m_intVars[1]);
    case 2:
        return static_cast<float>(This->m_intVars[2]);
    case 3:
        return static_cast<float>(This->m_intVars[3]);
    case 4:
        return This->m_floatVars[0];
    case 5:
        return This->m_floatVars[1];
    case 6:
        return This->m_floatVars[2];
    case 7:
        return This->m_floatVars[3];
    case 8:
        return static_cast<float>(This->m_intVar8);
    case 9:
        return static_cast<float>(This->m_intVar9);
    case 10:
        rngContext = (This->m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeToAngle(3.1415927f, rngContext);
    case 11:
        rngContext = (This->m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeUnsigned(rngContext);
    case 12:
        rngContext = (This->m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeSigned(rngContext);
    case 13:
        return This->m_pos.x;
    case 14:
        return This->m_pos.y;
    case 15:
        return This->m_pos.z;
    case 16:
        return g_supervisor.stageCam.offset.x;
    case 17:
        return g_supervisor.stageCam.offset.y;
    case 18:
        return g_supervisor.stageCam.offset.z;
    case 19:
        return g_supervisor.stageCam.v3.x;
    case 20:
        return g_supervisor.stageCam.v3.y;
    case 21:
        return g_supervisor.stageCam.v3.z;
    case 22:
        rngContext = (This->m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        rngValue = rng(rngContext);
        fRng = static_cast<float>(rngValue);

        // Check for overflow
        if (rngValue < 0)
            fRng += (1ULL << 32); // 2^32
        return fRng;

        // Should never be reached
    default:
        return 0.0f;
    }
}

// 0x4500f0
void AnmVm::writeSpriteCharacters(
    AnmVm* This,
    RenderVertex144* bottomLeft,
    RenderVertex144* bottomRight,
    RenderVertex144* topRight,
    RenderVertex144* topLeft
)
{
    D3DXVECTOR3 effectivePos = This->m_entityPos + This->m_pos + This->m_offsetPos;
    float width = This->m_spriteSize.x * This->m_scale.x;
    float height = This->m_spriteSize.y * This->m_scale.y;

    uint32_t hAlign = (This->m_flagsLow >> 0x12) & 3;
    if (hAlign == 0)
    {
        float leftX = effectivePos.x - width * 0.5f;
        topRight->pos.x = leftX;
        bottomLeft->pos.x = leftX;
        float rightX = leftX + width;
        topLeft->pos.x = rightX;
        bottomRight->pos.x = rightX;
    }
    else if (hAlign == 1)
    {
        float leftX = effectivePos.x;
        topRight->pos.x = leftX;
        bottomLeft->pos.x = leftX;
        float rightX = effectivePos.x + width;
        topLeft->pos.x = rightX;
        bottomRight->pos.x = rightX;
    }
    else if (hAlign == 2)
    {
        float leftX = effectivePos.x - width;
        topRight->pos.x = leftX;
        bottomLeft->pos.x = leftX;
        float rightX = effectivePos.x;
        topLeft->pos.x = rightX;
        bottomRight->pos.x = rightX;
    }
    // Note: If hAlign == 3, x-coordinates are not set (matching original behavior)

    uint32_t vAlign = (This->m_flagsLow >> 0x14) & 3;
    if (vAlign == 0)
    {
        float bottomY = effectivePos.y - height * 0.5f;
        bottomRight->pos.y = bottomY;
        bottomLeft->pos.y = bottomY;
        float topY = bottomY + height;
        topLeft->pos.y = topY;
        topRight->pos.y = topY;
    }
    else if (vAlign == 1)
    {
        float bottomY = effectivePos.y;
        bottomRight->pos.y = bottomY;
        bottomLeft->pos.y = bottomY;
        float topY = effectivePos.y + height;
        topLeft->pos.y = topY;
        topRight->pos.y = topY;
    }
    else if (vAlign == 2)
    {
        float bottomY = effectivePos.y - height;
        bottomRight->pos.y = bottomY;
        bottomLeft->pos.y = bottomY;
        float topY = effectivePos.y;
        topLeft->pos.y = topY;
        topRight->pos.y = topY;
    }
    // Note: If vAlign == 3, y-coordinates are not set (matching original behavior)

    float effectiveZ = effectivePos.z;
    topLeft->pos.z = effectiveZ;
    topRight->pos.z = effectiveZ;
    bottomRight->pos.z = effectiveZ;
    bottomLeft->pos.z = effectiveZ;
}

// 0x44fe30
void AnmVm::writeSpriteCharactersWithoutRot(
    AnmVm* This,
    RenderVertex144* bottomLeft,
    RenderVertex144* bottomRight,
    RenderVertex144* topRight,
    RenderVertex144* topLeft
)
{
    D3DXVECTOR3 effectivePos = This->m_entityPos + This->m_pos + This->m_offsetPos;
    float width = This->m_spriteSize.x * This->m_scale.x;
    float height = This->m_spriteSize.y * This->m_scale.y;

    uint32_t hAlign = (This->m_flagsLow >> 0x12) & 3;
    float leftX, rightX;
    if (hAlign == 0)
    {
        leftX = floor(effectivePos.x - width * 0.5f);
        rightX = leftX + width;
    }
    else if (hAlign == 1)
    {
        leftX = effectivePos.x;
        rightX = effectivePos.x + width;
    }
    else if (hAlign == 2)
    {
        leftX = effectivePos.x - width;
        rightX = effectivePos.x;
    }
    else
        return;  // x not set for mode 3

    // Assign based on param order: left to topRight/bottomLeft, right to topLeft/bottomRight
    topRight->pos.x = leftX;
    bottomLeft->pos.x = leftX;
    topLeft->pos.x = rightX;
    bottomRight->pos.x = rightX;

    uint32_t vAlign = (This->m_flagsLow >> 0x14) & 3;
    float bottomY, topY;
    if (vAlign == 0)
    {
        bottomY = floor(effectivePos.y - height * 0.5f);
        topY = bottomY + height;
    }
    else if (vAlign == 1)
    {
        bottomY = effectivePos.y;
        topY = effectivePos.y + height;
    }
    else if (vAlign == 2)
    {
        bottomY = effectivePos.y - height;
        topY = effectivePos.y;
    }
    else
        return;  // y not set for mode 3

    bottomRight->pos.y = bottomY;
    bottomLeft->pos.y = bottomY;
    topLeft->pos.y = topY;
    topRight->pos.y = topY;

    float effectiveZ = effectivePos.z;
    topLeft->pos.z = effectiveZ;
    topRight->pos.z = effectiveZ;
    bottomRight->pos.z = effectiveZ;
    bottomLeft->pos.z = effectiveZ;
}

void AnmVm::applyZRotationToQuadCorners(
    AnmVm* This,
    RenderVertex144* bottomLeft,
    RenderVertex144* bottomRight,
    RenderVertex144* topLeft,
    RenderVertex144* topRight
)
{
    D3DXVECTOR3 effectivePos = This->m_entityPos + This->m_pos + This->m_offsetPos;
    float width = This->m_spriteSize.x * This->m_scale.x;
    float height = This->m_spriteSize.y * This->m_scale.y;

    uint32_t hAlign = (This->m_flagsLow >> 0x12) & 3;
    float leftX;
    float rightX;

    if (hAlign == 0)
    {
        leftX = -width * 0.5f;
        rightX = width * 0.5f;
    }

    else if (hAlign == 1)
    {
        leftX = 0.0f;
        rightX = width;
    }

    else if (hAlign == 2)
    {
        leftX = -width;
        rightX = 0.0f;
    }
    else
        return; // x not set for mode 3

    uint32_t vAlign = (This->m_flagsLow >> 0x14) & 3;
    float bottomY;
    float topY;

    if (vAlign == 0)
    {
        bottomY = -height * 0.5f;
        topY = height * 0.5f;
    }

    else if (vAlign == 1)
    {
        bottomY = 0.0f;
        topY = height;
    }

    else if (vAlign == 2)
    {
        bottomY = -height;
        topY = 0.0f;
    }
    else
        return; // y not set for mode 3

    float rotZ = This->m_rotation.z;
    float cosZ = cosf(rotZ);
    float sinZ = sinf(rotZ);

    // Bottom-Left corner
    bottomLeft->pos.x = effectivePos.x + (cosZ * leftX - sinZ * bottomY);
    bottomLeft->pos.y = effectivePos.y + (cosZ * bottomY + sinZ * leftX);

    // Bottom-Right corner
    bottomRight->pos.x = effectivePos.x + (cosZ * rightX - sinZ * bottomY);
    bottomRight->pos.y = effectivePos.y + (cosZ * bottomY + sinZ * rightX);

    // Top-Left corner
    topLeft->pos.x = effectivePos.x + (cosZ * leftX - sinZ * topY);
    topLeft->pos.y = effectivePos.y + (cosZ * topY + sinZ * leftX);

    // Top-Right corner
    topRight->pos.x = effectivePos.x + (cosZ * rightX - sinZ * topY);
    topRight->pos.y = effectivePos.y + (cosZ * topY + sinZ * rightX);

    float effectiveZ = effectivePos.z;
    topRight->pos.z = effectiveZ;
    topLeft->pos.z = effectiveZ;
    bottomRight->pos.z = effectiveZ;
    bottomLeft->pos.z = effectiveZ;
}

int AnmVm::projectQuadCornersThroughCameraViewport(AnmVm* This)
{
    float rotZ = This->m_rotation.z;
    float cosZ = cosf(rotZ);
    float sinZ = sinf(rotZ);

    D3DXVECTOR3 localOrigin{};
    D3DXMATRIX worldMatrix;
    D3DXMatrixIdentity(&worldMatrix);

    // Set translation
    worldMatrix._41 = This->m_entityPos.x + This->m_pos.x + This->m_offsetPos.x;
    worldMatrix._42 = This->m_entityPos.y + This->m_pos.y + This->m_offsetPos.y;
    worldMatrix._43 = This->m_entityPos.z + This->m_pos.z + This->m_offsetPos.z;

    D3DXVECTOR3 screenPos;
    D3DXVec3Project(&screenPos, &localOrigin, &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix, &worldMatrix);

    if (screenPos.z < 0.0f || screenPos.z > 1.0f)
        return -1;

    D3DXVECTOR3 callibratedScreenPos;
    D3DXVec3Project(
        &callibratedScreenPos,
        &g_supervisor.currentCam->callibration,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix, &worldMatrix
    );

    D3DXVECTOR3 diff;
    diff.x = callibratedScreenPos.x - screenPos.x;
    diff.y = callibratedScreenPos.y - screenPos.y;
    diff.z = callibratedScreenPos.z - screenPos.z;

    float diffLength = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    float scaleFactor = diffLength * 0.5f;

    float width = This->m_scale.x * scaleFactor * This->m_spriteSize.x;
    float height = This->m_scale.y * scaleFactor * This->m_spriteSize.y;

    float offsetXBottomLeft{}, offsetXBottomRight{}, offsetXTopLeft{}, offsetXTopRight{};
    float offsetYBottomLeft{}, offsetYBottomRight{}, offsetYTopLeft{}, offsetYTopRight{};

    uint32_t anchorX = (This->m_flagsLow >> 18) & 3;
    if (anchorX == 0) // centered
    {
        float halfW = width * 0.5f;
        offsetXBottomLeft = -halfW;
        offsetXBottomRight = halfW;
        offsetXTopLeft = -halfW;
        offsetXTopRight = halfW;
    }

    else if (anchorX == 1) // left-aligned
    {
        offsetXBottomLeft = 0.0f;
        offsetXBottomRight = width;
        offsetXTopLeft = 0.0f;
        offsetXTopRight = width;
    }

    else if (anchorX == 2) // right-aligned
    {
        offsetXBottomLeft = -width;
        offsetXBottomRight = 0.0f;
        offsetXTopLeft = -width;
        offsetXTopRight = 0.0f;
    }

    uint32_t anchorY = (This->m_flagsLow >> 20) & 3;
    if (anchorY == 0) // centered
    {
        float halfH = height * 0.5f;
        offsetYBottomLeft = -halfH;
        offsetYBottomRight = -halfH;
        offsetYTopLeft = halfH;
        offsetYTopRight = halfH;
    }
    else if (anchorY == 1) // bottom-aligned (assuming y increases down)
    {
        offsetYBottomLeft = 0.0f;
        offsetYBottomRight = 0.0f;
        offsetYTopLeft = height;
        offsetYTopRight = height;
    }
    else if (anchorY == 2) // top-aligned
    {
        offsetYBottomLeft = -height;
        offsetYBottomRight = -height;
        offsetYTopLeft = 0.0f;
        offsetYTopRight = 0.0f;
    }

    g_renderQuad144[0].pos.x = screenPos.x + (cosZ * offsetXBottomLeft - sinZ * offsetYBottomLeft);
    g_renderQuad144[0].pos.y = screenPos.y + (cosZ * offsetYBottomLeft + sinZ * offsetXBottomLeft);
    g_renderQuad144[0].pos.z = screenPos.z;

    g_renderQuad144[1].pos.x = screenPos.x + (cosZ * offsetXBottomRight - sinZ * offsetYBottomRight);
    g_renderQuad144[1].pos.y = screenPos.y + (cosZ * offsetYBottomRight + sinZ * offsetXBottomRight);
    g_renderQuad144[1].pos.z = screenPos.z;

    g_renderQuad144[2].pos.x = screenPos.x + (cosZ * offsetXTopLeft - sinZ * offsetYTopLeft);
    g_renderQuad144[2].pos.y = screenPos.y + (cosZ * offsetYTopLeft + sinZ * offsetXTopLeft);
    g_renderQuad144[2].pos.z = screenPos.z;

    g_renderQuad144[3].pos.x = screenPos.x + (cosZ * offsetXTopRight - sinZ * offsetYTopRight);
    g_renderQuad144[3].pos.y = screenPos.y + (cosZ * offsetYTopRight + sinZ * offsetXTopRight);
    g_renderQuad144[3].pos.z = screenPos.z;
    return 0;
}

void AnmVm::WrapUVsX(RenderVertex144* vertices, int count)
{
    for (int i = 0; i < count; ++i)
    {
        vertices[i].uv.x += 1.0f;
    }
}

void AnmVm::WrapUVsY(RenderVertex144* vertices, int count)
{
    for (int i = 0; i < count; ++i)
    {
        vertices[i].uv.y += 1.0f;
    }
}

void AnmVm::inlineAsmFsincos(RenderVertex144* This, float theta, float scale)
{
    This->pos.x = cos(theta) * scale;
    This->pos.y = sin(theta) * scale;
}

// 0x452420
int AnmVm::onTick(AnmVm* This)
{
    puts("!! called !!");
    SpecialRenderData* data = This->m_specialRenderData;

    // 0x452423: Initialize angle to -PI
    float currentAngle = -3.1415927f;

    // The game maps World Y -> Vert X, World Z -> Vert Y, World X -> Vert Z
    float totalY = This->m_entityPos.y + This->m_pos.y;
    float totalZ = This->m_entityPos.z + This->m_pos.z;
    float totalX = This->m_entityPos.x + This->m_pos.x;

    // Update Center Vertex (Index 0)
    data->vertices[0].pos.x = totalY;
    data->vertices[0].pos.y = totalZ;
    data->vertices[0].pos.z = totalX;

    // UV Scrolling (U Coordinate)
    float scrollSpeed = data->uvScrollSpeedU;

    data->vertices[0].uv.x += scrollSpeed;

    // 0x452491: Check if U < 0. If so, loop through ALL vertices and add 1.0
    if (data->vertices[0].uv.x < 0.0f)
    {
        for (int i = 0; i < 33; i++)
        {
            data->vertices[i].uv.x += 1.0f;
        }
    }

    // UV Scrolling (V Coordinate)
    data->vertices[0].uv.y += scrollSpeed;
    if (data->vertices[0].uv.y < 0.0f)
    {
        for (int i = 0; i < 33; i++)
            data->vertices[i].uv.y += 1.0f;
    }

    data->vertices[0].diffuse = This->m_color0;

    // Update Ring Vertices (Indices 1 to 31)
    float* radiusPtr = data->currentRadii;
    RenderVertex144* vert = &data->vertices[1];

    for (int k = 0x1f; k > 0; k--) // Loop 31 times
    {
        // Update UVs for this vertex
        vert->uv.x += scrollSpeed;
        if (vert->uv.x < 0.0f)
        {
            for (int i = 0; i < 33; i++)
                data->vertices[i].uv.x += 1.0f;
        }

        vert->uv.y += scrollSpeed;
        if (vert->uv.y < 0.0f)
        {
            for (int i = 0; i < 33; i++)
                data->vertices[i].uv.y += 1.0f;
        }

        // Set Color (Clear Alpha/High byte)
        vert->diffuse = This->m_color0;
        vert->diffuse &= 0x00FFFFFF;

        // Update Radius
        float delta = *(radiusPtr + 33);
        *radiusPtr += delta;
        float r = *radiusPtr;

        // Calculate Position (X/Y)
        inlineAsmFsincos(vert, currentAngle, r);

        // Apply Position Offset
        vert->pos.x += totalY;
        vert->pos.y += totalZ;
        vert->pos.z += totalX;

        // Advance pointers and angle
        vert++;
        radiusPtr++;
        currentAngle += 0.2026834f; // ~0.2 radians
    }

    // Close the Loop
    data->vertices[32] = data->vertices[1];
    return 0;
}

void AnmVm::printD3DMatrix(const D3DMATRIX& m)
{
    std::cout << std::fixed << std::setprecision(4);

    std::cout
        << "[ "
        << std::setw(9) << m._11 << " "
        << std::setw(9) << m._12 << " "
        << std::setw(9) << m._13 << " "
        << std::setw(9) << m._14 << " ]\n"

        << "[ "
        << std::setw(9) << m._21 << " "
        << std::setw(9) << m._22 << " "
        << std::setw(9) << m._23 << " "
        << std::setw(9) << m._24 << " ]\n"

        << "[ "
        << std::setw(9) << m._31 << " "
        << std::setw(9) << m._32 << " "
        << std::setw(9) << m._33 << " "
        << std::setw(9) << m._34 << " ]\n"

        << "[ "
        << std::setw(9) << m._41 << " "
        << std::setw(9) << m._42 << " "
        << std::setw(9) << m._43 << " "
        << std::setw(9) << m._44 << " ]\n";
    std::cout << "\n\n";
}


// 0x44b4b0
void AnmVm::run(AnmVm* This)
{
#if 0
    while (This->m_currentInstruction != nullptr)
    {
        if (This->m_flagsLow & 0x20000)
            return;

        float gameSpeed = g_gameSpeed;
        g_gameSpeed = 1.0;

        AnmRawInstruction* instr_interrupt = nullptr;
        uint32_t opcode = This->m_currentInstruction->opcode;

        //TODO: Move local variables into inner scopes if possible
        uint32_t spriteNumber;
        uint32_t arg0_1;
        int* intVarPtr;
        int* interruptInstrVar;
        float rX;
        float rY;
        float rZ;
        float posX;
        float posY;
        float posZ;
        float prevPosX;
        float prevPosY;
        float prevPosZ;
        float prevOffsetPosX;
        float prevOffsetPosY;
        float prevOffsetPosZ;

        if (This->m_pendingInterrupt != 0)
            goto Interrupt;

        if (This->m_currentInstruction->time > This->m_timeInScript.m_current)
        {
            // TODO: Update timers and interpolation
            return;
        }

        switch (opcode)
        {
            // Does nothing.
        case 0: // nop
            break;

            // Destroys the graphic.
        case 1: // delete
            This->m_flagsLow &= ~1; // Clear visibility
            This->m_currentInstruction = nullptr; // Terminate script
            g_gameSpeed = gameSpeed;
            break;

            // Freezes the graphic until it is destroyed externally.
            // Any interpolation instructions like posTime will no longer advance, and interrupts are disabled.
        case 2: // static
            This->m_currentInstruction = nullptr; // Terminate script
            g_gameSpeed = gameSpeed;
            return;

            // Sets the image used by this VM to one of the sprites defined in the ANM file.
            // A value of -1 means to not use an image (this is frequently used with special drawing instructions).
            // Thanm also lets you use the sprite's name instead of an index.
            // Under some unknown conditions, these sprite indices are transformed by a "sprite-mapping" function; e.g. many bullet scripts use false indices, presumably to avoid repeating the same script for 16 different colors. The precise mechanism of this is not yet fully understood.
        case 3: // sprite(int id)
            This->m_flagsLow |= 1; // Set visibility

            if (This->m_spriteMappingFunc == nullptr)
            {
                spriteNumber = This->m_currentInstruction->args[0];
                if ((This->m_currentInstruction->varMask & 1) != 0)
                    spriteNumber = getIntVar(This, spriteNumber);
            }

            else
            {
                arg0_1 = This->m_currentInstruction->args[0];
                if ((This->m_currentInstruction->varMask & 1) != 0)
                    arg0_1 = getIntVar(This, arg0_1);
                spriteNumber = (uint32_t)This->m_spriteMappingFunc(This, arg0_1);
            }
            setupTextureQuadAndMatrices(This, spriteNumber, This->m_anmLoaded);
            This->loadNextInstruction();
            break;

            // Jumps to byte offset dest from the script's beginning and sets the time to t. thanm accepts a label name for dest.
            // Chinese wiki says some confusing recommendation about setting a=0, can someone explain to me?
        case 4: // jmp(int dest, int t)
        {
            int dest = This->m_currentInstruction->args[0];
            int t = This->m_currentInstruction->args[1];
            This->m_timeInScript.setCurrent(t);
            This->jumpToInstruction(dest);
            break;
        }

        // Decrement count and then jump if count > 0. You can use this to repeat a loop a fixed number of times.
        case 5: // jmpDec(int& count, int dest, int t)
        {
            static int* count;
            count = &This->m_currentInstruction->args[0];
            int dest = This->m_currentInstruction->args[1];
            int t = This->m_currentInstruction->args[2];
            int originalCount = *count;

            if (This->m_currentInstruction->varMask & 1)
                count = getIntVarPtr(This, This->m_currentInstruction->args);
            *count -= 1;

            if (This->m_currentInstruction->varMask & 1)
                originalCount = getIntVar(This, originalCount);

            if (originalCount > 0)
            {
                This->m_timeInScript.setCurrent(t);
                This->jumpToInstruction(dest);
            }
            break;
        }

        // Does a = b.
        case 6: // iset(int& dest, int val)
        {
            int src = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) == 0)
                src = getIntVar(This, This->m_currentInstruction->args[1]);

            int* dest = reinterpret_cast<int*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) == 0)
                dest = getIntVarPtr(This, dest);

            *dest = src;
            This->loadNextInstruction();
            break;
        }
        // Does a = b.
        case 7: // fset(float& dest, float val)
        {
            float src = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                src = getFloatVar(This, src);

            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);
            *dest = src;
            This->loadNextInstruction();
            break;
        }
        
        // Does a += b.
        case 8: // iadd(int& a, int b)
        {
            int b = getIntVar(This, This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = This->m_currentInstruction->args[1];
            
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);
            *a += b;
            This->loadNextInstruction();
            break;
        }
        
        // Does a += b.
        case 9: // fadd(float& a, float b)
        {
            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);

            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            *a += b;
            This->loadNextInstruction();
            break;
        }
        
        // Does a -= b.
        case 10: // isub(int& a, int b)
        {
            int b = This->m_currentInstruction->args[1];
    
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, This->m_currentInstruction->args[1]);

            int* a = This->m_currentInstruction->args;
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            *a -= b;
            This->loadNextInstruction();
            break;
        }
        
        // Does a -= b.
        case 11: // fsub(float& a, float b)
        {
            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);

            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            *a -= b;
            This->loadNextInstruction();
            break;
        }

        // Does a *= b.
        case 12: // imul(int& a, int b)
        {
            int b = getIntVar(This, This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) == 0)
                b = This->m_currentInstruction->args[1];

            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            *a *= b;
            This->loadNextInstruction();
            break;
        }

        // Does a *= b.
        case 13: // fmul(float& a, float b)
        {
            float b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);

            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            *a *= b;
            This->loadNextInstruction();
        }
        // Does a /= b.
        case 14: // idiv(int& a, int b)
        {
            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, This->m_currentInstruction->args[1]);

            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            *a /= b;
            This->loadNextInstruction();
            break;
        }

        // Does a /= b.
        case 15: // fdiv(float& a, float b)
        {
            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);

            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            *a /= b;
            This->loadNextInstruction();
            break;
        }
        // Does a %= b.
        case 16: // imod(int& a, int b)
        {
            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, This->m_currentInstruction->args[1]);

            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            *a %= b;
            This->loadNextInstruction();
            break;
        }

        // Does a %= b.
        case 17: // fmod(float& a, float b)
        {
            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);

            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float a = bit_cast<float>(This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
            {
                a = getFloatVar(This, a);
                dest = getFloatVarPtr(This, dest);
            }

            *dest = fmod(a, b);
            This->loadNextInstruction();
            break;
        }

        // Does a = b + c.
        case 18: // isetAdd(int& a, int b, int c)
        {
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, b);

            int c = This->m_currentInstruction->args[2];
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getIntVar(This, c);

            *a = b + c;
            This->loadNextInstruction();
            break;
        }

        // Does a = b + c.
        case 19: // fsetAdd(float& x, float a, float b)
        {
            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);
        
            float c = bit_cast<float>(This->m_currentInstruction->args[2]);
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getFloatVar(This, c);
            
            *a = b + c;
            This->loadNextInstruction();
            break;
        }

        // Does a = b - c.
        case 20: // isetSub(int& x, int a, int b)
        {
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, b);
        
            int c = This->m_currentInstruction->args[2];
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getIntVar(This, c);
            
            *a = b - c;
            This->loadNextInstruction();
            break;
        }
        // Does a = b - c.
        case 21: // fsetSub(float& a, float b, float c)
        {
            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);
        
            float c = bit_cast<float>(This->m_currentInstruction->args[2]);
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getFloatVar(This, c);
            
            *a = b - c;
            This->loadNextInstruction();
            break;
        }

        // Does a = b * c.
        case 22: // isetMul(int& a, int b, int c)
        {
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, b);
        
            int c = This->m_currentInstruction->args[2];
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getIntVar(This, c);
            
            *a = b * c;
            This->loadNextInstruction();
            break;
        }

        // Does a = b * c.
        case 23: // fsetMul(float& a, float b, float c)
        {
            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);
        
            float c = bit_cast<float>(This->m_currentInstruction->args[2]);
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getFloatVar(This, c);
            
            *a = b * c;
            This->loadNextInstruction();
            break;
        }
        // Does a = b / c.
        case 24: // isetDiv(int& x, int a, int b)
        {
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, b);
        
            int c = This->m_currentInstruction->args[2];
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getIntVar(This, c);
            
            *a = b / c;
            This->loadNextInstruction();
            break;
        }
        // Does a = b / c.
        case 25: // fsetDiv(float& x, float a, float b)
        {
            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);

            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);
        
            float c = bit_cast<float>(This->m_currentInstruction->args[2]);
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getFloatVar(This, c);
            
            *a = b / c;
            This->loadNextInstruction();
            break;
        }

        // Does a = b % c.
        case 26: // isetMod(int& x, int a, int b)
        {
            int* a = &This->m_currentInstruction->args[0];
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getIntVarPtr(This, a);

            int b = This->m_currentInstruction->args[1];
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getIntVar(This, b);

            int c = This->m_currentInstruction->args[2];
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getIntVar(This, c);
            
            *a = b % c;
            This->loadNextInstruction();
            break;
        }
 
        // Does a = b % c.
        case 27: // fsetMod(float& a, float b, float c)
        {
            float c = bit_cast<float>(This->m_currentInstruction->args[2]);
            if ((This->m_currentInstruction->varMask & 4) != 0)
                c = getFloatVar(This, c);

            float b = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                b = getFloatVar(This, b);
 
            float* a = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                a = getFloatVarPtr(This, a);
            
            *a = fmod(b,c);
            This->loadNextInstruction();
        }

        // Jumps if a == b.
        case 28: // ije(int a, int b, int dest, int t)

            break;

        // Jumps if a == b.
        case 29: // fje(float a, float b, int dest, int t)

            break;

        // Jumps if a != b.
        case 30: // ijne(int a, int b, int dest, int t)

            break;

        // Jumps if a != b.
        case 31: // fjne(float a, float b, int dest, int t)

            break;

        // Jumps if a < b.
        case 32: // ijl(int a, int b, int dest, int t)

            break;

        // Jumps if a < b.
        case 33: // fjl(float a, float b, int dest, int t)

            break;

        // Jumps if a <= b.
        case 34: // ijle(int a, int b, int dest, int t)

            break;

        // Jumps if a <= b.
        case 35: // fjle(float a, float b, int dest, int t)

            break;

        // Jumps if a > b.
        case 36: // ijg(int a, int b, int dest, int t)

            break;

        // Jumps if a > b.
        case 37: // fjg(float a, float b, int dest, int t)

            break;

        // Jumps if a >= b.
        case 38: // ijge(int a, int b, int dest, int t)
            break;

        // Jumps if a >= b.
        case 39: // fjge(float a, float b, int dest, int t)
            break;

        // Draw a random integer 0 <= x < n using the animation RNG.
        case 40: // isetRand(int& x, int n)
        {
            //         int n = m_currentInstruction->args[1];
            //         if ((m_flagsLow & 0x40000000) == 0) 
            //         {
            //             if (m_currentInstruction->varMask & 2) 
            //             {
            //                 uVar3 = getIntVar(This,uVar3);
            //             }
            //             if (uVar3 == 0) goto LAB_0044d4da;
            //             spriteNumber = rng(&g_replayRngContext);
            //             posY = (float)(spriteNumber % uVar3);
            //             instr_interrupt = (AnmRawInstruction *)posY;
            //         }
            //         else {
            //           if ((m_currentInstruction->varMask & 2) != 0) {
            //             uVar3 = getIntVar(This,uVar3);
            //           }
            //           if (uVar3 == 0) {
            // LAB_0044d4da:
            //             instr_interrupt = (AnmRawInstruction *)0x0;
            //           }
            //           else {
            //             spriteNumber = rng(&g_anmRngContext);
            //             posY = (float)(spriteNumber % uVar3);
            //             instr_interrupt = (AnmRawInstruction *)posY;
            //           }
            //         }
            //         args = m_currentInstruction->args;
            //         if ((m_currentInstruction->varMask & 1) != 0) {
            //           args = getIntVarPtr(This,args);
            //         }
            // HelpMe:
            //         *args = (int)instr_interrupt;
            //         nextInstruction =
            //              (AnmRawInstruction *)((int)m_currentInstruction->args + (m_currentInstruction->offsetToNextInstr - 8));
            //         This->m_currentInstruction = nextInstruction;
            break;
        }

        // Draw a random float 0 <= x <= r using the animation RNG.
        case 41: // fsetRand(float& x, float r)
        {
            RngContext* rngCtx;
            float r = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_flagsLow & 0x40000000) == 0)
            {
                if ((This->m_currentInstruction->varMask & 2) != 0)
                    posX = getFloatVar(This, posX);
                rngCtx = &g_replayRngContext;
            }
            else
            {
                if ((This->m_currentInstruction->varMask & 2) != 0)
                    posX = getFloatVar(This, posX);
                rngCtx = &g_anmRngContext;
            }

            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = randScale(posX, rngCtx);
            This->loadNextInstruction();
        }

        // Compute sin(theta) (in radians).
        case 42: // fsin(float& dest, float theta)
        {
            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float theta = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                theta = getFloatVar(This, posX);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = std::sinf(posX);
            This->loadNextInstruction();
            break;
        }

        // Compute cos(theta) (in radians).
        case 43: // fcos(float& dest, float theta)
        {
            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float theta = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                theta = getFloatVar(This, theta);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = std::cosf(theta);
            This->loadNextInstruction();
            break;
        }

        // Compute tan(theta) (in radians).
        case 44: // ftan(float& dest, float theta)
        {
            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float theta = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                theta = getFloatVar(This, theta);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = std::tanf(theta);
            This->loadNextInstruction();
            break;
        }
        // Compute acos(x) (in radians).
        case 45: // facos(float& dest, float x)
        {
            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float theta = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                theta = getFloatVar(This, theta);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = std::acosf(theta);
            This->loadNextInstruction();
            break;
        }

        // Compute atan(f) (in radians).
        case 46: // fatan(float& dest, float f)
        {
            float* dest = reinterpret_cast<float*>(&This->m_currentInstruction->args[0]);
            float theta = bit_cast<float>(This->m_currentInstruction->args[1]);
            if ((This->m_currentInstruction->varMask & 2) != 0)
                theta = getFloatVar(This, theta);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                dest = getFloatVarPtr(This, dest);

            *dest = std::atanf(theta);
            This->loadNextInstruction();
            break;
        }

        // Reduce an angle modulo 2*PI into the range [-PI, +PI].
        case 47: // validRad(float& theta)
            break;

            // Sets the position of the graphic.
            // If you look in the code, you'll see that if a certain bitflag is set, this will write to a different variable.
            // This is part of the implementation of th09:posMode in earlier games, and is, to my knowledge,
            // entirely dead code in every game since StB.
        case 48: // pos(float x, float y, float z)
            posX = bit_cast<float>(This->m_currentInstruction->args[0]);
            posY = bit_cast<float>(This->m_currentInstruction->args[1]);
            posZ = bit_cast<float>(This->m_currentInstruction->args[2]);

            if ((This, This->m_flagsLow & 0x100) == 0)
            {
                if ((This, This->m_currentInstruction->varMask & 4) != 0)
                    posZ = getFloatVar(This, posZ);

                if ((This, This->m_currentInstruction->varMask & 2) != 0)
                    posY = getFloatVar(This, posY);

                if ((This, This->m_currentInstruction->varMask & 1) != 0)
                    posX = getFloatVar(This, posX);

                This->m_pos.x = posX;
                This->m_pos.y = posY;
                This->m_pos.z = posZ;
                prevPosX = posX;
                prevPosY = posY;
                prevPosZ = posZ;
            }
            else
            {
                if ((This->m_currentInstruction->varMask & 4) != 0)
                    posZ = getFloatVar(This, posZ);

                if ((This->m_currentInstruction->varMask & 2) != 0)
                    posY = getFloatVar(This, posY);

                if ((This->m_currentInstruction->varMask & 1) != 0)
                    posX = getFloatVar(This, posX);

                This->m_offsetPos.x = posX;
                This->m_offsetPos.y = posY;
                This->m_offsetPos.z = posZ;
                prevOffsetPosX = posX;
                prevOffsetPosY = posY;
                prevOffsetPosZ = posZ;
            }
            This->loadNextInstruction();
            break;

            // Set the graphic's rotation. For 2D objects, only the z rotation matters.
            // In some rare cases, x rotation has a special meaning for special drawing instructions.
            // Graphics rotate around their anchor point (see anchor).
            // A positive angle around the z-axis goes clockwise from the +x direction towards the +y direction (defined to point down).
            // 3D rotations are performed as follows:
            // (EoSD) Rotate first around x, then around y, then around z.
            // (PCB-GFW) Haven't checked. Probably the same?
            // (TD-) You can choose the rotation system with th185:rotationMode. (what's the default? probably the same?)
            // If nothing seems to be happening when you call this, check your type setting!
        case 49: // rotate(float rx, float ry, float rz)
        {
            rX = bit_cast<float>(This->m_currentInstruction->args[0]);
            rY = bit_cast<float>(This->m_currentInstruction->args[1]);
            rZ = bit_cast<float>(This->m_currentInstruction->args[2]);

            if ((This->m_currentInstruction->varMask & 1) != 0)
                rX = getFloatVar(This, rX);

            if ((This->m_currentInstruction->varMask & 2) != 0)
                rY = getFloatVar(This, rY);

            if ((This->m_currentInstruction->varMask & 4) != 0)
                rZ = getFloatVar(This, rZ);

            This->m_flagsLow |= 4;
            This->m_rotation.x = rX;
            This->m_rotation.y = rY;
            This->m_rotation.z = rZ;
            This->loadNextInstruction();
            break;
        }
        // Scales the ANM independently along the x and y axis. Graphics grow around their anchor point (see anchor). Some special drawing instructions give the x and y scales special meaning.
        case 50: // scale(float sx, float sy)

            break;

            // Set alpha (opacity) to a value 0-255.
        case 51: // alpha(int alpha)

            break;

            // Set a color which gets blended with this sprite. Blend operation is multiply, so setting white (255, 255, 255) eliminates the effect.
        case 52: // color(int r, int g, int b)

            break;

            // Set a constant angular velocity, in radians per frame.
        case 53: // angleVel(float x, float y, float z)

            break;

            // Every frame, it increases the values of scale as sx -> sx + gx and sy -> sy + gy. Basically, scaleGrowth is to scale as angleVel is to rotate. (they even share implemenation details...)
        case 54: // scaleGrowth(float gx, float gy)

            break;

            // Obsolete. Use alphaTime instead.
            // UNTESTED: Linearly changes alpha to alpha over the next t frames. Identical to calling alphaTime(t, 0, alpha).
        case 55: // alphaTimeLinear(int alpha, int t)

            break;

            // Over the next t frames, changes pos to the given values using interpolation mode mode.
        case 56: // posTime(int t, int mode, float x, float y, float z)

            break;

            // Over the next t frames, changes color to the given value using interpolation mode mode.
        case 57: // colorTime(int t, int mode, int r, int g, int b)

            break;

            // Over the next t frames, changes alpha to the given values using interpolation mode mode.
        case 58: // alphaTime(int t, int mode, int alpha)

            break;

            // Over the next t frames, changes rotate to the given values using interpolation mode mode.
        case 59: // rotateTime(int t, int mode, float rx, float ry, float rz)

            break;

            // Over the next t frames, changes scale to the given values using interpolation mode mode.
        case 60: // scaleTime(int t, int mode, float sx, float sy)

            break;

            // Toggles mirroring on the x axis.
            // This flips the sign of sx in scale. Future calls to scale will thus clobber it.
            // (It also toggles a bitflag. This flag is used by EoSD to keep the sign flipped during interpolation of scale, and is then unused until BM which added th185:unflip.)
        case 61: // flipX

            break;

            // Toggles mirroring on the y axis.
            // This flips the sign of sy in scale. Future calls to scale will thus clobber it.
            // (It also toggles a bitflag. This flag is used by EoSD to keep the sign flipped during interpolation of scale, and is then unused until BM which added th185:unflip.)
        case 62: // flipY

            break;

            // Stops executing the script (at least for now), but keeps the graphic alive.
            // Interpolation instructions like posTime will continue to advance, and interrupts can be triggered at any time. You could say this essentially behaves like an infinitely long wait.
        case 63: // stop
            goto switchD_0044b52d_caseD_3f;

            // A label for an interrupt. When executed, it is a no-op.
        case 64: // interruptLabel(int n)
            // No-op
            break;

            // Set the horizontal and vertical anchoring of the sprite. Notice the args are each two bytes. For further fine-tuning see th185:anchorOffset.
            // Args: 0=Center, 1=Left, 2=Right (h); 0=Center, 1=Top, 2=Bottom (v)
        case 65: // anchor(short h, short v)

            break;

            // Set color blending mode.
            // Modes for DDC: (other games may be different)
            // 0: Normal (SRCALPHA, INVSRCALPHA, ADD)
            // 1: Add (SRCALPHA, ONE, ADD)
            // 2: Subtract (SRCALPHA, ONE, REVSUBTRACT)
            // ...
        case 66: // blendMode(int mode)

            break;

            // Determines how the ANM is rendered.
            // Mode 0: 2D sprites, no rotation.
            // Mode 1: 2D sprites with z-axis rotation.
            // Mode 8: 3D rotation.
            // Mode 2: Like mode 0 but shifted by (-0.5, -0.5) pixels.
        case 67: // type(int mode)

            break;

            // Sets the layer of the ANM. This may or may not affect z-ordering? It's weird...
            // Different layer numbers may behave differently! Each game only has a finite number of layers, and certain groups of these layers are drawn at different stages in the rendering pipeline.
        case 68: // layer(int n)

            break;

            // This is like stop except that it also hides the graphic by clearing the visibility flag (see visible).
            // Interpolation instructions like posTime will continue to advance, and interrupts can be triggered at any time. Successful interrupts will automatically re-enable the visibility flag.
        case 69: // stopHide
            This->m_flagsLow &= 0xfffffffe;

        switchD_0044b52d_caseD_3f:

            if (This->m_pendingInterrupt == 0)
                This->m_flagsLow |= 0x1000;
            else
            {
            Interrupt:
                This->m_currentInstruction = This->m_beginningOfScript;
                instr_interrupt = nullptr;
                while (1)
                {
                    uint16_t m_currentInstruction_opcode = This->m_currentInstruction->opcode;
                    opcode = 0; // Nop

                    if ((m_currentInstruction_opcode == 64 && This->m_pendingInterrupt == This->m_currentInstruction->args[0]) || m_currentInstruction_opcode == -1)
                        break;

                    if (m_currentInstruction_opcode == 64 && This->m_currentInstruction->args[0] == -1)
                        instr_interrupt = This->m_currentInstruction;

                    This->loadNextInstruction();
                }

                if (This->m_currentInstruction->opcode == 64 || (This->m_currentInstruction = instr_interrupt, instr_interrupt != nullptr))
                {
                    This->m_interruptReturnTime.m_previous      = This->m_timeInScript.m_previous;
                    This->m_interruptReturnTime.m_current       = This->m_timeInScript.m_current;
                    This->m_interruptReturnTime.m_currentF      = This->m_timeInScript.m_currentF;
                    This->m_interruptReturnTime.m_gameSpeed     = This->m_timeInScript.m_gameSpeed;
                    This->m_interruptReturnTime.m_isInitialized = This->m_timeInScript.m_isInitialized;
                    This->m_interruptReturnInstr                = This->m_currentInstruction;
                    This->m_timeInScript.setCurrent(This->m_currentInstruction->time);

                    This->m_pendingInterrupt = This->m_currentInstruction->offsetToNextInstr; //TODO: verify???

                    This->m_flagsLow |= 1;
                    This->loadNextInstruction();
                    break;
                }
            }
            This->m_timeInScript.addf(-1.0);

            //goto Idk_Why_I_Must_Jump_0044c2ae;

            // Add vel to the texture u coordinate every frame (in units of 1 / total_image_width), causing the displayed sprite to scroll horizontally through the image file.
        case 70: // scrollX(float vel)

            break;

            // Add vel to the texture v coordinate every frame (in units of 1 / total_image_height), causing the displayed sprite to scroll vertically through the image file.
        case 71: // scrollY(float vel)

            break;

            // Set the visibility flag (1 = visible). Invisible graphics are skipped during rendering.
            // Generally speaking this is not a huge deal as the most expensive parts of rendering are typically skipped anyways whenever alpha and alpha2 are both 0.
        case 72: // visible(byte visible)

            break;

            // If disable is 1, writing to the z-buffer is disabled. Otherwise, it is enabled. This can matter in cases where z-testing is used to filter writes.
        case 73: // zWriteDisable(int disable)

            break;

            // Sets the state of a STD-related bitflag. When this flag is enabled, some unknown vector from the stage background camera data is added to some poorly-understood position-related vector on the ANM, for an even more unknown purpose.
        case 74: // ins_74(int enable)
            // TODO: Implement STD-related bitflag logic
            break;

            // Wait t frames.
            // StB and earlier: wait is implemented using a dedicated timer.
            // MoF and later: Subtracts t from the current time before executing the next instruction.
        case 75: // wait(int t)

            break;

            // Set a second color for gradients. Gradients are used by certain special drawing instructions, and can be enabled on regular sprites using colorMode.
        case 76: // color2(int r, int g, int b)

            break;

            // Set a second alpha for gradients. Gradients are used by certain special drawing instructions, and can be enabled on regular sprites using colorMode.
        case 77: // alpha2(int alpha)

            break;

            // Over the next t frames, changes color2 to the given value using interpolation mode mode.
            // For some reason, in DDC onwards, this also sets colorMode to 1, which can be a mild inconvenience.
        case 78: // color2Time(int t, int mode, int r, int g, int b)
            break;

            // Over the next t frames, changes alpha2 to the given value using interpolation mode mode.
            // For some reason, in DDC onwards, this also sets colorMode to 1, which can be a mild inconvenience.
        case 79: // alpha2Time(int t, int mode, int alpha)

            break;

            // Lets you enable gradients on regular sprites.
            // 0: Only use color and alpha.
            // 1: Only use color2 and alpha2.
            // 2: (DS-) Horizontal gradient.
            // 3: (DS-) Vertical gradient.
        case 80: // colorMode(int mode)
            break;

            // Can be used at the end of a interruptLabel block to return back to the moment just before the VM received the interrupt.
            // This is not the only way to end a interruptLabel block; oftentimes the game may use a stop instead.
        case 81: // caseReturn
            break;

            // Placeholder for rotateAuto(byte mode)
        case 82: // rotateAuto
            // TODO: Implement rotateAuto logic
            break;

            // Placeholder for ins_83()
        case 83: // ins_83
            // TODO: Unknown instruction
            break;

            // Placeholder for texCircle(int nmax)
        case 84: // texCircle
            // TODO: Implement texCircle logic
            break;

            // Placeholder for ins_85(int enable)
        case 85: // ins_85
            // TODO: Unknown instruction
            break;

            // Placeholder for slowdownImmune(int enable)
        case 86: // slowdownImmune
            // TODO: Implement slowdownImmune logic
            break;

            // Placeholder for randMode(int mode)
        case 87: // randMode
            // TODO: Implement randMode logic
            break;

            // Placeholder for scriptNew(int script)
        case 88: // scriptNew
            // TODO: Implement script creation (calls 0x455a00)
            break;

            // Placeholder for resampleMode(int n)
        case 89: // resampleMode
            // TODO: Implement resampleMode logic
            break;

            // Placeholder for scriptNewUI(int script)
        case 90: // scriptNewUI
            // TODO: Implement scriptNewUI logic
            break;

            // Placeholder for scriptNewFront(int script)
        case 91: // scriptNewFront
            // TODO: Implement scriptNewFront logic
            break;

            // Placeholder for scriptNewUIFront(int script)
        case 92: // scriptNewUIFront
            // TODO: Implement scriptNewUIFront logic
            break;

            // Placeholder for scrollXTime(int t, int mode, float vel)
        case 93: // scrollXTime
            // TODO: Implement scrollXTime logic
            break;

            // Placeholder for scrollYTime(int t, int mode, float vel)
        case 94: // scrollYTime
            // TODO: Implement scrollYTime logic
            break;

            // Placeholder for scriptNewRoot(int script)
        case 95: // scriptNewRoot
            // TODO: Implement scriptNewRoot logic
            break;

            // Placeholder for scriptNewPos(int script, float x, float y)
        case 96: // scriptNewPos
            // TODO: Implement scriptNewPos logic
            break;

            // Placeholder for scriptNewRootPos(int script, float x, float y)
        case 97: // scriptNewRootPos
            // TODO: Implement scriptNewRootPos logic
            break;

            // Placeholder for ins_98()
        case 98: // ins_98
            // TODO: Unknown instruction
            break;

            // Placeholder for ins_99(int enable)
        case 99: // ins_99
            // TODO: Unknown instruction
            break;

            // Placeholder for moveBezier(int t, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3)
        case 100: // moveBezier
            // TODO: Implement Bezier curve movement
            break;

            // Placeholder for ins_101()
        case 101: // ins_101
            // TODO: Unknown instruction
            break;

            // Placeholder for spriteRand(int a, int b)
        case 102: // spriteRand
            // TODO: Implement spriteRand logic
            break;

            // Placeholder for drawRect(float w, float h)
        case 103: // drawRect
            // TODO: Implement drawRect logic
            break;

        default:
            // Unknown opcode
            break;
        }
    }
#endif
}