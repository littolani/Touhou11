#include "AnmManager.h"
#include "AnmLoaded.h"
#include "Globals.h"
#include "ThunkGenerator.h"

AnmLoaded* AnmManager::preloadAnm(int anmSlotIndex, const char* anmFileName)
{
    AnmManager* This = g_anmManager;

    // Return existing animation if already loaded
    if (This->m_loadedAnms[anmSlotIndex] != nullptr)
    {
        printf("preloadAnm: already loaded %s\n", anmFileName);
        return This->m_loadedAnms[anmSlotIndex];
    }

    // Load animation from memory
    AnmLoaded* anmLoaded = preloadAnmFromMemory(This, anmSlotIndex, anmFileName);
    if (anmLoaded == nullptr)
        return nullptr;

    anmLoaded->m_anmsLoading = 1;
    while (anmLoaded->m_anmsLoading != 0 && (g_supervisor.criticalSectionFlag & 0x180) == 0) // Wait until animation loading is complete
        Sleep(1);

    printf("preloadAnm: %s\n", anmFileName);
    return anmLoaded;
}

int AnmManager::openAnmLoaded(AnmLoaded* anmLoaded, AnmHeader* anmHeader, int chunkIndex)
{
    // Check if the version is 7 (specific to this ANM format)
    if (anmHeader->version != 7)
    {
        printf("ANM version is incorrect\n");
        return -1;
    }

    // If hasdata is 0, load an external texture file
    if (anmHeader->hasData == 0)
    {
        char* name = (char*)anmHeader + anmHeader->nameOffset;

        // Skip if name starts with '@' (special case)
        if (*name != '@')
        {
            char filePath[260];
            sprintf_s(filePath, "%s", name);
            size_t outSize;
            uint32_t* memoryMappedFile = (uint32_t*)openFile(filePath, &outSize, 1);
            if (memoryMappedFile == nullptr)
            {
                printf(" %s \n", name);
                return -1;
            }
            // Store the loaded file and size in the chunk data buffer
            anmLoaded->m_anmLoadedD3D[chunkIndex].m_srcData = memoryMappedFile;
            anmLoaded->m_anmLoadedD3D[chunkIndex].m_srcDataSize = outSize;
        }
    }
    return 1;
}

AnmLoaded* AnmManager::preloadAnmFromMemory(AnmManager* This, int anmSlotIndex, const char* anmFilePath)
{
    printf("preloadAnmFromMemory: %s\n", anmFilePath);

    // Check if the slot index is within bounds (0-31)
    if (anmSlotIndex >= 32)
    {
        printf("Anm slot index %d is out of bounds!\n", anmSlotIndex);
        return nullptr;
    }

    // Resolve the file path
    char resolvedFilePath[260];
    sprintf_s(resolvedFilePath, "%s", anmFilePath);

    AnmHeader* header = (AnmHeader*)openFile(resolvedFilePath, nullptr, 0);
    if (!header)
    {
        printf("Anm header is NULL!\n");
        return nullptr;
    }

    // Allocate and initialize the AnmLoaded structure
    AnmLoaded* anmLoaded = (AnmLoaded*)game_new(sizeof(AnmLoaded));
    if (!anmLoaded)
    {
        printf("Unable to allocate anmLoaded!\n");
        return nullptr;
    }
    memset(anmLoaded, 0, sizeof(AnmLoaded));
    This->m_loadedAnms[anmSlotIndex] = anmLoaded;

    // Set initial fields
    anmLoaded->m_anmSlotIndex = anmSlotIndex;
    anmLoaded->m_header = header;
    strcpy_s(anmLoaded->m_filePath, anmFilePath);

    // Parse the ANM header and count chunks, sprites, and scripts
    AnmHeader* chunk = (AnmHeader*)header;
    int numSprites = chunk->numSprites;
    int numScripts = chunk->numScripts;
    int processedCount = 1;
    uint32_t nextOffset = chunk->nextOffset;
    AnmHeader* currentChunk = chunk;

    while (nextOffset != 0)
    {
        currentChunk = (AnmHeader*)((char*)currentChunk + nextOffset);
        numSprites += currentChunk->numSprites;
        numScripts += currentChunk->numScripts;
        processedCount++;
        nextOffset = currentChunk->nextOffset;
    }

    // Allocate buffers for chunk data, keyframes, and sprites
    anmLoaded->m_numAnmLoadedD3Ds = processedCount;

    anmLoaded->m_anmLoadedD3D = (AnmLoadedD3D*)game_malloc(processedCount * sizeof(AnmLoadedD3D));
    memset(anmLoaded->m_anmLoadedD3D, 0, processedCount * sizeof(AnmLoadedD3D));

    //anmLoaded->m_keyframeData = new AnmLoadedSprite[numSprites]();
    anmLoaded->m_keyframeData = (AnmLoadedSprite*)game_malloc(numSprites * sizeof(AnmLoadedSprite));

    //anmLoaded->m_spriteData = new uint32_t[numScripts]();
    anmLoaded->m_spriteData = (uint32_t*)game_malloc(numScripts * sizeof(uint32_t));

    anmLoaded->m_numScripts = numScripts;
    anmLoaded->m_numSprites = numSprites;

    // Process each chunk
    int chunkIndex = 0;
    currentChunk = header;
    while (currentChunk != nullptr)
    {
        int result = openAnmLoaded(anmLoaded, currentChunk, chunkIndex);
        if (result < 0)
        {
            printf("Could not read ANM data.\n");
            game_free(anmLoaded->m_header);
            game_free(anmLoaded->m_keyframeData);
            game_free(anmLoaded->m_spriteData);
            game_free(anmLoaded);
            This->m_loadedAnms[anmSlotIndex] = nullptr;
            return nullptr;
        }
        chunkIndex++;
        if (currentChunk->nextOffset == 0)
            break;
        currentChunk = (AnmHeader*)((byte*)currentChunk + currentChunk->nextOffset);
    }
    return anmLoaded;
}

void AnmManager::markAnmLoadedAsReleasedInVmList(AnmManager* This, AnmLoaded* anmLoaded)
{
    AnmVmList* temp = This->m_primaryGlobalNext;
    while (temp)
    {
        AnmVmList* next = temp->next;
        AnmVm* entry = temp->entry;
        temp = next;
        if (entry->m_anmLoaded == anmLoaded)
            entry->m_flagsLow |= 0x4000000;
    }
    temp = This->m_secondaryGlobalNext;
    while (temp)
    {
        AnmVmList* next = temp->next;
        AnmVm* entry = temp->entry;
        temp = next;
        if (entry->m_anmLoaded == anmLoaded)
            entry->m_flagsLow |= 0x4000000;
    }
}

