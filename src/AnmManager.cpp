#include "AnmManager.h"

AnmManager* g_anmManager;
RenderVertex144 g_renderQuad144[4];

// 0x454360
AnmLoaded* AnmManager::preloadAnm(AnmManager* This, int anmIdx, const char* anmFileName)
{
    // Return existing animation if already loaded
    if (This->m_loadedAnms[anmIdx] != nullptr)
    {
        printf("::preloadAnm already loaded: %s\n", anmFileName);
        return This->m_loadedAnms[anmIdx];
    }

    // Load animation from memory
    AnmLoaded* anmLoaded = preloadAnmFromMemory(This, anmFileName, anmIdx);
    if (anmLoaded == nullptr)
        return nullptr;

    anmLoaded->m_anmsLoading = 1;
    while (anmLoaded->m_anmsLoading != 0 && (g_supervisor.criticalSectionFlag & 0x180) == 0) // Wait until animation loading is complete
        Sleep(1);

    printf("::preloadAnmEnd: %s\n", anmFileName);
    return anmLoaded;
}

// Function to process a single ANM chunk
int openAnmLoaded(AnmLoaded* anmLoaded, AnmHeader* chunk, int chunkIndex)
{
    // Check if the version is 7 (specific to this ANM format)
    if (chunk->version != 7)
    {
        printf("\n");
        return -1;
    }

    // If hasdata is 0, load an external texture file
    if (chunk->hasData == 0)
    {
        char* name = (char*)chunk + chunk->nameOffset;

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

// 0x454190
// Function to preload an ANM file into memory
AnmLoaded* AnmManager::preloadAnmFromMemory(AnmManager* This, const char* anmFilePath, int m_anmSlotIndex)
{
    // Log the preload action
    printf("::preloadAnim : %s\n", anmFilePath);

    // Check if the slot index is within bounds (0-31)
    if (m_anmSlotIndex >= 32)
    {
        printf("\n");
        return nullptr;
    }

    // Resolve the file path
    char resolvedFilePath[260];
    sprintf_s(resolvedFilePath, "%s", anmFilePath);

    // Open the ANM file (memory-mapped)
    AnmHeader* header = (AnmHeader*)openFile(resolvedFilePath, nullptr, 0);
    if (!header)
    {
        printf("\n");
        return nullptr;
    }

    // Allocate and initialize the AnmLoaded structure
    AnmLoaded* anmLoaded = (AnmLoaded*)malloc(sizeof(AnmLoaded));
    if (anmLoaded)
        memset(anmLoaded, 0, sizeof(AnmLoaded));
    This->m_loadedAnms[m_anmSlotIndex] = anmLoaded;

    // Set initial fields
    anmLoaded->m_anmSlotIndex = m_anmSlotIndex;
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
    anmLoaded->m_header = new AnmHeader[processedCount]();
    anmLoaded->m_keyframeData = new AnmLoadedSprite[numSprites]();
    anmLoaded->m_spriteData = new char[numScripts * 4]();
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
            printf("\n");
            free(anmLoaded->m_header);
            free(anmLoaded->m_keyframeData);
            free(anmLoaded->m_spriteData);
            free(anmLoaded);
            This->m_loadedAnms[m_anmSlotIndex] = nullptr;
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
void AnmManager::releaseTextures(AnmManager* This)
{
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

// 0x44fd10
void AnmManager::flushSprites(AnmManager* This)
{
    if ((This->m_anmVertexBuffers).leftoverSpriteCount == 0)
        return;

    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->SetFVF(0x144);

    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLELIST,
        This->m_anmVertexBuffers.leftoverSpriteCount * 2,
        This->m_anmVertexBuffers.spriteRenderCursor,
        0x1c
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
                    g_supervisor.d3dPresetParameters.BackBufferWidth,
                    g_supervisor.d3dPresetParameters.BackBufferHeight,
                    1,
                    1,
                    g_supervisor.d3dPresetParameters.BackBufferFormat,
                    D3DPOOL_DEFAULT,
                    &anmLoadedD3D->m_texture,
                    NULL
                );

                anmLoadedD3D->m_bytesPerPixel = (g_supervisor.d3dPresetParameters.BackBufferFormat == D3DFMT_X8R8G8B8) * 2 + 2;
            }
        }
    }
}

