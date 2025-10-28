#include "AnmVm.h"

/* Global Variables */

RngContext g_anmRngContext;
RngContext g_replayRngContext;
D3DXVECTOR3 g_bottomLeftDrawCorner;
D3DXVECTOR3 g_bottomRightDrawCorner;
D3DXVECTOR3 g_topLeftDrawCorner;
D3DXVECTOR3 g_topRightDrawCorner;

AnmVm::AnmVm()
{
    m_spriteNumber = -1;
}

void AnmVm::initialize()
{
    D3DXVECTOR3 entityPos = m_entityPos;
    int layer = m_layer;
    memset(this,0,0x434);

    m_scale.x = 1.0;
    m_scale.y = 1.0;
    m_entityPos = entityPos;
    m_layer = layer;
    m_color0 = 0xffffffff;
    D3DXMatrixIdentity(&m_baseScaleMatrix);
    m_flagsLow = 7;
    m_timeInScript.m_current = 0;
    m_timeInScript.m_currentF = 0.0;
    m_timeInScript.m_previous = -999999;
    m_posInterp.endTime = 0;
    m_rgbInterp.endTime = 0;
    m_alphaInterp.endTime = 0;
    m_rotationInterp.endTime = 0;
    m_scaleInterp.endTime = 0;
    m_rgb2Interp.endTime = 0;
    m_alpha2Interp.endTime = 0;
    m_uVelInterp.endTime = 0;
    m_vVelInterp.endTime = 0;
    m_nodeInGlobalList.next = nullptr;
    m_nodeInGlobalList.prev = nullptr;
    m_nodeInGlobalList.entry = this;
    m_nodeAsFamilyMember.next = nullptr;
    m_nodeAsFamilyMember.prev = nullptr;
    m_nodeAsFamilyMember.entry = this;
}

int AnmVm::setupTextureQuadAndMatrices(uint32_t spriteNumber, AnmLoaded* anmLoaded)
{
    if (anmLoaded->header == nullptr || anmLoaded->anmsLoading != 0)
        return -1;

    m_spriteNumber = static_cast<uint16_t>(spriteNumber);
    m_anmLoaded = anmLoaded;

    AnmLoadedSprite* sprite = &anmLoaded->keyframeData[spriteNumber];
    m_sprite = sprite;

    m_spriteUvQuad[0].x = sprite->uvStart.x;
    m_spriteUvQuad[0].y = sprite->uvStart.y;
    m_spriteUvQuad[1].x = sprite->uvEnd.x;
    m_spriteUvQuad[1].y = sprite->uvStart.y;
    m_spriteUvQuad[2].x = sprite->uvEnd.x;
    m_spriteUvQuad[2].y = sprite->uvEnd.y;
    m_spriteUvQuad[3].x = sprite->uvStart.x;
    m_spriteUvQuad[3].y = sprite->uvEnd.y;

    m_spriteSize.x = sprite->spriteWidth;
    m_spriteSize.y = sprite->spriteHeight;

    D3DXMatrixIdentity(&m_baseScaleMatrix);
    D3DXMatrixIdentity(&m_matrix37c);

    m_baseScaleMatrix._11 = m_spriteSize.x * (1.f / 256.f);
    m_baseScaleMatrix._22 = m_spriteSize.y * (1.f / 256.f);

    m_localTransformMatrix = m_baseScaleMatrix;

    m_matrix37c._11 = (m_spriteSize.x / sprite->bitmapWidth) * sprite->maybeScale.x;
    m_matrix37c._22 = (m_spriteSize.y / sprite->bitmapHeight) * sprite->maybeScale.y;

    return 0;
}

// 0x44b2a0
int AnmVm::getIntVar(int id)
{  
    switch(id)
    {
    case 10000:
        return m_intVars[0];
    case 10001:
        return m_intVars[1];
    case 10002:
        return m_intVars[2];
    case 10003:
        return m_intVars[3];
    case 10004:
        return static_cast<float>(m_floatVars[0]);
    case 10005:
        return static_cast<float>(m_floatVars[1]);
    case 10006:
        return static_cast<float>(m_floatVars[2]);
    case 10007:
        return static_cast<float>(m_floatVars[3]);
    case 10008:
        return m_intVar8;
    case 10009:
        return m_intVar9;
    case 10022:
    RngContext* ctx = &g_anmRngContext;
    if ((m_flagsLow & 0x40000000) == 0)
      ctx = &g_replayRngContext; // One of these is ANM RNG, while the other is replay RNG
    return rng(ctx);
  }
  return id;
}

// 0x44b400
int* AnmVm::getIntVarPtr(int* id)
{
    switch(*id)
    {
    case 10000:
        return &m_intVars[0];
    case 10001:
        return &m_intVars[1];
    case 10002:
        return &m_intVars[2];
    case 10003:
        return &m_intVars[3];
    case 10008:
        return &m_intVar8;
    case 10009:
        return &m_intVar9;
    }
    return id;
}