AnmVm* AnmManager::allocateVm(AnmManager* This)
{
    int index = This->m_nextBulkVmIndex;
    AnmVm* vm;

    if (This->m_bulkVmsIsAlive[index] == 0)
    {
        vm = &This->m_bulkVms[index];
        This->m_bulkVmsIsAlive[index] = 1;
        This->m_nextBulkVmIndex = This->getNextBulkVmIndex(index);
    }
    else
    {
        int nextIdx = This->getNextBulkVmIndex(index);
        if (This->m_bulkVmsIsAlive[nextIdx] == 0)
        {
            vm = &This->m_bulkVms[nextIdx];
            This->m_bulkVmsIsAlive[nextIdx] = 1;
        }
        else
        {
            vm = new AnmVm();
            if (vm)
                vm->initialize(vm);
        }
        This->m_nextBulkVmIndex = This->getNextBulkVmIndex(nextIdx);
    }
    return vm;
}

// 0x445320
void AnmManager::releaseTextures()
{
    AnmManager* This = g_anmManager;
    for (int i = 0; i < NUM_ANM_LOADEDS; ++i)
    {
        AnmLoaded* anmLoaded = This->m_loadedAnms[i];
        if (anmLoaded == nullptr)
            continue; // Skip null entries

        AnmLoadedD3D* anmLoadedD3Ds = anmLoaded->m_anmLoadedD3D;
        int m_numAnmLoadedD3Ds = anmLoaded->m_numAnmLoadedD3Ds;
        for (int j = 0; j < m_numAnmLoadedD3Ds; ++j)
        {
            AnmLoadedD3D* entry = &anmLoadedD3Ds[j];
            if ((entry->m_flags & 1) != 0 && entry->m_texture != nullptr)
            {
                entry->m_texture->Release();
                entry->m_texture = nullptr;
            }
        }
    }
}

void AnmManager::flushSprites(AnmManager* This)
{
    if (This->m_anmVertexBuffers.leftoverSpriteCount == 0)
        return;

    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->SetFVF(0x144);

    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLELIST,
        This->m_anmVertexBuffers.leftoverSpriteCount * 2,
        This->m_anmVertexBuffers.spriteRenderCursor,
        sizeof(RenderVertex144)
    );

    ++This->m_refreshCounter;
    This->m_anmVertexBuffers.spriteRenderCursor = This->m_anmVertexBuffers.spriteWriteCursor;
    This->m_anmVertexBuffers.leftoverSpriteCount = 0;
}

// 0x454ec0
void AnmManager::blitTextureToSurface(AnmManager* This, BlitParams* blitParams)
{
    // Early return if the texture is null
    if (!This->m_loadedAnms[blitParams->anmLoadedIndex]->m_anmLoadedD3D[blitParams->anmLoadedD3dIndex].m_texture)
        return;

    // Ensure pending sprite operations are completed
    flushSprites(This);

    // Retrieve the back buffer from the Direct3D device
    IDirect3DSurface9* backBuffer = nullptr;
    HRESULT hr = g_supervisor.d3dDevice->GetBackBuffer(
        0,
        0,
        D3DBACKBUFFER_TYPE_MONO,
        &backBuffer
    );
    if (FAILED(hr))
        return;

    // Access the texture from the AnmLoadedD3D array
    AnmLoadedD3D* anmLoadedD3d = This->m_loadedAnms[blitParams->anmLoadedIndex]->m_anmLoadedD3D;
    IDirect3DTexture9* texture = anmLoadedD3d[blitParams->anmLoadedD3dIndex].m_texture;

    // Get the surface from the texture (level 0)
    IDirect3DSurface9* textureSurface = nullptr;
    hr = texture->GetSurfaceLevel(0, &textureSurface);
    if (FAILED(hr))
    {
        backBuffer->Release();
        return;
    }

    // Set up source rectangle (assuming r1.right and r1.bottom are width and height)
    RECT srcRect{};
    srcRect.left = blitParams->srcRect.left;
    srcRect.top = blitParams->srcRect.top;
    srcRect.right = blitParams->srcRect.left + blitParams->srcRect.right;
    srcRect.bottom = blitParams->srcRect.top + blitParams->srcRect.bottom;

    // Set up destination rectangle (assuming r2.right and r2.bottom are width and height)
    RECT dstRect{};
    dstRect.left = blitParams->destRect.left;
    dstRect.top = blitParams->destRect.top;
    dstRect.right = blitParams->destRect.left + blitParams->destRect.right;
    dstRect.bottom = blitParams->destRect.top + blitParams->destRect.bottom;

    // Copy the region from the back buffer to the texture surface
    hr = D3DXLoadSurfaceFromSurface(
        textureSurface,
        nullptr,
        &dstRect,
        backBuffer,
        nullptr,
        &srcRect,
        2,
        0
    ); // Filter and color key values preserved from original

    // If the copy succeeded, mark the texture as dirty
    if (SUCCEEDED(hr))
        texture->AddDirtyRect(nullptr);

    // Release resources
    textureSurface->Release();
    backBuffer->Release();
}

// 0x4453b0
void AnmManager::createD3DTextures(AnmManager* This)
{
    for (int i = 0; i < 32; ++i)
    {
        AnmLoaded* loadedAnm = This->m_loadedAnms[i];
        if (!loadedAnm)
            continue;

        if (loadedAnm->m_numAnmLoadedD3Ds <= 0)
            continue;

        for (int j = 0; j < loadedAnm->m_numAnmLoadedD3Ds; ++j)
        {
            AnmLoadedD3D* anmLoadedD3D = &loadedAnm->m_anmLoadedD3D[j];
            if (anmLoadedD3D->m_flags & 1)
            {
                anmLoadedD3D->m_flags |= 1;
                g_supervisor.d3dDevice->CreateTexture(
                    g_supervisor.m_d3dPresetParameters.BackBufferWidth,
                    g_supervisor.m_d3dPresetParameters.BackBufferHeight,
                    1,
                    1,
                    g_supervisor.m_d3dPresetParameters.BackBufferFormat,
                    D3DPOOL_DEFAULT,
                    &anmLoadedD3D->m_texture,
                    NULL
                );

                anmLoadedD3D->m_bytesPerPixel = (g_supervisor.m_d3dPresetParameters.BackBufferFormat == D3DFMT_X8R8G8B8) * 2 + 2;
            }
        }
    }
}