// 0x450e20
int AnmManager::updateWorldMatrixAndProjectQuadCorners(AnmManager* This, AnmVm* vm)
{
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

// 0x44f710
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
        case 2:  // Subtract blending?
            srcBlend = D3DBLEND_ZERO;
            destBlend = D3DBLEND_INVSRCCOLOR;
            break;
        case 3:  // Replace?
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

// 0x44fda0
int AnmManager::writeSprite(AnmManager* This, RenderVertex144* someVertices)
{
    RenderVertex144* cursor = This->m_anmVertexBuffers.spriteWriteCursor;
    cursor[0] = someVertices[0];
    cursor[1] = someVertices[1];
    cursor[2] = someVertices[2];
    cursor[3] = someVertices[1];
    cursor[4] = someVertices[2];
    cursor[5] = someVertices[3];
    This->m_anmVertexBuffers.spriteWriteCursor += 6;
    This->m_anmVertexBuffers.leftoverSpriteCount += 1;
    return 0;
}

// 0x44f880
void AnmManager::drawVmSprite2D(AnmManager* This, uint32_t layer, AnmVm* anmVm)
{
    // Store original layer for later use
    uint32_t originalLayer = layer;

    // Apply offsets to all quad positions
    for (int i = 0; i < 4; ++i)
    {
        g_renderQuad144[i].pos.x += static_cast<float>(This->m_globalRenderQuadOffsetX);
        g_renderQuad144[i].pos.y += static_cast<float>(This->m_globalRenderQuadOffsetY);
    }

    // Pixel alignment if layer is odd
    if (layer & 1)
    {
        // Round and offset x coordinates for bottom-left and bottom-right
        g_renderQuad144[0].pos.x = static_cast<float>(g_renderQuad144[0].pos.x) - 0.5f;
        g_renderQuad144[1].pos.x = static_cast<float>(g_renderQuad144[1].pos.x) - 0.5f;
        // Round and offset y coordinates for bottom-left and top-left
        g_renderQuad144[0].pos.y = static_cast<float>(g_renderQuad144[0].pos.y) - 0.5f;
        g_renderQuad144[2].pos.y = static_cast<float>(g_renderQuad144[2].pos.y) - 0.5f;
        // Copy adjusted y to bottom-right and top-right
        g_renderQuad144[1].pos.y = g_renderQuad144[0].pos.y;
        g_renderQuad144[3].pos.y = g_renderQuad144[2].pos.y;
        // Copy adjusted x to top-left and top-right
        g_renderQuad144[2].pos.x = g_renderQuad144[0].pos.x;
        g_renderQuad144[3].pos.x = g_renderQuad144[1].pos.x;
    }

    // Apply UVs with scroll offset
    for (int i = 0; i < 4; ++i)
    {
        g_renderQuad144[i].uv.x = anmVm->m_spriteUvQuad[i].x + anmVm->m_uvScrollPos.x;
        g_renderQuad144[i].uv.y = anmVm->m_spriteUvQuad[i].y + anmVm->m_uvScrollPos.y;
    }

    // Compute bounding box max x
    float maxX = g_renderQuad144[0].pos.x;
    maxX = std::max(maxX, g_renderQuad144[1].pos.x);
    maxX = std::max(maxX, g_renderQuad144[2].pos.x);
    maxX = std::max(maxX, g_renderQuad144[3].pos.x);

    // Compute bounding box max y
    float maxY = g_renderQuad144[0].pos.y;
    maxY = std::max(maxY, g_renderQuad144[1].pos.y);
    maxY = std::max(maxY, g_renderQuad144[2].pos.y);
    maxY = std::max(maxY, g_renderQuad144[3].pos.y);

    // Compute bounding box min x
    float minX = g_renderQuad144[0].pos.x;
    minX = std::min(minX, g_renderQuad144[1].pos.x);
    minX = std::min(minX, g_renderQuad144[2].pos.x);
    minX = std::min(minX, g_renderQuad144[3].pos.x);

    // Compute bounding box min y
    float minY = g_renderQuad144[0].pos.y;
    minY = std::min(minY, g_renderQuad144[1].pos.y);
    minY = std::min(minY, g_renderQuad144[2].pos.y);
    minY = std::min(minY, g_renderQuad144[3].pos.y);

    // Get viewport dimensions
    const D3DVIEWPORT9& viewport = g_supervisor.currentCam->viewport;
    float viewportLeft = static_cast<float>(viewport.X);
    float viewportTop = static_cast<float>(viewport.Y);
    float viewportRight = viewportLeft + static_cast<float>(viewport.Width);
    float viewportBottom = viewportTop + static_cast<float>(viewport.Height);

    // Cull if outside viewport
    if (viewportLeft > maxX || viewportTop > maxY || viewportRight < minX || viewportBottom < minY)
        return;

    // Handle texture change
    IDirect3DTexture9** newTex = &anmVm->m_sprite->anmLoadedD3D->m_texture;
    if (This->m_tex != newTex)
    {
        This->m_tex = newTex;
        flushSprites(This);
        g_supervisor.d3dDevice->SetTexture(0, *This->m_tex);
    }

    // Flush if needed
    if (This->m_haveFlushedSprites != 1)
    {
        flushSprites(This);
        This->m_haveFlushedSprites = 1;
    }

    // Set colors
    D3DCOLOR c0 = g_renderQuad144[0].diffuse;
    D3DCOLOR c1 = g_renderQuad144[1].diffuse;
    D3DCOLOR c2 = g_renderQuad144[2].diffuse;
    D3DCOLOR c3 = g_renderQuad144[3].diffuse;

    if ((originalLayer & 2) == 0)
    {
        c0 = (anmVm->m_flagsLow & 0x8000) ? anmVm->m_color0 : anmVm->m_color1;
        c1 = c0;
        c2 = c0;
        c3 = c0;

        if (This->m_rebuildColorFlag != 0)
        {
            // Extract channels (assuming ARGB format?)
            //FIXME: Confirm this??
            uint8_t a = (c0 >> 24) & 0xFF;
            uint8_t r = (c0 >> 16) & 0xFF;
            uint8_t g = (c0 >> 8) & 0xFF;
            uint8_t b = c0 & 0xFF;

            // Clamp each channel
            r = modulateColorComponent(r, This->m_scaleR);
            g = modulateColorComponent(g, This->m_scaleG);
            b = modulateColorComponent(b, This->m_scaleB);
            a = modulateColorComponent(a, This->m_scaleA);

            // Rebuild color
            c0 = (a << 24) | (r << 16) | (g << 8) | b;
            c1 = c0;
            c2 = c0;
            c3 = c0;
        }
    }

    // Assign colors back to vertices
    g_renderQuad144[0].diffuse = c0;
    g_renderQuad144[1].diffuse = c1;
    g_renderQuad144[2].diffuse = c2;
    g_renderQuad144[3].diffuse = c3;

    setupRenderStateForVm(This, anmVm);
    writeSprite(This, g_renderQuad144);
}

// 0x44f490
uint32_t AnmManager::modulateColorComponent(uint16_t base, uint16_t factor)
{
    uint32_t i = (base & 0xff) * (factor & 0xff) >> 7;
    if (0xff < i)
        i = 0xff;
    return i;
}

// 0x450b00
int AnmManager::drawVmWithFog(AnmManager* This, AnmVm* vm)
{
    if (vm->projectQuadCornersThroughCameraViewport(vm) != 0)
        return -1;

    float fogStart = g_supervisor.currentCam->f0;
    float fogEnd = g_supervisor.currentCam->f1;
    float cullDistance = g_supervisor.cam0.f0;  // Assuming this is the fog start or end, but based on logic it's the fog start for comparison

    D3DCOLOR color = (vm->m_flagsLow & 0x8000) ? vm->m_color1 : vm->m_color0;

    float x = vm->m_pos.x + vm->m_entityPos.x + vm->m_offsetPos.x - g_supervisor.currentCam->offset.x;
    float y = vm->m_pos.y + vm->m_entityPos.y + vm->m_offsetPos.y - g_supervisor.currentCam->offset.y;
    float z = vm->m_pos.z + vm->m_entityPos.z + vm->m_offsetPos.z - g_supervisor.currentCam->offset.z;

    float dist = sqrtf(x * x + y * y + z * z);

    if (This->m_rebuildColorFlag != 0) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint8_t a = (color >> 24) & 0xFF;

        r = std::min(static_cast<uint32_t>(This->m_scaleR * r) >> 7, 255u);
        g = std::min(static_cast<uint32_t>(This->m_scaleG * g) >> 7, 255u);
        b = std::min(static_cast<uint32_t>(This->m_scaleB * b) >> 7, 255u);
        a = std::min(static_cast<uint32_t>(This->m_scaleA * a) >> 7, 255u);

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

// 0x44f4b0
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

int AnmManager::drawVmTriangleStrip(AnmManager* This, AnmVm* vm, RenderVertex144* vertexBuffer, uint32_t vertexCount)
{
    uint8_t alpha = (vm->m_color0 >> 24) & 0xff;
    if ((vm->m_flagsLow & 0x3) != 0x3 || alpha == 0)
        return -1;

    if (This->m_anmVertexBuffers.leftoverSpriteCount != 0)
        flushSprites(This);

    IDirect3DTexture9** tex = &vm->m_sprite->anmLoadedD3D->m_texture;
    if (This->m_tex != tex)
    {
        This->m_tex = tex;
        g_supervisor.d3dDevice->SetTexture(0, *tex);
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
        vertexBuffer,
        0x1c
    );
    return 0;
}

// 0x451e10
int AnmManager::drawVmTriangleFan(AnmManager* This, AnmVm* vm, RenderVertex144* vertexBuffer, uint32_t vertexCount)
{
    if (This->m_anmVertexBuffers.leftoverSpriteCount != 0)
        flushSprites(This);

    if (This->m_haveFlushedSprites != 3)
    {
        g_supervisor.d3dDevice->SetFVF(0x144);
        This->m_haveFlushedSprites = 3;
    }
    setupRenderStateForVm(This, vm);

    IDirect3DTexture9** tex = &vm->m_sprite->anmLoadedD3D->m_texture;
    if (This->m_tex != tex)
    {
        This->m_tex = tex;
        g_supervisor.d3dDevice->SetTexture(0, *tex);
    }

    flushSprites(This);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLEFAN,
        vertexCount - 2,
        vertexBuffer,
        0x1c
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

// 0x451ef0
void AnmManager::drawVm(AnmManager* This, AnmVm* vm)
{
    uint8_t alpha = (vm->m_color0 >> 24) & 0xff;
    if ((vm->m_flagsLow & 0x3) != 0x3 || alpha == 0)
        return;

    switch (vm->m_flagsLow >> 0x16 & 0xf)
    {
    case 0:
        //writeSpriteCharacterWithoutRotAndDrawVmSprite2D(vm);
        break;
    case 1:
        //writeSpriteCharactersWithoutRotAndApplyZRotationToQuadCorners(vm);
        break;
    case 2:
        //writeSpriteCharactersAndDrawVmSprite2D(This,vm);
        break;
    case 3:
        //writeSpriteCharactersAndDrawVmSprite2DAndApplyZRotationToQuadCorners(This,vm);
        break;
    case 4:
        //projectQuadCornersThroughCameraViewportAndDrawVmSprite2D(This,vm);
        break;
    case 5:
        //updateWorldMatrixAndProjectBoundingBoxAndDrawVmSprite2DAndSetFloats(This,vm);
        break;
    case 6:
        drawVmWithFog(This, vm);
        break;
    case 7:
        // transformAndDraw(vm);
        break;
    case 8:
        //sub_004513a0(vm);
        break;
    case 9:
    case 0xc:
    case 0xd:
        //drawVmTriangleStrip(This, vm, vm->m_specialRenderData, vm->m_intVars[0] * 2);
        break;
    case 0xb:
        //drawVmTriangleFan(This, vm, vm->m_specialRenderData, vm->m_intVars[0] * 2);
        break;
    default:
        break;
    }
}

/* Global Functions */

// 0x445460
void blitTextures()
{
    for (int i = 0; i < 4; ++i)
    {
        BlitParams* blitParams = &g_anmManager->m_blitParamsArray[i];
        if (blitParams->anmLoadedIndex >= 0)
        {
            AnmManager::blitTextureToSurface(g_anmManager, blitParams);
            blitParams->anmLoadedIndex = -1;
        }
    }
}