// 0x44b380
float* AnmVm::getFloatVarPtr(float* id)
{
    int intTime = static_cast<float>(*id); 
    switch(intTime)
    {
    case 10004: // 0x2714
        return &m_floatVars[0];
    case 10005: // 0x2715
        return &m_floatVars[1];
    case 10006: // 0x2716
        return &m_floatVars[2];
    case 10007: // 0x2717
        return &m_floatVars[3];
    case 10013: // 0x271d
        return &m_pos.x;
    case 10014: // 0x271e
        return &m_pos.y;
    case 10015: // 0x271f
        return &m_pos.z;
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
    float fRng = rng;

    // Check for overflow
    if (rng < 0)
        fRng += (1ULL << 32);

    return (fRng * (1.0 / (1ULL << 32)) - 1.0) * angle;
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

    return fRng * (1.0 / (1ULL << 32));
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

    return fRng * (1.0 / (1ULL << 31)) - 1.0;
}

// 0x44b080
float AnmVm::getFloatVar(float id)
{
    RngContext* rngContext;
    uint32_t rngValue;
    int roundedId = static_cast<float>(id);
    int index = roundedId - 10000;
    float fRng;

    switch(index)
    {
    case 0:
        return m_intVars[0];
    case 1:
        return m_intVars[1];
    case 2:
        return m_intVars[2];
    case 3:
        return m_intVars[3];
    case 4:
        return m_floatVars[0];
    case 5:
        return m_floatVars[1];
    case 6:
        return m_floatVars[2];
    case 7:
        return m_floatVars[3];
    case 8:
        return m_intVar8;
    case 9:
        return m_intVar9;
    case 10:
        rngContext = (m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeToAngle(3.1415927f, rngContext);
    case 11:
        rngContext = (m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeUnsigned(rngContext);
    case 12:
        rngContext = (m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
        return normalizeSigned(rngContext);
    case 13:
        return m_pos.x;
    case 14:
        return m_pos.y;
    case 15:
        return m_pos.z;
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
        rngContext = (m_flagsLow & 0x40000000) ? &g_anmRngContext : &g_replayRngContext;
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
void AnmVm::writeSpriteCharacters(D3DXVECTOR3* topLeft, D3DXVECTOR3* bottomLeft, D3DXVECTOR3* topRight, D3DXVECTOR3* bottomRight)
{
    D3DXVECTOR3 effectivePos = m_entityPos + m_pos + m_offsetPos;
    float width = m_spriteSize.x * m_scale.x;
    float height = m_spriteSize.y * m_scale.y;

    uint32_t hAlign = (m_flagsLow >> 0x12) & 3;
    if (hAlign == 0)
    {
        float leftX = effectivePos.x - width * 0.5f;
        topRight->x = leftX;
        bottomLeft->x = leftX;
        float rightX = leftX + width;
        topLeft->x = rightX;
        bottomRight->x = rightX;
    }
    else if (hAlign == 1)
    {
        float leftX = effectivePos.x;
        topRight->x = leftX;
        bottomLeft->x = leftX;
        float rightX = effectivePos.x + width;
        topLeft->x = rightX;
        bottomRight->x = rightX;
    }
    else if (hAlign == 2)
    {
        float leftX = effectivePos.x - width;
        topRight->x = leftX;
        bottomLeft->x = leftX;
        float rightX = effectivePos.x;
        topLeft->x = rightX;
        bottomRight->x = rightX;
    }
    // Note: If hAlign == 3, x-coordinates are not set (matching original behavior)

    uint32_t vAlign = (m_flagsLow >> 0x14) & 3;
    if (vAlign == 0)
    {
        float bottomY = effectivePos.y - height * 0.5f;
        bottomRight->y = bottomY;
        bottomLeft->y = bottomY;
        float topY = bottomY + height;
        topLeft->y = topY;
        topRight->y = topY;
    }
    else if (vAlign == 1)
    {
        float bottomY = effectivePos.y;
        bottomRight->y = bottomY;
        bottomLeft->y = bottomY;
        float topY = effectivePos.y + height;
        topLeft->y = topY;
        topRight->y = topY;
    }
    else if (vAlign == 2)
    {
        float bottomY = effectivePos.y - height;
        bottomRight->y = bottomY;
        bottomLeft->y = bottomY;
        float topY = effectivePos.y;
        topLeft->y = topY;
        topRight->y = topY;
    }
    // Note: If vAlign == 3, y-coordinates are not set (matching original behavior)

    float effectiveZ = effectivePos.z;
    topLeft->z = effectiveZ;
    topRight->z = effectiveZ;
    bottomRight->z = effectiveZ;
    bottomLeft->z = effectiveZ;
}

// 0x44fe30
void AnmVm::writeSpriteCharactersWithoutRot(D3DXVECTOR3* bottomLeft, D3DXVECTOR3* bottomRight, D3DXVECTOR3* topRight, D3DXVECTOR3* topLeft)
{
    D3DXVECTOR3 effectivePos = m_entityPos + m_pos + m_offsetPos;
    float width = m_spriteSize.x * m_scale.x;
    float height = m_spriteSize.y * m_scale.y;

    uint32_t hAlign = (m_flagsLow >> 0x12) & 3;
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
    topRight->x = leftX;
    bottomLeft->x = leftX;
    topLeft->x = rightX;
    bottomRight->x = rightX;

    uint32_t vAlign = (m_flagsLow >> 0x14) & 3;
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
    } else
        return;  // y not set for mode 3

    bottomRight->y = bottomY;
    bottomLeft->y = bottomY;
    topLeft->y = topY;
    topRight->y = topY;

    float effectiveZ = effectivePos.z;
    topLeft->z = effectiveZ;
    topRight->z = effectiveZ;
    bottomRight->z = effectiveZ;
    bottomLeft->z = effectiveZ;
}

// 0x4503d0
void AnmVm::applyZRotationToQuadCorners(D3DXVECTOR3* bottomLeft, D3DXVECTOR3* bottomRight, D3DXVECTOR3* topRight, D3DXVECTOR3* topLeft)
{
    D3DXVECTOR3 effectivePos = m_entityPos + m_pos + m_offsetPos;
    float width = m_spriteSize.x * m_scale.x;
    float height = m_spriteSize.y * m_scale.y;

    uint32_t hAlign = (m_flagsLow >> 0x12) & 3;
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

    uint32_t vAlign = (m_flagsLow >> 0x14) & 3;
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

    float rotZ = m_rotation.z;
    float cosZ = cosf(rotZ);
    float sinZ = sinf(rotZ);

    // Bottom-Left corner
    bottomLeft->x = effectivePos.x + (cosZ * leftX - sinZ * bottomY);
    bottomLeft->y = effectivePos.y + (cosZ * bottomY + sinZ * leftX);

    // Bottom-Right corner
    bottomRight->x = effectivePos.x + (cosZ * rightX - sinZ * bottomY);
    bottomRight->y = effectivePos.y + (cosZ * bottomY + sinZ * rightX);

    // Top-Left corner
    topLeft->x = effectivePos.x + (cosZ * leftX - sinZ * topY);
    topLeft->y = effectivePos.y + (cosZ * topY + sinZ * leftX);

    // Top-Right corner
    topRight->x = effectivePos.x + (cosZ * rightX - sinZ * topY);
    topRight->y = effectivePos.y + (cosZ * topY + sinZ * rightX);

    float effectiveZ = effectivePos.z;
    topRight->z = effectiveZ;
    topLeft->z = effectiveZ;
    bottomRight->z = effectiveZ;
    bottomLeft->z = effectiveZ;
}

// 0x450700
int AnmVm::projectQuadCornersThroughCameraViewport()
{
    float rotZ = m_rotation.z;
    float cosZ = cosf(rotZ);
    float sinZ = sinf(rotZ);

    D3DXVECTOR3 localOrigin = { 0.0f, 0.0f, 0.0f };
    D3DXMATRIX worldMatrix;
    // Initialize worldMatrix to identity
    memset(&worldMatrix, 0, sizeof(worldMatrix));
    worldMatrix._11 = 1.0f;
    worldMatrix._22 = 1.0f;
    worldMatrix._33 = 1.0f;
    worldMatrix._44 = 1.0f;
    // Set translation
    worldMatrix._41 = m_entityPos.x + m_pos.x + m_offsetPos.x;
    worldMatrix._42 = m_entityPos.y + m_pos.y + m_offsetPos.y;
    worldMatrix._43 = m_entityPos.z + m_pos.z + m_offsetPos.z;

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

    float width = m_scale.x * scaleFactor * m_spriteSize.x;
    float height = m_scale.y * scaleFactor * m_spriteSize.y;

    float offsetXBottomLeft, offsetXBottomRight, offsetXTopLeft, offsetXTopRight;
    float offsetYBottomLeft, offsetYBottomRight, offsetYTopLeft, offsetYTopRight;

    uint32_t anchorX = (m_flagsLow >> 18) & 3;
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

    uint32_t anchorY = (m_flagsLow >> 20) & 3;
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

    g_bottomLeftDrawCorner.x = screenPos.x + (cosZ * offsetXBottomLeft - sinZ * offsetYBottomLeft);
    g_bottomLeftDrawCorner.y = screenPos.y + (cosZ * offsetYBottomLeft + sinZ * offsetXBottomLeft);
    g_bottomLeftDrawCorner.z = screenPos.z;

    g_bottomRightDrawCorner.x = screenPos.x + (cosZ * offsetXBottomRight - sinZ * offsetYBottomRight);
    g_bottomRightDrawCorner.y = screenPos.y + (cosZ * offsetYBottomRight + sinZ * offsetXBottomRight);
    g_bottomRightDrawCorner.z = screenPos.z;

    g_topLeftDrawCorner.x = screenPos.x + (cosZ * offsetXTopLeft - sinZ * offsetYTopLeft);
    g_topLeftDrawCorner.y = screenPos.y + (cosZ * offsetYTopLeft + sinZ * offsetXTopLeft);
    g_topLeftDrawCorner.z = screenPos.z;

    g_topRightDrawCorner.x = screenPos.x + (cosZ * offsetXTopRight - sinZ * offsetYTopRight);
    g_topRightDrawCorner.y = screenPos.y + (cosZ * offsetYTopRight + sinZ * offsetXTopRight);
    g_topRightDrawCorner.z = screenPos.z;
    return 0;
}

// 0x44b4b0
void AnmVm::run()
{
    while (m_currentInstruction != nullptr)
    {
        if (m_flagsLow & 0x20000)
            return;
        
        float gameSpeed = g_gameSpeed;
        g_gameSpeed = 1.0;

        AnmRawInstruction* cur_instr = m_currentInstruction;
        AnmRawInstruction* instr_interrupt = nullptr;

        //TODO: Move local variables into inner scopes if possible
        uint32_t spriteNumber;
        uint32_t arg0_1;
        int* intVarPtr;
        int* interruptInstrVar;

        if (m_pendingInterrupt != 0)
            goto Interrupt;

        if (m_currentInstruction->time <= m_timeInScript.m_current)
        {
            // Opcode is adjusted in the actual binary for the jump table, where each case is 1 higher than the actual opcode.
            // This reconstruction will be following the correct values.
            uint32_t opcode  = cur_instr->opcode; // + 1;
            switch (opcode)
            {
                // Does nothing.
                case 0: // nop
                    break;

                // Destroys the graphic.
                case 1: // delete
                    m_flagsLow &= ~1; // Clear visibility
                    m_currentInstruction = nullptr; // Terminate script
                    g_gameSpeed = gameSpeed;
                    break;

                // Freezes the graphic until it is destroyed externally.
                // Any interpolation instructions like posTime will no longer advance, and interrupts are disabled.
                case 2: // static
                    m_currentInstruction = nullptr; // Terminate script
                    g_gameSpeed = gameSpeed;
                    break;

                // Sets the image used by this VM to one of the sprites defined in the ANM file.
                // A value of -1 means to not use an image (this is frequently used with special drawing instructions).
                // Thanm also lets you use the sprite's name instead of an index.
                // Under some unknown conditions, these sprite indices are transformed by a "sprite-mapping" function; e.g. many bullet scripts use false indices, presumably to avoid repeating the same script for 16 different colors. The precise mechanism of this is not yet fully understood.
                case 3: // sprite(int id)
                    m_flagsLow |= 1; // Set visibility

                    if (m_spriteMappingFunc == nullptr)
                    {
                        spriteNumber = m_currentInstruction->args[0];
                        if ((m_currentInstruction->varMask & 1) != 0)
                            spriteNumber = getIntVar(spriteNumber);
                    }

                    else
                    {
                        arg0_1 = m_currentInstruction->args[0];
                        if ((m_currentInstruction->varMask & 1) != 0)
                            arg0_1 = getIntVar(arg0_1);
                        spriteNumber = (uint32_t) m_spriteMappingFunc(this, arg0_1);
                    }
                    setupTextureQuadAndMatrices(spriteNumber,m_anmLoaded);
                    break;

                // Jumps to byte offset dest from the script's beginning and sets the time to t. thanm accepts a label name for dest.
                // Chinese wiki says some confusing recommendation about setting a=0, can someone explain to me?
                case 4: // jmp(int dest, int t)
                    m_timeInScript.setCurrent(cur_instr->args[1]);
                    m_currentInstruction = (AnmRawInstruction*)((char*)m_beginningOfScript + cur_instr->args[0]);
                    break;

                // Decrement x and then jump if x > 0. You can use this to repeat a loop a fixed number of times.
                case 5: // jmpDec(int& x, int dest, int t)
                    intVarPtr = cur_instr->args; // Iterator
                    if (cur_instr->varMask & 1)
                        intVarPtr = getIntVarPtr(cur_instr->args);
                    *intVarPtr -= 1;
                    opcode = *cur_instr->args;

                    if (cur_instr->varMask & 1)
                        opcode = getIntVar(opcode);

                    if (opcode > 0)
                    {
                        m_timeInScript.setCurrent(cur_instr->args[2]);
                        m_currentInstruction = (AnmRawInstruction*)((char*)cur_instr->args + cur_instr->offsetToNextInstr);
                    }
                    break;

                // Does a = b.
                case 6: // iset(int& a, int b)
                    if ((cur_instr->varMask & 2) == 0)
                        instr_interrupt = (AnmRawInstruction*)cur_instr->args[1];
                    else
                        instr_interrupt = (AnmRawInstruction*)getIntVar(cur_instr->args[1]);

                    // goto HelpMe inlined
                    if ((cur_instr->varMask & 1) == 0)
                    {
                        *cur_instr->args = (int)instr_interrupt;
                        m_currentInstruction = (AnmRawInstruction*)((char*)cur_instr->args + cur_instr->offsetToNextInstr);
                        break;
                    }

                    interruptInstrVar = getIntVarPtr(cur_instr->args);
                    m_currentInstruction = (AnmRawInstruction*)((char*)cur_instr->args + cur_instr->offsetToNextInstr);
                    break;

                // Does a = b.
                case 7: // fset(float& a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) = getFloatArg(this, cur_instr, 1);
                    break;

                // Does a += b.
                case 8: // iadd(int& a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) += getIntArg(this, cur_instr, 1);
                    break;

                // Does a += b.
                case 9: // fadd(float& a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) += getFloatArg(this, cur_instr, 1);
                    break;

                // Does a -= b.
                case 10: // isub(int& a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) -= getIntArg(this, cur_instr, 1);
                    break;

                // Does a -= b.
                case 11: // fsub(float& a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) -= getFloatArg(this, cur_instr, 1);
                    break;

                // Does a *= b.
                case 12: // imul(int& a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) *= getIntArg(this, cur_instr, 1);
                    break;

                // Does a *= b.
                case 13: // fmul(float& a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) *= getFloatArg(this, cur_instr, 1);
                    break;

                // Does a /= b.
                case 14: // idiv(int& a, int b)
                    {
                        int b = getIntArg(this, cur_instr, 1);
                        if (b != 0) *getIntVarPtr(this, cur_instr->args[0]) /= b;
                    }
                    break;

                // Does a /= b.
                case 15: // fdiv(float& a, float b)
                    {
                        float b = getFloatArg(this, cur_instr, 1);
                        if (b != 0.0f) *getFloatVarPtr(this, cur_instr->args[0]) /= b;
                    }
                    break;

                // Does a %= b.
                case 16: // imod(int& a, int b)
                    {
                        int b = getIntArg(this, cur_instr, 1);
                        if (b != 0) *getIntVarPtr(this, cur_instr->args[0]) %= b;
                    }
                    break;

                // Does a %= b.
                case 17: // fmod(float& a, float b)
                    {
                        float b = getFloatArg(this, cur_instr, 1);
                        if (b != 0.0f) *getFloatVarPtr(this, cur_instr->args[0]) = fmodf(*getFloatVarPtr(this, cur_instr->args[0]), b);
                    }
                    break;

                // Does a = b + c.
                case 18: // isetAdd(int& x, int a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) = getIntArg(this, cur_instr, 1) + getIntArg(this, cur_instr, 2);
                    break;

                // Does a = b + c.
                case 19: // fsetAdd(float& x, float a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) = getFloatArg(this, cur_instr, 1) + getFloatArg(this, cur_instr, 2);
                    break;

                // Does a = b - c.
                case 20: // isetSub(int& x, int a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) = getIntArg(this, cur_instr, 1) - getIntArg(this, cur_instr, 2);
                    break;

                // Does a = b - c.
                case 21: // fsetSub(float& x, float a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) = getFloatArg(this, cur_instr, 1) - getFloatArg(this, cur_instr, 2);
                    break;

                // Does a = b * c.
                case 22: // isetMul(int& x, int a, int b)
                    *getIntVarPtr(this, cur_instr->args[0]) = getIntArg(this, cur_instr, 1) * getIntArg(this, cur_instr, 2);
                    break;

                // Does a = b * c.
                case 23: // fsetMul(float& x, float a, float b)
                    *getFloatVarPtr(this, cur_instr->args[0]) = getFloatArg(this, cur_instr, 1) * getFloatArg(this, cur_instr, 2);
                    break;

                // Does a = b / c.
                case 24: // isetDiv(int& x, int a, int b)
                    int b = getIntArg(this, cur_instr, 2);
                    if (b != 0) 
                        *getIntVarPtr(this, cur_instr->args[0]) = getIntArg(this, cur_instr, 1) / b;
                    break;

                // Does a = b / c.
                case 25: // fsetDiv(float& x, float a, float b)
                    float b = getFloatArg(this, cur_instr, 2);
                    if (b != 0.0f)
                        *getFloatVarPtr(this, cur_instr->args[0]) = getFloatArg(this, cur_instr, 1) / b;
                    break;

                // Does a = b % c.
                case 26: // isetMod(int& x, int a, int b)
                    int b = getIntArg(this, cur_instr, 2);
                    if (b != 0)
                        *getIntVarPtr(this, cur_instr->args[0]) = getIntArg(this, cur_instr, 1) % b;
                    break;

                // Does a = b % c.
                case 27: // fsetMod(float& x, float a, float b)
                    float b = getFloatArg(this, cur_instr, 2);
                    if (b != 0.0f)
                        *getFloatVarPtr(this, cur_instr->args[0]) = fmodf(getFloatArg(this, cur_instr, 1), b);
                    break;

                // Jumps if a == b.
                case 28: // ije(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) == getIntArg(this, cur_instr, 1))
                    {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a == b.
                case 29: // fje(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) == getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a != b.
                case 30: // ijne(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) != getIntArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a != b.
                case 31: // fjne(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) != getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a < b.
                case 32: // ijl(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) < getIntArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a < b.
                case 33: // fjl(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) < getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a <= b.
                case 34: // ijle(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) <= getIntArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a <= b.
                case 35: // fjle(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) <= getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a > b.
                case 36: // ijg(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) > getIntArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a > b.
                case 37: // fjg(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) > getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a >= b.
                case 38: // ijge(int a, int b, int dest, int t)
                    if (getIntArg(this, cur_instr, 0) >= getIntArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Jumps if a >= b.
                case 39: // fjge(float a, float b, int dest, int t)
                    if (getFloatArg(this, cur_instr, 0) >= getFloatArg(this, cur_instr, 1)) {
                        currentInstruction = (AnmRawInstruction*)((char*)beginningOfScript + getIntArg(this, instr, 2));
                        timeInScript.setCurrent(getIntArg(this, instr, 3));
                    }
                    break;

                // Draw a random integer 0 <= x < n using the animation RNG.
                case 40: // isetRand(int& x, int n)
                    *getIntVarPtr(this, cur_instr->args[0]) = randInt(0, getIntArg(this, cur_instr, 1)); // Placeholder randInt
                    break;

                // Draw a random float 0 <= x <= r using the animation RNG.
                case 41: // fsetRand(float& x, float r)
                    *getFloatVarPtr(this, cur_instr->args[0]) = randFloat(0.0f, getFloatArg(this, cur_instr, 1)); // Placeholder randFloat
                    break;

                // Compute sin(θ) (θ in radians).
                case 42: // fsin(float& dest, float θ)
                    *getFloatVarPtr(this, cur_instr->args[0]) = sinf(getFloatArg(this, cur_instr, 1));
                    break;

                // Compute cos(θ) (θ in radians).
                case 43: // fcos(float& dest, float θ)
                    *getFloatVarPtr(this, cur_instr->args[0]) = cosf(getFloatArg(this, cur_instr, 1));
                    break;

                // Compute tan(θ) (θ in radians).
                case 44: // ftan(float& dest, float θ)
                    *getFloatVarPtr(this, cur_instr->args[0]) = tanf(getFloatArg(this, cur_instr, 1));
                    break;

                // Compute acos(x) (output in radians).
                case 45: // facos(float& dest, float x)
                    *getFloatVarPtr(this, cur_instr->args[0]) = acosf(getFloatArg(this, cur_instr, 1));
                    break;

                // Compute atan(m) (output in radians).
                case 46: // fatan(float& dest, float m)
                    *getFloatVarPtr(this, cur_instr->args[0]) = atanf(getFloatArg(this, cur_instr, 1));
                    break;

                // Reduce an angle modulo 2*PI into the range [-PI, +PI].
                case 47: // validRad(float& θ)
                    {
                        float* theta = getFloatVarPtr(this, cur_instr->args[0]);
                        *theta = fmodf(*theta + M_PI, 2.0f * M_PI) - M_PI;
                    }
                    break;

                // Sets the position of the graphic.
                // If you look in the code, you'll see that if a certain bitflag is set, this will write to a different variable. This is part of the implementation of th09:posMode in earlier games, and is, to my knowledge, entirely dead code in every game since StB.
                case 48: // pos(float x, float y, float z)
                    pos.x = getFloatArg(this, instr, 0);
                    pos.y = getFloatArg(this, instr, 1);
                    pos.z = getFloatArg(this, instr, 2);
                    break;

                // Set the graphic's rotation. For 2D objects, only the z rotation matters.
                // In some rare cases, x rotation has a special meaning for special drawing instructions. Graphics rotate around their anchor point (see anchor).
                // A positive angle around the z-axis goes clockwise from the +x direction towards the +y direction (defined to point down). 3D rotations are performed as follows:
                // (EoSD) Rotate first around x, then around y, then around z.
                // (PCB–GFW) Haven't checked. Probably the same?
                // (TD–) You can choose the rotation system with th185:rotationMode. (what's the default? probably the same?)
                // If nothing seems to be happening when you call this, check your type setting!
                case 49: // rotate(float rx, float ry, float rz)
                    rotation.x = getFloatArg(this, instr, 0);
                    rotation.y = getFloatArg(this, instr, 1);
                    rotation.z = getFloatArg(this, instr, 2);
                    break;

                // Scales the ANM independently along the x and y axis. Graphics grow around their anchor point (see anchor). Some special drawing instructions give the x and y scales special meaning.
                case 50: // scale(float sx, float sy)
                    scale.x = getFloatArg(this, instr, 0);
                    scale.y = getFloatArg(this, instr, 1);
                    break;

                // Set alpha (opacity) to a value 0-255.
                case 51: // alpha(int alpha)
                    alpha = getIntArg(this, instr, 0);
                    break;

                // Set a color which gets blended with this sprite. Blend operation is multiply, so setting white (255, 255, 255) eliminates the effect.
                case 52: // color(int r, int g, int b)
                    color.r = getIntArg(this, instr, 0);
                    color.g = getIntArg(this, instr, 1);
                    color.b = getIntArg(this, instr, 2);
                    break;

                // Set a constant angular velocity, in radians per frame.
                case 53: // angleVel(float ωx, float ωy, float ωz)
                    angularVelocity.x = getFloatArg(this, instr, 0);
                    angularVelocity.y = getFloatArg(this, instr, 1);
                    angularVelocity.z = getFloatArg(this, instr, 2);
                    break;

                // Every frame, it increases the values of scale as sx -> sx + gx and sy -> sy + gy. Basically, scaleGrowth is to scale as angleVel is to rotate. (they even share implemenation details...)
                case 54: // scaleGrowth(float gx, float gy)
                    scaleGrowth.x = getFloatArg(this, instr, 0);
                    scaleGrowth.y = getFloatArg(this, instr, 1);
                    break;

                // Obsolete. Use alphaTime instead.
                // UNTESTED: Linearly changes alpha to alpha over the next t frames. Identical to calling alphaTime(t, 0, alpha).
                case 55: // alphaTimeLinear(int alpha, int t)
                    alphaInterp.m_endTime = getIntArg(this, instr, 1);
                    alphaInterp.m_method = 0; // Linear
                    alphaInterp.m_initial = alpha;
                    alphaInterp.m_goal = getIntArg(this, instr, 0);
                    alphaInterp.m_timer.setCurrent(0);
                    break;

                // Over the next t frames, changes pos to the given values using interpolation mode mode.
                case 56: // posTime(int t, int mode, float x, float y, float z)
                    posInterp.m_endTime = getIntArg(this, instr, 0);
                    posInterp.m_method = getIntArg(this, instr, 1);
                    posInterp.m_goal.x = getFloatArg(this, instr, 2);
                    posInterp.m_goal.y = getFloatArg(this, instr, 3);
                    posInterp.m_goal.z = getFloatArg(this, instr, 4);
                    posInterp.m_initial = pos;
                    posInterp.m_timer.setCurrent(0);
                    break;

                // Over the next t frames, changes color to the given value using interpolation mode mode.
                case 57: // colorTime(int t, int mode, int r, int g, int b)
                    colorInterp.m_endTime = getIntArg(this, instr, 0);
                    colorInterp.m_method = getIntArg(this, instr, 1);
                    colorInterp.m_goal.r = getIntArg(this, instr, 2);
                    colorInterp.m_goal.g = getIntArg(this, instr, 3);
                    colorInterp.m_goal.b = getIntArg(this, instr, 4);
                    colorInterp.m_initial = color;
                    colorInterp.m_timer.setCurrent(0);
                    break;

                // Over the next t frames, changes alpha to the given values using interpolation mode mode.
                case 58: // alphaTime(int t, int mode, int alpha)
                    alphaInterp.m_endTime = getIntArg(this, instr, 0);
                    alphaInterp.m_method = getIntArg(this, instr, 1);
                    alphaInterp.m_goal = getIntArg(this, instr, 2);
                    alphaInterp.m_initial = alpha;
                    alphaInterp.m_timer.setCurrent(0);
                    break;

                // Over the next t frames, changes rotate to the given values using interpolation mode mode.
                case 59: // rotateTime(int t, int mode, float rx, float ry, float rz)
                    rotationInterp.m_endTime = getIntArg(this, instr, 0);
                    rotationInterp.m_method = getIntArg(this, instr, 1);
                    rotationInterp.m_goal.x = getFloatArg(this, instr, 2);
                    rotationInterp.m_goal.y = getFloatArg(this, instr, 3);
                    rotationInterp.m_goal.z = getFloatArg(this, instr, 4);
                    rotationInterp.m_initial = rotation;
                    rotationInterp.m_timer.setCurrent(0);
                    break;

                // Over the next t frames, changes scale to the given values using interpolation mode mode.
                case 60: // scaleTime(int t, int mode, float sx, float sy)
                    scaleInterp.m_endTime = getIntArg(this, instr, 0);
                    scaleInterp.m_method = getIntArg(this, instr, 1);
                    scaleInterp.m_goal.x = getFloatArg(this, instr, 2);
                    scaleInterp.m_goal.y = getFloatArg(this, instr, 3);
                    scaleInterp.m_initial = scale;
                    scaleInterp.m_timer.setCurrent(0);
                    break;

                // Toggles mirroring on the x axis.
                // This flips the sign of sx in scale. Future calls to scale will thus clobber it.
                // (It also toggles a bitflag. This flag is used by EoSD to keep the sign flipped during interpolation of scale, and is then unused until BM which added th185:unflip.)
                case 61: // flipX
                    scale.x = -scale.x;
                    flagsLow ^= 0x2; // Toggle flipX flag
                    break;

                // Toggles mirroring on the y axis.
                // This flips the sign of sy in scale. Future calls to scale will thus clobber it.
                // (It also toggles a bitflag. This flag is used by EoSD to keep the sign flipped during interpolation of scale, and is then unused until BM which added th185:unflip.)
                case 62: // flipY
                    scale.y = -scale.y;
                    flagsLow ^= 0x4; // Toggle flipY flag
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
                    anchorH = getIntArg(this, instr, 0);
                    anchorV = getIntArg(this, instr, 1);
                    break;

                // Set color blending mode.
                // Modes for DDC: (other games may be different)
                // 0: Normal (SRCALPHA, INVSRCALPHA, ADD)
                // 1: Add (SRCALPHA, ONE, ADD)
                // 2: Subtract (SRCALPHA, ONE, REVSUBTRACT)
                // ...
                case 66: // blendMode(int mode)
                    blendMode = getIntArg(this, instr, 0);
                    break;

                // Determines how the ANM is rendered.
                // Mode 0: 2D sprites, no rotation.
                // Mode 1: 2D sprites with z-axis rotation.
                // Mode 8: 3D rotation.
                // Mode 2: Like mode 0 but shifted by (-0.5, -0.5) pixels.
                case 67: // type(int mode)
                    renderMode = getIntArg(this, instr, 0);
                    break;

                // Sets the layer of the ANM. This may or may not affect z-ordering? It's weird...
                // Different layer numbers may behave differently! Each game only has a finite number of layers, and certain groups of these layers are drawn at different stages in the rendering pipeline.
                case 68: // layer(int n)
                    layer = getIntArg(this, instr, 0);
                    break;

                // This is like stop except that it also hides the graphic by clearing the visibility flag (see visible).
                // Interpolation instructions like posTime will continue to advance, and interrupts can be triggered at any time. Successful interrupts will automatically re-enable the visibility flag.
                case 69: // stopHide
                    m_flagsLow &= 0xfffffffe;

switchD_0044b52d_caseD_3f:

                    if (m_pendingInterrupt == 0)
                        m_flagsLow |= 0x1000;
                    else
                    {
Interrupt:
                        cur_instr = m_beginningOfScript;
                        instr_interrupt = nullptr;
                        while (1)
                        {
                            uint16_t cur_instr_opcode = cur_instr->opcode;
                            opcode = 0; // Nop

                            if ((cur_instr_opcode == 64 && m_pendingInterrupt == cur_instr->args[0]) || cur_instr_opcode == -1)
                                break;

                            if (cur_instr_opcode == 64 && cur_instr->args[0] == -1)
                                instr_interrupt = cur_instr;
                        
                            cur_instr = (AnmRawInstruction*)((char*)m_currentInstruction + cur_instr->offsetToNextInstr);
                        }
                    
                        if (cur_instr->opcode == 64 || (cur_instr = instr_interrupt, instr_interrupt != nullptr)
                        {
                            m_interruptReturnTime.previous = m_timeInScript.previous;
                            m_interruptReturnTime.current = m_timeInScript.current;
                            m_interruptReturnTime.currentF = m_timeInScript.currentF;
                            m_interruptReturnTime.gameSpeed = m_timeInScript.gameSpeed;
                            instr_interrupt = m_currentInstruction;
                            m_interruptReturnTime.isInitialized = m_timeInScript.isInitialized;
                            This->interruptReturnInstr = instr_interrupt;
                            Timer::setCurrent(&This->timeInScript,(int)cur_instr->time);
                            pendingInterrupt = cur_instr->offsetToNextInstr;
                            This->flagsLow = This->flagsLow | 1;
                            This->currentInstruction = (AnmRawInstruction*)((char*)m_currentInstruction + cur_instr->offsetToNextInstr);
                            break;
                        }
                    }
                    Timer::add(&This->timeInScript,-1.0);
                    goto Idk_Why_I_Must_Jump_0044c2ae;

                // Add vel to the texture u coordinate every frame (in units of 1 / total_image_width), causing the displayed sprite to scroll horizontally through the image file.
                case 70: // scrollX(float vel)
                    scrollU = getFloatArg(this, instr, 0);
                    break;

                // Add vel to the texture v coordinate every frame (in units of 1 / total_image_height), causing the displayed sprite to scroll vertically through the image file.
                case 71: // scrollY(float vel)
                    scrollV = getFloatArg(this, instr, 0);
                    break;

                // Set the visibility flag (1 = visible). Invisible graphics are skipped during rendering.
                // Generally speaking this is not a huge deal as the most expensive parts of rendering are typically skipped anyways whenever alpha and alpha2 are both 0.
                case 72: // visible(byte visible)
                    visible = getIntArg(this, instr, 0);
                    break;

                // If disable is 1, writing to the z-buffer is disabled. Otherwise, it is enabled. This can matter in cases where z-testing is used to filter writes.
                case 73: // zWriteDisable(int disable)
                    zWriteDisable = getIntArg(this, instr, 0);
                    break;

                // Sets the state of a STD-related bitflag. When this flag is enabled, some unknown vector from the stage background camera data is added to some poorly-understood position-related vector on the ANM, for an even more unknown purpose.
                case 74: // ins_74(int enable)
                    // TODO: Implement STD-related bitflag logic
                    break;

                // Wait t frames.
                // StB and earlier: wait is implemented using a dedicated timer.
                // MoF and later: Subtracts t from the current time before executing the next instruction.
                case 75: // wait(int t)
                    timeInScript.setCurrent(timeInScript.m_current - getIntArg(this, instr, 0)); // MoF+ behavior
                    break;

                // Set a second color for gradients. Gradients are used by certain special drawing instructions, and can be enabled on regular sprites using colorMode.
                case 76: // color2(int r, int g, int b)
                    color2.r = getIntArg(this, instr, 0);
                    color2.g = getIntArg(this, instr, 1);
                    color2.b = getIntArg(this, instr, 2);
                    break;

                // Set a second alpha for gradients. Gradients are used by certain special drawing instructions, and can be enabled on regular sprites using colorMode.
                case 77: // alpha2(int alpha)
                    alpha2 = getIntArg(this, instr, 0);
                    break;

                // Over the next t frames, changes color2 to the given value using interpolation mode mode.
                // For some reason, in DDC onwards, this also sets colorMode to 1, which can be a mild inconvenience.
                case 78: // color2Time(int t, int mode, int r, int g, int b)
                    color2Interp.m_endTime = getIntArg(this, instr, 0);
                    color2Interp.m_method = getIntArg(this, instr, 1);
                    color2Interp.m_goal.r = getIntArg(this, instr, 2);
                    color2Interp.m_goal.g = getIntArg(this, instr, 3);
                    color2Interp.m_goal.b = getIntArg(this, instr, 4);
                    color2Interp.m_initial = color2;
                    color2Interp.m_timer.setCurrent(0);
                    colorMode = 1; // DDC+ behavior
                    break;

                // Over the next t frames, changes alpha2 to the given value using interpolation mode mode.
                // For some reason, in DDC onwards, this also sets colorMode to 1, which can be a mild inconvenience.
                case 79: // alpha2Time(int t, int mode, int alpha)
                    alpha2Interp.m_endTime = getIntArg(this, instr, 0);
                    alpha2Interp.m_method = getIntArg(this, instr, 1);
                    alpha2Interp.m_goal = getIntArg(this, instr, 2);
                    alpha2Interp.m_initial = alpha2;
                    alpha2Interp.m_timer.setCurrent(0);
                    colorMode = 1; // DDC+ behavior
                    break;

                // Lets you enable gradients on regular sprites.
                // 0: Only use color and alpha.
                // 1: Only use color2 and alpha2.
                // 2: (DS–) Horizontal gradient.
                // 3: (DS–) Vertical gradient.
                case 80: // colorMode(int mode)
                    colorMode = getIntArg(this, instr, 0);
                    break;

                // Can be used at the end of a interruptLabel block to return back to the moment just before the VM received the interrupt.
                // This is not the only way to end a interruptLabel block; oftentimes the game may use a stop instead.
                case 81: // caseReturn
                    currentInstruction = interruptReturnInstr;
                    timeInScript = interruptReturnTime;
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
        // TODO: Update timers and interpolation
    }
}