// 0x450e20
int AnmManager::updateWorldMatrixAndProjectQuadCorners(AnmManager* This, AnmVm* vm)
{
    puts("Called updateWorldMatrixAndProjectQuadCorners\n");
    uint32_t flags;
    D3DXMATRIX* baseSpriteScaleMatrix;
    D3DXMATRIX* localTransformMatrix;
    D3DXMATRIX* lpMatrixCopy;
    D3DXVECTOR3 vecBottomLeft;
    D3DXVECTOR3 vecBottomRight;
    D3DXVECTOR3 vecTopLeft;
    D3DXVECTOR3 vecTopRight;
    D3DXMATRIX worldMatrix;
    D3DXMATRIX rotationMatrix;
    float rotX;
    float rotY;
    float rotZ;
    float entityPosX;
    float posX;
    float scaleX;

    flags = vm->m_flagsLow;
    if (((flags & 0x4000) == 0) && ((flags & 0xc) != 0))
    {
        scaleX = vm->m_scale.x;
        localTransformMatrix = &vm->m_localTransformMatrix; // Local transform (scale + rotation)
        baseSpriteScaleMatrix = &vm->m_baseScaleMatrix;     // Base scale from unit quad to sprite size
        lpMatrixCopy = localTransformMatrix;

        // Copy baseSpriteScaleMatrix to localTransformMatrix
        memcpy(localTransformMatrix, baseSpriteScaleMatrix, sizeof(D3DXMATRIX));

        // Apply VM scale to localTransformMatrix
        localTransformMatrix->m[0][0] = scaleX * localTransformMatrix->m[0][0];
        localTransformMatrix->m[1][1] = vm->m_scale.y * localTransformMatrix->m[1][1];

        // Clear rotation-applied flag
        vm->m_flagsLow = flags & 0xfffffff7;

        // Apply rotations if non-zero
        rotX = vm->m_rotation.x;
        if (rotX != 0.0)
        {
            D3DXMatrixRotationX(&rotationMatrix, vm->m_rotation.x);
            D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &rotationMatrix);
        }

        rotY = vm->m_rotation.y;
        if (rotY != 0.0)
        {
            D3DXMatrixRotationY(&rotationMatrix, vm->m_rotation.y);
            D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &rotationMatrix);
        }

        rotZ = vm->m_rotation.z;
        if (rotZ != 0.0)
        {
            D3DXMatrixRotationZ(&rotationMatrix, vm->m_rotation.z);
            D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &rotationMatrix);
        }

        // Clear scale-applied flag
        vm->m_flagsLow &= 0xfffffffb;
    }

    // Build world matrix: local transform + translations
    entityPosX = vm->m_entityPos.x;
    posX = vm->m_pos.x;

    memcpy(&worldMatrix, &vm->m_localTransformMatrix, sizeof(D3DXMATRIX));

    worldMatrix.m[3][0] = entityPosX + posX + vm->m_offsetPos.x + worldMatrix.m[3][0];
    worldMatrix.m[3][1] = vm->m_entityPos.y + vm->m_pos.y + vm->m_offsetPos.y + worldMatrix.m[3][1];
    worldMatrix.m[3][2] = vm->m_entityPos.z + vm->m_pos.z + vm->m_offsetPos.z;

    // Determine quad corners based on horizontal alignment flags (bits 18-19)
    flags = vm->m_flagsLow >> 0x12 & 3;
    if (flags == 0) // Centered
    {
        vecBottomLeft.x = -128.0;
        vecTopLeft.x = -128.0;
        vecBottomRight.x = 128.0;
        vecTopRight.x = 128.0;
    }
    else if (flags == 1) // Left-aligned
    {
        vecBottomLeft.x = 0.0;
        vecTopLeft.x = 0.0;
        vecBottomRight.x = 256.0;
        vecTopRight.x = 256.0;
    }
    else if (flags == 2) // Right-aligned
    {
        vecBottomLeft.x = -256.0;
        vecTopLeft.x = -256.0;
        vecBottomRight.x = 0.0;
        vecTopRight.x = 0.0;
    }

    // Determine quad corners based on vertical alignment flags (bits 20-21)
    flags = vm->m_flagsLow >> 0x14 & 3;
    if (flags == 0) // Centered
    {
        vecBottomRight.y = -128.0;
        vecBottomLeft.y = -128.0;
        vecTopRight.y = 128.0;
        vecTopLeft.y = 128.0;
    }
    else if (flags == 1) // Top-aligned
    {
        vecBottomRight.y = 0.0;
        vecBottomLeft.y = 0.0;
        vecTopRight.y = 256.0;
        vecTopLeft.y = 256.0;
    }
    else if (flags == 2) // Bottom-aligned
    {
        vecBottomRight.y = -256.0;
        vecBottomLeft.y = -256.0;
        vecTopRight.y = 0.0;
        vecTopLeft.y = 0.0;
    }

    // Set Z to 0 for all corners (2D quad)
    vecBottomRight.z = 0.0;
    vecTopLeft.z = 0.0;
    vecBottomLeft.z = 0.0;
    vecTopRight.z = 0.0;

    // Project quad corners to screen space
    D3DXVECTOR3 bottomLeftOut, bottomRightOut, topLeftOut, topRightOut;
    D3DXVec3Project(
        &bottomLeftOut,
        &vecBottomLeft,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &bottomRightOut,
        &vecBottomRight,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &topLeftOut,
        &vecTopLeft,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &topRightOut,
        &vecTopRight,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    g_renderQuad144[0].pos.x = bottomLeftOut.x;
    g_renderQuad144[0].pos.y = bottomLeftOut.y;
    g_renderQuad144[0].pos.z = bottomLeftOut.z;
    g_renderQuad144[1].pos.x = bottomRightOut.x;
    g_renderQuad144[1].pos.y = bottomRightOut.y;
    g_renderQuad144[1].pos.z = bottomRightOut.z;
    g_renderQuad144[2].pos.x = topRightOut.x;
    g_renderQuad144[2].pos.y = topRightOut.y;
    g_renderQuad144[2].pos.z = topRightOut.z;
    g_renderQuad144[3].pos.x = topLeftOut.x;
    g_renderQuad144[3].pos.y = topLeftOut.y;
    g_renderQuad144[3].pos.z = topLeftOut.z;

    memcpy(&This->m_currentWorldMatrix, &worldMatrix, sizeof(D3DXMATRIX));
    return 0;
}

void AnmManager::setupRenderStateForVm(AnmManager* This, AnmVm* vm)
{
    uint8_t mode = (vm->m_flagsLow >> 4) & 0x7;
    if (This->m_renderStateMode != mode)
    {
        flushSprites(This);
        This->m_renderStateMode = mode;
        D3DBLEND srcBlend = D3DBLEND_ONE;
        D3DBLEND destBlend = D3DBLEND_ONE;

        switch (mode)
        {
        case 0:  // Normal alpha blending
            srcBlend = D3DBLEND_SRCALPHA;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        case 1:  // Additive blending
            srcBlend = D3DBLEND_SRCALPHA;
            destBlend = D3DBLEND_ONE;
            break;
        case 2:  // Subtract blending
            srcBlend = D3DBLEND_ZERO;
            destBlend = D3DBLEND_INVSRCCOLOR;
            break;
        case 3:  // Replace
            srcBlend = D3DBLEND_ONE;
            destBlend = D3DBLEND_ZERO;
            break;
        case 4:
            srcBlend = D3DBLEND_INVDESTCOLOR;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        case 5:
            srcBlend = D3DBLEND_DESTCOLOR;
            destBlend = D3DBLEND_ZERO;
            break;
        case 6:
            srcBlend = D3DBLEND_INVSRCCOLOR;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        default:
            return;
        }
        g_supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, srcBlend);
        g_supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, destBlend);
    }
    bool usefilter = (vm->m_flagsLow & 0x80000000) != 0;
    if (This->m_usePointFilter != usefilter)
    {
        flushSprites(This);
        This->m_usePointFilter = usefilter;
        D3DTEXTUREFILTERTYPE filterType = This->m_usePointFilter ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, filterType);
        g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, filterType);
    }
    This->m_someCounter++;
}

void AnmManager::writeSprite(AnmManager* This, RenderVertex144* srcVertices)
{
    RenderVertex144* dest = This->m_anmVertexBuffers.spriteWriteCursor;

    // Quad (0,1,2,3) -> Triangle List (0,1,2) + (1,3,2)
    // The specific order depends on the cull mode (CW vs CCW).
    // Dest[0] = Src[0]
    // Dest[1] = Src[1]
    // Dest[2] = Src[2]
    // Dest[3] = Src[1]
    // Dest[4] = Src[3]
    // Dest[5] = Src[2]

    // Tri 1: TL, TR, BL
    dest[0] = srcVertices[0];
    dest[1] = srcVertices[1];
    dest[2] = srcVertices[2];

    // Tri 2: TR, BR, BL
    dest[3] = srcVertices[1];
    dest[4] = srcVertices[3];
    dest[5] = srcVertices[2];

    // Advance cursor by 6 vertices
    This->m_anmVertexBuffers.spriteWriteCursor += 6;
    This->m_anmVertexBuffers.leftoverSpriteCount++;
}

void AnmManager::drawVmSprite2D(AnmManager* This, uint32_t layer, AnmVm* anmVm)
{
    // Apply global offset
    for (int i = 0; i < 4; i++)
    {
        g_renderQuad144[i].pos.x += This->m_globalRenderQuadOffsetX;
        g_renderQuad144[i].pos.y += This->m_globalRenderQuadOffsetY;
    }

    // Pixel Alignment (Round to nearest pixel - 0.5 for texel center mapping in DX9)
    if (layer & 1)
    {
        // ASM: 44f92c fsubs 0x4973f8 (0.5) after rounding
        g_renderQuad144[0].pos.x = roundf(g_renderQuad144[0].pos.x) - 0.5f;
        g_renderQuad144[1].pos.x = roundf(g_renderQuad144[1].pos.x) - 0.5f;
        g_renderQuad144[0].pos.y = roundf(g_renderQuad144[0].pos.y) - 0.5f;
        g_renderQuad144[2].pos.y = roundf(g_renderQuad144[2].pos.y) - 0.5f;

        // Propagate aligned positions to form the rect
        g_renderQuad144[1].pos.y = g_renderQuad144[0].pos.y;
        g_renderQuad144[2].pos.x = g_renderQuad144[0].pos.x;
        g_renderQuad144[3].pos.x = g_renderQuad144[1].pos.x;
        g_renderQuad144[3].pos.y = g_renderQuad144[2].pos.y;
    }

    // Apply UV Scrolling
    for (int i = 0; i < 4; i++)
    {
        g_renderQuad144[i].uv.x = anmVm->m_spriteUvQuad[i].x + anmVm->m_uvScrollPos.x;
        g_renderQuad144[i].uv.y = anmVm->m_spriteUvQuad[i].y + anmVm->m_uvScrollPos.y;
    }

    // Bounding Box Calculation
    float maxX = g_renderQuad144[1].pos.x;
    if (g_renderQuad144[0].pos.x > maxX) maxX = g_renderQuad144[0].pos.x;
    if (g_renderQuad144[2].pos.x > maxX) maxX = g_renderQuad144[2].pos.x;
    if (g_renderQuad144[3].pos.x > maxX) maxX = g_renderQuad144[3].pos.x;

    float minX = g_renderQuad144[1].pos.x;
    if (g_renderQuad144[0].pos.x < minX) minX = g_renderQuad144[0].pos.x;
    if (g_renderQuad144[2].pos.x < minX) minX = g_renderQuad144[2].pos.x;
    if (g_renderQuad144[3].pos.x < minX) minX = g_renderQuad144[3].pos.x;

    float maxY = g_renderQuad144[1].pos.y;
    if (g_renderQuad144[0].pos.y > maxY) maxY = g_renderQuad144[0].pos.y;
    if (g_renderQuad144[2].pos.y > maxY) maxY = g_renderQuad144[2].pos.y;
    if (g_renderQuad144[3].pos.y > maxY) maxY = g_renderQuad144[3].pos.y;

    float minY = g_renderQuad144[1].pos.y;
    if (g_renderQuad144[0].pos.y < minY) minY = g_renderQuad144[0].pos.y;
    if (g_renderQuad144[2].pos.y < minY) minY = g_renderQuad144[2].pos.y;
    if (g_renderQuad144[3].pos.y < minY) minY = g_renderQuad144[3].pos.y;

    // Viewport Culling
    D3DVIEWPORT9* vp = &g_supervisor.currentCam->viewport;
    float vpX = static_cast<float>(vp->X);
    float vpY = static_cast<float>(vp->Y);
    float vpW = static_cast<float>(vp->Width);
    float vpH = static_cast<float>(vp->Height);

    if (vpX <= maxX && (vpX + vpW) >= minX && vpY <= maxY && (vpY + vpH) >= minY)
    {
        AnmLoadedD3D* tex = anmVm->m_sprite->anmLoadedD3D;
        if (This->m_anmLoadedD3D != tex)
        {
            This->m_anmLoadedD3D = tex;
            flushSprites(This);
            g_supervisor.d3dDevice->SetTexture(0, This->m_anmLoadedD3D->m_texture);
        }

        if (This->m_haveFlushedSprites != 1)
        {
            flushSprites(This);
            This->m_haveFlushedSprites = 1;
        }

        D3DCOLOR c0 = g_renderQuad144[0].diffuse;
        D3DCOLOR c1 = g_renderQuad144[1].diffuse;
        D3DCOLOR c2 = g_renderQuad144[2].diffuse;
        D3DCOLOR c3 = g_renderQuad144[3].diffuse;

        if ((layer & 2) == 0)
        {
            if ((anmVm->m_flagsLow & 0x8000) == 0)
                c0 = anmVm->m_color0;
            else
                c0 = anmVm->m_color1;

            c1 = c0; c2 = c0; c3 = c0;

            if (This->m_rebuildColorFlag)
            {
                // Extract ARGB from c0
                uint32_t a = (c0 >> 24) & 0xFF;
                uint32_t r = (c0 >> 16) & 0xFF;
                uint32_t g = (c0 >> 8) & 0xFF;
                uint32_t b = c0 & 0xFF;

                // Modulate
                r = modulateColorComponent(r, This->m_scaleR);
                g = modulateColorComponent(g, This->m_scaleG);
                b = modulateColorComponent(b, This->m_scaleB);
                a = modulateColorComponent(a, This->m_scaleA);

                c0 = (a << 24) | (r << 16) | (g << 8) | b;
                c1 = c0; c2 = c0; c3 = c0;
            }
        }

        g_renderQuad144[0].diffuse = c0;
        g_renderQuad144[1].diffuse = c1;
        g_renderQuad144[2].diffuse = c2;
        g_renderQuad144[3].diffuse = c3;

        setupRenderStateForVm(This, anmVm);
        writeSprite(This, g_renderQuad144);
    }
}

// 0x44f490
uint32_t AnmManager::modulateColorComponent(uint16_t base, uint16_t factor)
{
    uint32_t i = (base & 0xff) * (factor & 0xff) >> 7;
    if (0xff < i)
        i = 0xff;
    return i;
}

int AnmManager::drawVmWithFog(AnmManager* This, AnmVm* vm)
{
    puts("Drawing vm with fog\n");
    if (vm->projectQuadCornersThroughCameraViewport(vm) != 0)
        return -1;

    float fogStart = g_supervisor.currentCam->fogStart;
    float fogEnd = g_supervisor.currentCam->fogEnd;
    float cullDistance = g_supervisor.cam0.fogStart;

    D3DCOLOR color = (vm->m_flagsLow & 0x8000) ? vm->m_color1 : vm->m_color0;

    float x = vm->m_pos.x + vm->m_entityPos.x + vm->m_offsetPos.x - g_supervisor.currentCam->offset.x;
    float y = vm->m_pos.y + vm->m_entityPos.y + vm->m_offsetPos.y - g_supervisor.currentCam->offset.y;
    float z = vm->m_pos.z + vm->m_entityPos.z + vm->m_offsetPos.z - g_supervisor.currentCam->offset.z;

    float dist = sqrtf(x * x + y * y + z * z);

    if (This->m_rebuildColorFlag != 0)
    {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint8_t a = (color >> 24) & 0xFF;

        r = scaleChannel(This->m_scaleR, r);
        g = scaleChannel(This->m_scaleG, g);
        b = scaleChannel(This->m_scaleB, b);
        a = scaleChannel(This->m_scaleA, a);

        color = (a << 24) | (r << 16) | (g << 8) | b;
    }

    if (dist <= cullDistance)
        g_renderQuad144[0].diffuse = color;
    else
    {
        float denominator = fogStart - fogEnd;
        float factor = (cullDistance - dist) / denominator;

        if (factor >= 1.0f)
            return -1;

        int fogR = static_cast<int>(g_supervisor.cam0.fogR);
        int fogG = static_cast<int>(g_supervisor.cam0.fogG);
        int fogB = static_cast<int>(g_supervisor.cam0.fogB);

        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;


        int subR = static_cast<int>(static_cast<float>(r - fogR) * factor);
        int subG = static_cast<int>(static_cast<float>(g - fogG) * factor);
        int subB = static_cast<int>(static_cast<float>(b - fogB) * factor);

        r -= subR;
        g -= subG;
        b -= subB;

        a = static_cast<uint8_t>(static_cast<float>(a) * (1.0f - factor));
        color = (a << 24) | (r << 16) | (g << 8) | b;
        g_renderQuad144[0].diffuse = color;
    }

    g_renderQuad144[1].diffuse = g_renderQuad144[0].diffuse;
    g_renderQuad144[2].diffuse = g_renderQuad144[0].diffuse;
    g_renderQuad144[3].diffuse = g_renderQuad144[0].diffuse;

    drawVmSprite2D(This, 2, vm);
    return 0;
}

void AnmManager::applyRenderStateForVm(AnmManager* This, AnmVm* vm)
{
    IDirect3DDevice9* device = g_supervisor.d3dDevice;

    uint8_t blendMode = (vm->m_flagsLow >> 4) & 0x7;
    if (blendMode != This->m_renderStateMode)
    {
        flushSprites(This);
        This->m_renderStateMode = blendMode;

        D3DBLEND srcBlend, destBlend;
        switch (blendMode)
        {
        case 0:
            srcBlend = D3DBLEND_SRCALPHA;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        case 1:
            srcBlend = D3DBLEND_SRCALPHA;
            destBlend = D3DBLEND_ONE;
            break;
        case 2:
            srcBlend = D3DBLEND_ZERO;
            destBlend = D3DBLEND_INVSRCCOLOR;
            break;
        case 3:
            srcBlend = D3DBLEND_ONE;
            destBlend = D3DBLEND_ZERO;
            break;
        case 4:
            srcBlend = D3DBLEND_INVDESTCOLOR;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        case 5:
            srcBlend = D3DBLEND_DESTCOLOR;
            destBlend = D3DBLEND_ZERO;
            break;
        case 6:
            srcBlend = D3DBLEND_INVSRCCOLOR;
            destBlend = D3DBLEND_INVSRCALPHA;
            break;
        default:
            return;
        }
        device->SetRenderState(D3DRS_SRCBLEND, srcBlend);
        device->SetRenderState(D3DRS_DESTBLEND, destBlend);
    }

    D3DCOLOR color = (vm->m_flagsLow & 0x8000) ? vm->m_color1 : vm->m_color0;

    if (This->m_rebuildColorFlag)
    {
        uint32_t r = ((color >> 16) & 0xFF) * This->m_scaleR >> 7;
        r = (r > 255) ? 255 : r;

        uint32_t g = ((color >> 8) & 0xFF) * This->m_scaleG >> 7;
        g = (g > 255) ? 255 : g;

        uint32_t b = (color & 0xff) * This->m_scaleB >> 7;
        b = (b > 255) ? 255 : b;

        uint32_t a = (color >> 24) * This->m_scaleA >> 7;
        a = (a > 255) ? 255 : a;

        color = (a << 24) | (r << 16) | (g << 8) | b;
    }

    if (color != This->m_color)
    {
        flushSprites(This);
        This->m_color = color;
        device->SetRenderState(D3DRS_TEXTUREFACTOR, color);
    }

    bool usePointFilterNew = (vm->m_flagsLow & 0x80000000) != 0;
    if (usePointFilterNew != This->m_usePointFilter)
    {
        flushSprites(This);
        This->m_usePointFilter = usePointFilterNew;

        DWORD filter = usePointFilterNew ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
    }
    This->m_someCounter++;
}

int AnmManager::drawVmTriangleStrip(AnmManager* This, AnmVm* vm, SpecialRenderData* specialRenderData, uint32_t vertexCount)
{
    uint8_t alpha = (vm->m_color0 >> 24) & 0xff;
    if ((vm->m_flagsLow & 0x3) != 0x3 || alpha == 0)
        return -1;

    if (This->m_anmVertexBuffers.leftoverSpriteCount != 0)
        flushSprites(This);

    AnmLoadedD3D* tex = vm->m_sprite->anmLoadedD3D;
    if (This->m_anmLoadedD3D != tex)
    {
        This->m_anmLoadedD3D = tex;
        g_supervisor.d3dDevice->SetTexture(0, tex->m_texture);
    }

    if (This->m_haveFlushedSprites != 3)
    {
        g_supervisor.d3dDevice->SetFVF(0x144);
        This->m_haveFlushedSprites = 3;
    }
    setupRenderStateForVm(This, vm);

    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP,
        vertexCount - 2,
        specialRenderData->vertices,
        sizeof(RenderVertex144)
    );
    return 0;
}

// 0x451e10
int AnmManager::drawVmTriangleFan(AnmManager* This, AnmVm* vm, SpecialRenderData* specialRenderData, uint32_t vertexCount)
{
    if (This->m_anmVertexBuffers.leftoverSpriteCount != 0)
        flushSprites(This);

    if (This->m_haveFlushedSprites != 3)
    {
        g_supervisor.d3dDevice->SetFVF(0x144);
        This->m_haveFlushedSprites = 3;
    }
    setupRenderStateForVm(This, vm);

    AnmLoadedD3D* tex = vm->m_sprite->anmLoadedD3D;
    if (This->m_anmLoadedD3D != tex)
    {
        This->m_anmLoadedD3D = tex;
        g_supervisor.d3dDevice->SetTexture(0, tex->m_texture);
    }

    flushSprites(This);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLEFAN,
        vertexCount - 2,
        specialRenderData->vertices,
        sizeof(RenderVertex144)
    );
    return 0;
}

// 0x4561e0
AnmVm* AnmManager::getVmWithId(AnmManager* This, int anmId)
{
    if (anmId == 0)
        return nullptr;

    AnmVmList* primaryVmList = This->m_primaryGlobalNext;
    while (primaryVmList)
    {
        if (primaryVmList->entry->m_id.id == anmId)
            return primaryVmList->entry;
        primaryVmList = primaryVmList->next;
    }

    AnmVmList* secondaryVmList = This->m_secondaryGlobalNext;
    while (secondaryVmList)
    {
        if (secondaryVmList->entry->m_id.id == anmId)
            return secondaryVmList->entry;
        secondaryVmList = secondaryVmList->next;
    }
    return nullptr;
}

void AnmManager::loadIntoAnmVm(AnmVm* vm, AnmLoaded* anmLoaded, int scriptNumber)
{
    AnmRawInstruction* instr;
    uint32_t isInitialized;

    if (anmLoaded->m_spriteData[scriptNumber] != 0 && anmLoaded->m_anmsLoading == 0)
    {
        vm->initialize(vm);
        vm->m_scriptNumber = static_cast<uint16_t>(scriptNumber);
        vm->m_flagsLow &= 0xfffff9ff;
        vm->m_anmFileIndex = static_cast<uint16_t>(anmLoaded->m_anmSlotIndex);
        vm->m_anmLoaded = anmLoaded;

        instr = (AnmRawInstruction*)anmLoaded->m_spriteData[scriptNumber];
        vm->m_beginningOfScript = instr;
        vm->m_currentInstruction = instr;

        isInitialized = (vm->m_timeInScript).m_isInitialized;
        if ((isInitialized & 1) == 0)
        {
            vm->m_timeInScript.m_currentF = 0.0;
            vm->m_timeInScript.m_current = 0;
            vm->m_timeInScript.m_previous = -999999;
            vm->m_timeInScript.m_gameSpeed = &g_gameSpeed;
            vm->m_timeInScript.m_isInitialized = isInitialized | 1;
        }
        vm->m_timeInScript.m_currentF = 0.0;
        vm->m_timeInScript.m_current = 0;
        vm->m_timeInScript.m_previous = -1;
        vm->m_flagsLow &= 0xfffffffe;
        vm->run(vm);
        ++g_anmManager->m_allocatedVmCountMaybe;
        return;
    }
    memset(vm, 0, 0x434);
}

void AnmManager::putInVmList(AnmManager* This, AnmVm* vm, AnmId* anmId)
{
    AnmVmList* curVm = &vm->m_nodeInGlobalList;
    curVm->entry = vm;
    vm->m_nodeInGlobalList.next = nullptr;
    vm->m_nodeInGlobalList.prev = nullptr;
    if (This->m_primaryGlobalNext == nullptr)
        This->m_primaryGlobalNext = curVm;

    else
    {
        AnmVmList* primaryVms = This->m_primaryGlobalPrev;
        if (primaryVms->next != nullptr)
        {
            (vm->m_nodeInGlobalList).next = primaryVms->next;
            primaryVms->next->prev = curVm;
        }
        primaryVms->next = curVm;
        vm->m_nodeInGlobalList.prev = primaryVms;
    }
    This->m_primaryGlobalPrev = curVm;

    // Strange but correct game logic
    ++This->m_id;
    if (This->m_id == 0)
        ++This->m_id;

    vm->m_id.id = This->m_id;
    anmId->id = This->m_id;
}

// 0x4549e0
void AnmManager::releaseAnmLoaded(AnmManager* This, AnmLoaded* anmLoaded)
{
    if (!anmLoaded->m_header)
        return;
    This->markAnmLoadedAsReleasedInVmList(This, anmLoaded);

    if (anmLoaded->m_numAnmLoadedD3Ds > 0)
    {
        for (int i = 0; i < anmLoaded->m_numAnmLoadedD3Ds; ++i)
        {
            AnmLoadedD3D* entry = &anmLoaded->m_anmLoadedD3D[i];
            if (entry->m_texture)
            {
                entry->m_texture->Release();
                entry->m_texture = nullptr;
            }
            if (entry->m_srcData)
            {
                free(entry->m_srcData);
                entry->m_srcData = nullptr;
            }
        }
    }

    if (anmLoaded->m_anmLoadedD3D)
    {
        free(anmLoaded->m_anmLoadedD3D);
        anmLoaded->m_anmLoadedD3D = nullptr;
    }

    if (anmLoaded->m_keyframeData)
    {
        free(anmLoaded->m_keyframeData);
        anmLoaded->m_keyframeData = nullptr;
    }

    if (anmLoaded->m_spriteData)
    {
        free(anmLoaded->m_spriteData);
        anmLoaded->m_spriteData = nullptr;
    }

    if (anmLoaded->m_unknownHeapAllocated)
    {
        free(anmLoaded->m_unknownHeapAllocated);
        anmLoaded->m_unknownHeapAllocated = nullptr;
    }

    if (anmLoaded->m_header)
    {
        free(anmLoaded->m_header);
        anmLoaded->m_header = nullptr;
    }
}

void AnmManager::transformAndDraw(AnmManager* This, AnmVm* vm)
{
    // Pre-calculate world matrix and setup quad corners (likely in This->m_someBuffer or similar)
    updateWorldMatrixAndProjectQuadCorners(This, vm);

    float fogEnd = g_supervisor.currentCam->fogEnd;
    float fogStart = g_supervisor.currentCam->fogStart;
    float fogRange = fogEnd - fogStart;

    // Determine base color (Normal or Blend color)
    D3DCOLOR baseColor;
    if ((vm->m_flagsLow & 0x8000) == 0)
        baseColor = vm->m_color0;
    else
        baseColor = vm->m_color1;

    // Extract base color components
    int a = (baseColor >> 24) & 0xFF;
    int r = (baseColor >> 16) & 0xFF;
    int g = (baseColor >> 8) & 0xFF;
    int b = baseColor & 0xFF;

    // Pointer to the source positions calculated by updateWorldMatrix...
    // In the assembly, this source array seems to be contiguous to primitive0Position or similar.
    D3DXVECTOR3* srcPos = &This->m_primitive0Position;

    // Loop over the 4 vertices of the global render quad
    for (int i = 0; i < 4; i++)
    {
        D3DXVECTOR4 pOut;

        // Transform vertex position by the current world matrix
        D3DXVec3Transform(&pOut, &srcPos[i], &This->m_currentWorldMatrix);

        // Calculate vector from Camera to Vertex
        float dy = pOut.x - g_supervisor.cam0.offset.x;
        float dz = pOut.y - g_supervisor.cam0.offset.y;
        float dx = pOut.z - g_supervisor.cam0.offset.z;

        // Calculate distance (hypotenuse)
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);

        // Calculate Fog Factor
        // If distance <= fogEnd (renderStateValue2), we might be fully inside or outside depending on logic
        // The assembly logic is: if (dist <= fogEnd) -> Use Full Color.
        if (dist <= g_supervisor.cam0.fogEnd)
            g_renderQuad144[i].diffuse = baseColor;
        else
        {
            // Calculate interpolation factor 'y' (0.0 to 1.0)
            float fogFactor = (g_supervisor.cam0.fogEnd - dist) / fogRange;

            // Clamp and apply fog
            if (fogFactor >= 1.0f)
            {
                // This branch seems to set a specific render state value as color 
                // possibly indicating "full fog" or "no fog" depending on blend mode.
                // Based on asm: mov renderStateValue1 -> d[-1].uv.y+1 (likely color)
                g_renderQuad144[i].diffuse = g_supervisor.currentCam->renderStateValue1;

                // The assembly also sets the 'rhw' or 'x' position byte to 'c' (Alpha of base color)?
                // *(undefined1 *)&(d->pos).x = c; 
                // This part implies a specific hack for fully fogged items or Z-sorting.
            }
            else
            {
                // Interpolate RGB channels towards the Fog Color (fogR, fogG, fogB)
                // The assembly calculates: Base - (Base - Fog) * Factor
                // Or: (Base - FogBase) * Factor?

                // Reconstructing the byte math from assembly:
                // cStack_54 = ROUND(((float)(color & 0xff) - g_supervisor.cam0.fogB) * y);
                // finalBlue = (char)color - cStack_54;

                // Blue
                int deltaB = (int)roundf(((float)b - g_supervisor.cam0.fogB) * fogFactor);
                int finalB = b - deltaB;

                // Green
                int deltaG = (int)roundf(((float)g - g_supervisor.cam0.fogG) * fogFactor);
                int finalG = g - deltaG;

                // Red
                int deltaR = (int)roundf(((float)r - g_supervisor.cam0.fogR) * fogFactor);
                int finalR = r - deltaR;

                // Reassemble color
                g_renderQuad144[i].diffuse = D3DCOLOR_ARGB(a, finalR, finalG, finalB);
            }
        }
    }
    drawVmSprite2D(This, 2, vm);

    // Reset RHW to 1.0 for all vertices after drawing
    g_renderQuad144[3].rhw = 1.0f;
    g_renderQuad144[2].rhw = 1.0f;
    g_renderQuad144[1].rhw = 1.0f;
    g_renderQuad144[0].rhw = 1.0f;
}

int AnmManager::drawVmWithTextureTransform(AnmManager* This, AnmVm* vm)
{
    // Check flags: Bit 0 and Bit 1 must be set, and Alpha (byte 3 of color) must not be 0
    if ((vm->m_flagsLow & 1) && (vm->m_flagsLow & 2) && (vm->m_color0 >> 24) != 0)
    {
        // 0x435620: Check if there are batched sprites waiting to be drawn
        if (This->m_anmVertexBuffers.leftoverSpriteCount != 0)
        {
            // 0x44fd10: Flush existing sprites to maintain draw order
            flushSprites(This);
        }

        uint32_t flags = vm->m_flagsLow;

        // Matrix update logic (same as your version, verified correct)
        if (!(flags & 0x4000) && (flags & 0xC))
        {
            vm->m_localTransformMatrix = vm->m_baseScaleMatrix;
            vm->m_localTransformMatrix._11 *= vm->m_scale.x;
            vm->m_localTransformMatrix._22 *= vm->m_scale.y;
            vm->m_flagsLow &= ~0x8; // Clear scale dirty flag

            D3DXMATRIX tempMatrix;
            if (vm->m_rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&tempMatrix, vm->m_rotation.x);
                D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &tempMatrix);
            }
            if (vm->m_rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&tempMatrix, vm->m_rotation.y);
                D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &tempMatrix);
            }
            if (vm->m_rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&tempMatrix, vm->m_rotation.z);
                D3DXMatrixMultiply(&vm->m_localTransformMatrix, &vm->m_localTransformMatrix, &tempMatrix);
            }
            vm->m_flagsLow &= ~0x4; // Clear rotation dirty flag
        }

        // Initialize World Matrix from Local Matrix
        D3DXMATRIX worldMatrix = vm->m_localTransformMatrix;

        // Position & Anchor Calculation
        // Note: Assembly logic suggests Pos - fabs(Size*Scale).
        // If m_spriteSize is full width, * 0.5f is required for edge anchoring.
        int anchorX = (vm->m_flagsLow >> 18) & 3;
        float finalX = vm->m_entityPos.x + vm->m_pos.x + vm->m_offsetPos.x;

        if (anchorX == 1)
            finalX -= fabsf(vm->m_spriteSize.x * vm->m_scale.x * 0.5f);
        else if (anchorX == 2)
            finalX += fabsf(vm->m_spriteSize.x * vm->m_scale.x * 0.5f);

        worldMatrix._41 = finalX;

        int anchorY = (vm->m_flagsLow >> 20) & 3;
        float finalY = vm->m_entityPos.y + vm->m_pos.y + vm->m_offsetPos.y;

        if (anchorY == 1)
            finalY -= fabsf(vm->m_spriteSize.y * vm->m_scale.y * 0.5f);
        else if (anchorY == 2)
            finalY += fabsf(vm->m_spriteSize.y * vm->m_scale.y * 0.5f);

        worldMatrix._42 = finalY;
        worldMatrix._43 = vm->m_entityPos.z + vm->m_pos.z + vm->m_offsetPos.z;

        // Apply material/colors/render states
        applyRenderStateForVm(This, vm);

        // Set World Transform (State 256 / 0x100)
        g_supervisor.d3dDevice->SetTransform(D3DTS_WORLD, &worldMatrix);

        // Texture Setup
        AnmLoadedD3D* anmLoadedD3D = vm->m_sprite->anmLoadedD3D;
        if (This->m_anmLoadedD3D != anmLoadedD3D)
        {
            This->m_anmLoadedD3D = anmLoadedD3D;
            g_supervisor.d3dDevice->SetTexture(0, anmLoadedD3D->m_texture);
        }

        // Texture Transform (Scroll/Matrix)
        // Assembly accesses offset 0x70 (m_uvScrollPos?) and 0x54 (m_spriteUvQuad[0]?) of AnmVm
        float scrollX = vm->m_uvScrollPos.x;
        if (This->m_cachedSprite != vm->m_sprite || scrollX != 0.0f || vm->m_uvScrollPos.y != 0.0f)
        {
            This->m_cachedSprite = vm->m_sprite;

            // Copy base texture matrix
            D3DXMATRIX texMatrix = vm->m_textureMatrix;

            // Update translation components
            texMatrix._31 = vm->m_uvScrollPos.x + vm->m_spriteUvQuad[0].x;
            texMatrix._32 = vm->m_uvScrollPos.y + vm->m_spriteUvQuad[0].y;

            g_supervisor.d3dDevice->SetTransform(D3DTS_TEXTURE0, &texMatrix);
        }

        if (This->m_haveFlushedSprites != 2)
        {
            // CRITICAL FIX: The assembly uses stride 0x14 (20 bytes), NOT sizeof(RenderVertex144) (28 bytes).
            // This expects a buffer of struct { float x,y,z; float u,v; };
            g_supervisor.d3dDevice->SetStreamSource(0, This->m_d3dVertexBuffer, 0, 20);

            // Set FVF to XYZ | TEX1 (0x102) matches the 20-byte stride.
            g_supervisor.d3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

            g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
            g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            This->m_haveFlushedSprites = 2;
        }

        g_supervisor.d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        return 0;
    }
    return -1;
}

void AnmManager::drawVm(AnmManager* This, AnmVm* vm)
{
    uint8_t alpha = (vm->m_color0 >> 24) & 0xff;
    if ((vm->m_flagsLow & 0x3) != 0x3 || alpha == 0)
        return;
    uint32_t mode = vm->m_flagsLow >> 0x16 & 0xf;
    //printf("drawing vm with mode %d\n", mode);

    switch (mode)
    {
    case 0:
    {
        vm->writeSpriteCharactersWithoutRot(
            vm,
            &g_renderQuad144[0],
            &g_renderQuad144[1],
            &g_renderQuad144[2],
            &g_renderQuad144[3]
        );
        drawVmSprite2D(This, 1, vm);
        break;
    }
    case 1:
    {
        if (vm->m_rotation.z == 0.0)
        {
            vm->writeSpriteCharactersWithoutRot(
                vm,
                &g_renderQuad144[0],
                &g_renderQuad144[1],
                &g_renderQuad144[2],
                &g_renderQuad144[3]);
            drawVmSprite2D(This, 1, vm);
        }
        else
        {
            vm->applyZRotationToQuadCorners(
                vm,
                &g_renderQuad144[0],
                &g_renderQuad144[1],
                &g_renderQuad144[2],
                &g_renderQuad144[3]
            );
            drawVmSprite2D(This, 0, vm);
        }
        break;
    }
    case 2:
    {
        vm->writeSpriteCharacters(
            vm,
            &g_renderQuad144[0],
            &g_renderQuad144[1],
            &g_renderQuad144[2],
            &g_renderQuad144[3]
        );
        drawVmSprite2D(This, 0, vm);
        break;
    }
    case 3:
    {
        if (vm->m_rotation.z == 0.0)
        {
            vm->writeSpriteCharacters(
                vm,
                &g_renderQuad144[0],
                &g_renderQuad144[1],
                &g_renderQuad144[2],
                &g_renderQuad144[3]
            );
            drawVmSprite2D(This, 0, vm);
        }
        else
        {
            vm->applyZRotationToQuadCorners(
                vm,
                &g_renderQuad144[0],
                &g_renderQuad144[1],
                &g_renderQuad144[2],
                &g_renderQuad144[3]
            );
            drawVmSprite2D(This, 0, vm);
        }
        break;
    }
    case 4:
    {
        vm->projectQuadCornersThroughCameraViewport(vm);
        drawVmSprite2D(This, 0, vm);
        break;
    }
    case 5:
    {
        updateWorldMatrixAndProjectQuadCorners(This, vm);
        drawVmSprite2D(This, 0, vm);
        g_renderQuad144[0].rhw = 1.0;
        g_renderQuad144[1].rhw = 1.0;
        g_renderQuad144[2].rhw = 1.0;
        g_renderQuad144[3].rhw = 1.0;
        break;
    }
    case 6:
        drawVmWithFog(This, vm);
        break;
    case 7:
        transformAndDraw(This, vm);
        break;
    case 8:
    {
#if 0
        using ltoFunc = Signature<int, AnmManager*, AnmVm*>;
        using ltoSig = Storage<
            Returns<RegCode::EAX>,   // Return      -> EAX
            Stack<0x4>,              // AnmManager* -> Stack[0x4]
            EBX                      // AnmVm*      -> EBX
        >;
        static auto game_drawVmWithTextureTransform = createCustomCallingConvention<ltoSig, ltoFunc>(0x4513a0);
        game_drawVmWithTextureTransform(This, vm);
#else
        drawVmWithTextureTransform(This, vm);
#endif
        break;
    }
    case 9:
    case 0xc:
    case 0xd:
        drawVmTriangleStrip(This, vm, vm->m_specialRenderData, vm->m_intVars[0] * 2);
        break;
    case 0xb:
        drawVmTriangleFan(This, vm, vm->m_specialRenderData, vm->m_intVars[0] * 2);
        break;
    default:
        break;
    }
}

// 0x445460
void AnmManager::blitTextures(AnmManager* This)
{
    for (int i = 0; i < 4; ++i)
    {
        BlitParams* blitParams = &This->m_blitParamsArray[i];
        if (blitParams->anmLoadedIndex >= 0)
        {
            This->blitTextureToSurface(This, blitParams);
            blitParams->anmLoadedIndex = -1;
        }
    }
}