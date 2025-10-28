#include "AnmManager.h"
#include "AnmVm.h"
#include "Chireiden.h"

AnmManager* g_anmManager;
RenderVertex144 g_renderQuad144[4];

// 0x454360
AnmLoaded* AnmManager::preloadAnm(int anmIdx, const char* anmFileName)
{
    // Return existing animation if already loaded
    if (g_anmManager->loadedAnms[anmIdx] != nullptr)
    {
        printf("::preloadAnm already loaded: %s\n", anmFileName);
        return g_anmManager->loadedAnms[anmIdx];
    }

    // Load animation from memory
    AnmLoaded* anmLoaded = preloadAnmFromMemory(anmFileName, anmIdx);
    if (anmLoaded == nullptr)
        return nullptr;

    anmLoaded->anmsLoading = 1;
    while (anmLoaded->anmsLoading != 0 && (g_supervisor.criticalSectionFlag & 0x180) == 0) // Wait until animation loading is complete
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
        printf("アニメのバージョンが違います\n");
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
                printf("テクスチャ %s が読み込めません。データが失われてるか壊れています\n", name);
                return -1;
            }
            // Store the loaded file and size in the chunk data buffer
            anmLoaded->anmLoadedD3D[chunkIndex].m_srcData = memoryMappedFile;
            anmLoaded->anmLoadedD3D[chunkIndex].m_srcDataSize = outSize;
        }
    }
    return 1;
}

// 0x454190
// Function to preload an ANM file into memory
AnmLoaded* AnmManager::preloadAnmFromMemory(const char* anmFilePath, int anmSlotIndex)
{
    // Log the preload action
    printf("::preloadAnim : %s\n", anmFilePath);

    // Check if the slot index is within bounds (0-31)
    if (anmSlotIndex >= 32)
    {
        printf("テクスチャ格納先が足りません\n");
        return nullptr;
    }

    // Resolve the file path
    char resolvedFilePath[260];
    sprintf_s(resolvedFilePath, "%s", anmFilePath);

    // Open the ANM file (memory-mapped)
    AnmHeader* header = (AnmHeader*)openFile(resolvedFilePath, nullptr, 0);
    if (!header)
    {
        printf("アニメが読み込めません。データが失われてるか壊れています\n");
        return nullptr;
    }

    // Allocate and initialize the AnmLoaded structure
    AnmLoaded* anmLoaded = (AnmLoaded*) malloc(sizeof(AnmLoaded));
    if (anmLoaded)
        memset(anmLoaded, 0, sizeof(AnmLoaded));
    loadedAnms[anmSlotIndex] = anmLoaded;

    // Set initial fields
    anmLoaded->anmSlotIndex = anmSlotIndex;
    anmLoaded->header = header;
    strcpy_s(anmLoaded->filePath, anmFilePath);

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
    anmLoaded->numAnmLoadedD3Ds = processedCount;
    anmLoaded->header = new AnmHeader[processedCount]();
    anmLoaded->keyframeData = new AnmLoadedSprite[numSprites]();
    anmLoaded->spriteData = new char[numScripts * 4]();
    anmLoaded->numScripts = numScripts;
    anmLoaded->numSprites = numSprites;

    // Process each chunk
    int chunkIndex = 0;
    currentChunk = header;
    while (currentChunk != nullptr)
    {
        int result = openAnmLoaded(anmLoaded, currentChunk, chunkIndex);
        if (result < 0)
        {
            printf("アニメが読み込めません。データが失われてるか壊れています\n");
            free(anmLoaded->header);
            free(anmLoaded->keyframeData);
            free(anmLoaded->spriteData);
            free(anmLoaded);
            loadedAnms[anmSlotIndex] = nullptr;
            return nullptr;
        }
        chunkIndex++;
        if (currentChunk->nextOffset == 0) 
            break;
        currentChunk = (AnmHeader*)((byte*)currentChunk + currentChunk->nextOffset);
    }
    return anmLoaded;
}

void AnmManager::markAnmLoadedAsReleasedInVmList(AnmLoaded* anmLoaded)
{
    AnmVmList* temp = this->primaryGlobalNext;
    while (temp)
    {
        AnmVmList* next = temp->next;
        AnmVm* entry = temp->entry;
        temp = next;
        if (entry->m_anmLoaded == anmLoaded)
        entry->m_flagsLow |= 0x4000000;
    }
    temp = this->secondaryGlobalNext;
    while (temp)
    {
        AnmVmList* next = temp->next;
        AnmVm* entry = temp->entry;
        temp = next;
        if (entry->m_anmLoaded == anmLoaded)
            entry->m_flagsLow |= 0x4000000;
    }
}

AnmVm* AnmManager::allocateVm()
{
    int index = g_anmManager->nextBulkVmIndex;
    AnmVm* vm;

    if (g_anmManager->bulkVmsIsAlive[index] == 0)
    {
        vm = &g_anmManager->bulkVms[index];
        g_anmManager->bulkVmsIsAlive[index] = 1;
        g_anmManager->nextBulkVmIndex = g_anmManager->getNextBulkVmIndex(index);
    }
    else
    {
        int nextIdx = getNextBulkVmIndex(index);
        if (g_anmManager->bulkVmsIsAlive[nextIdx] == 0)
        {
            vm = &g_anmManager->bulkVms[nextIdx];
            g_anmManager->bulkVmsIsAlive[nextIdx] = 1;
        }
        else
        {
            vm = new AnmVm();
            if (vm)
                vm->initialize();
        }
        g_anmManager->nextBulkVmIndex = getNextBulkVmIndex(nextIdx);
    }
    return vm;
}

// 0x445320
void AnmManager::releaseTextures()
{
    for (int i = 0; i < NUM_ANM_LOADEDS; ++i)
    {
        AnmLoaded* anmLoaded = g_anmManager->loadedAnms[i];
        if (anmLoaded == nullptr)
            continue; // Skip null entries

        AnmLoadedD3D* anmLoadedD3Ds = anmLoaded->anmLoadedD3D;
        int numAnmLoadedD3Ds = anmLoaded->numAnmLoadedD3Ds;
        for (int j = 0; j < numAnmLoadedD3Ds; ++j)
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
void AnmManager::flushSprites()
{
    if ((this->m_vertexBuffers).leftoverSpriteCount == 0)
        return;

    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->SetFVF(0x144);

    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLELIST,
        this->m_vertexBuffers.leftoverSpriteCount * 2,
        this->m_vertexBuffers.spriteRenderCursor,
        0x1c
    );

    ++this->refreshCounter;
    this->m_vertexBuffers.spriteRenderCursor = this->m_vertexBuffers.spriteWriteCursor;
    this->m_vertexBuffers.leftoverSpriteCount = 0;
}

// 0x454ec0
void AnmManager::blitTextureToSurface(BlitParams* blitParams)
{
    // Early return if the texture is null
    if (!this->loadedAnms[blitParams->anmLoadedIndex]->anmLoadedD3D[blitParams->anmLoadedD3dIndex].m_texture)
        return;

    // Ensure pending sprite operations are completed
    flushSprites();

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
    AnmLoadedD3D* anmLoadedD3d = this->loadedAnms[blitParams->anmLoadedIndex]->anmLoadedD3D;
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
void AnmManager::createD3DTextures()
{
    for (int i = 0; i < 32; ++i)
    {
        AnmLoaded* loadedAnm = g_anmManager->loadedAnms[i];
        if (!loadedAnm)
            continue;

        if (loadedAnm->numAnmLoadedD3Ds <= 0)
            continue;

        for (int j = 0; j < loadedAnm->numAnmLoadedD3Ds; ++j)
        {
            AnmLoadedD3D *anmLoadedD3D = &loadedAnm->anmLoadedD3D[j];
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
int AnmManager::updateWorldMatrixAndProjectQuadCorners(AnmVm* vm)
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
    D3DXVec3Project(
        &g_bottomLeftDrawCorner,
        &vecBottomLeft,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &g_bottomRightDrawCorner,
        &vecBottomRight,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &g_topLeftDrawCorner,
        &vecTopLeft,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    D3DXVec3Project(
        &g_topRightDrawCorner,
        &vecTopRight,
        &g_supervisor.currentCam->viewport,
        &g_supervisor.currentCam->projectionMatrix,
        &g_supervisor.currentCam->viewMatrix,
        &worldMatrix
    );

    memcpy(&m_currentWorldMatrix, &worldMatrix, sizeof(D3DXMATRIX));
    return 0;
}

void AnmManager::drawPrimitiveImmediate(AnmVm* vm, void* vertexStreamZeroData, uint32_t primitiveCount)
{
    if (m_vertexBuffers.leftoverSpriteCount != 0)
        flushSprites();
   
    if (m_haveFlushedSprites != 3)
    {
        g_supervisor.d3dDevice->SetFVF(0x144);
        m_haveFlushedSprites = 3;
    }

    setupRenderStateForVm(vm);

    IDirect3DTexture9** tex = &vm->m_sprite->anmLoadedD3D->m_texture;
    if (m_tex != tex)
    {
        m_tex = tex;
        g_supervisor.d3dDevice->SetTexture(0, *tex);
    }
    flushSprites(); // this time with g_anmManager?

    g_supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, primitiveCount, vertexStreamZeroData, 0x1c);
}

// 0x44f710
void AnmManager::setupRenderStateForVm(AnmVm* vm)
{
    uint8_t mode = (vm->m_flagsLow >> 4) & 0x7;
    if (renderStateMode != mode)
    {
        flushSprites();
        renderStateMode = mode;
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
    if (usePointFilter != usefilter)
    {
        flushSprites();
        usePointFilter = usefilter;
        D3DTEXTUREFILTERTYPE filterType = usePointFilter ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, filterType);
        g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, filterType);
    }
    someCounter++;
}

// 0x44fda0
int AnmManager::writeSprite(RenderVertex144* someVertices)
{
    RenderVertex144* cursor = m_vertexBuffers.spriteWriteCursor;
    cursor[0] = someVertices[0];
    cursor[1] = someVertices[1];
    cursor[2] = someVertices[2];
    cursor[3] = someVertices[1];
    cursor[4] = someVertices[2];
    cursor[5] = someVertices[3];
    m_vertexBuffers.spriteWriteCursor += 6;
    m_vertexBuffers.leftoverSpriteCount += 1;
    return 0;
}

int AnmManager::drawVmSprite2D(uint32_t layer, AnmVm* anmVm)
{
    // Store original layer for later use
    uint32_t originalLayer = layer;

    // Apply offsets to all quad positions
    for (int i = 0; i < 4; ++i)
    {
        g_renderQuad144[i].transformedPos.x += static_cast<float>(globalRenderQuadOffsetX);
        g_renderQuad144[i].transformedPos.y += static_cast<float>(globalRenderQuadOffsetY);
    }

    // Pixel alignment if layer is odd
    if (layer & 1)
    {
        // Round and offset x coordinates for bottom-left and bottom-right
        g_renderQuad144[0].transformedPos.x = static_cast<float>(g_renderQuad144[0].transformedPos.x) - 0.5f;
        g_renderQuad144[1].transformedPos.x = static_cast<float>(g_renderQuad144[1].transformedPos.x) - 0.5f;
        // Round and offset y coordinates for bottom-left and top-left
        g_renderQuad144[0].transformedPos.y = static_cast<float>(g_renderQuad144[0].transformedPos.y) - 0.5f;
        g_renderQuad144[2].transformedPos.y = static_cast<float>(g_renderQuad144[2].transformedPos.y) - 0.5f;
        // Copy adjusted y to bottom-right and top-right
        g_renderQuad144[1].transformedPos.y = g_renderQuad144[0].transformedPos.y;
        g_renderQuad144[3].transformedPos.y = g_renderQuad144[2].transformedPos.y;
        // Copy adjusted x to top-left and top-right
        g_renderQuad144[2].transformedPos.x = g_renderQuad144[0].transformedPos.x;
        g_renderQuad144[3].transformedPos.x = g_renderQuad144[1].transformedPos.x;
    }

    // Apply UVs with scroll offset
    for (int i = 0; i < 4; ++i)
    {
        g_renderQuad144[i].textureUv.x = anmVm->m_spriteUvQuad[i].x + anmVm->m_uvScrollPos.x;
        g_renderQuad144[i].textureUv.y = anmVm->m_spriteUvQuad[i].y + anmVm->m_uvScrollPos.y;
    }

    // Compute bounding box max x
    float maxX = g_renderQuad144[0].transformedPos.x;
    maxX = std::max(maxX, g_renderQuad144[1].transformedPos.x);
    maxX = std::max(maxX, g_renderQuad144[2].transformedPos.x);
    maxX = std::max(maxX, g_renderQuad144[3].transformedPos.x);

    // Compute bounding box max y
    float maxY = g_renderQuad144[0].transformedPos.y;
    maxY = std::max(maxY, g_renderQuad144[1].transformedPos.y);
    maxY = std::max(maxY, g_renderQuad144[2].transformedPos.y);
    maxY = std::max(maxY, g_renderQuad144[3].transformedPos.y);

    // Compute bounding box min x
    float minX = g_renderQuad144[0].transformedPos.x;
    minX = std::min(minX, g_renderQuad144[1].transformedPos.x);
    minX = std::min(minX, g_renderQuad144[2].transformedPos.x);
    minX = std::min(minX, g_renderQuad144[3].transformedPos.x);

    // Compute bounding box min y
    float minY = g_renderQuad144[0].transformedPos.y;
    minY = std::min(minY, g_renderQuad144[1].transformedPos.y);
    minY = std::min(minY, g_renderQuad144[2].transformedPos.y);
    minY = std::min(minY, g_renderQuad144[3].transformedPos.y);

    // Get viewport dimensions
    const auto& viewport = g_supervisor.currentCam->viewport;
    float viewportLeft = static_cast<float>(viewport.X);
    float viewportTop = static_cast<float>(viewport.Y);
    float viewportRight = viewportLeft + static_cast<float>(viewport.Width);
    float viewportBottom = viewportTop + static_cast<float>(viewport.Height);

    // Cull if outside viewport
    if (viewportLeft > maxX || viewportTop > maxY || viewportRight < minX || viewportBottom < minY) {
        return 0;
    }

    // Handle texture change
    IDirect3DTexture9** newTex = &anmVm->m_sprite->anmLoadedD3D->m_texture;
    if (m_tex != newTex)
    {
        m_tex = newTex;
        flushSprites();
        g_supervisor.d3dDevice->SetTexture(0, *m_tex);
    }

    // Flush if needed
    if (m_haveFlushedSprites != 1)
    {
        flushSprites();
        m_haveFlushedSprites = 1;
    }

    // Set colors
    D3DCOLOR c0 = g_renderQuad144[0].diffuseColor;
    D3DCOLOR c1 = g_renderQuad144[1].diffuseColor;
    D3DCOLOR c2 = g_renderQuad144[2].diffuseColor;
    D3DCOLOR c3 = g_renderQuad144[3].diffuseColor;

    if ((originalLayer & 2) == 0)
    {
        c0 = (anmVm->m_flagsLow & 0x8000) ? anmVm->m_color0 : anmVm->m_color1;
        c1 = c0;
        c2 = c0;
        c3 = c0;

        if (m_rebuildColorFlag != 0)
        {
            // Extract channels (assuming ARGB format?)
            //FIXME: Confirm this??
            uint8_t a = (c0 >> 24) & 0xFF;
            uint8_t r = (c0 >> 16) & 0xFF;
            uint8_t g = (c0 >> 8) & 0xFF;
            uint8_t b = c0 & 0xFF;

            // Clamp each channel
            r = modulateColorComponent(r, scaleR);
            g = modulateColorComponent(g, scaleG);
            b = modulateColorComponent(b, scaleB);
            a = modulateColorComponent(a, scaleA);

            // Rebuild color
            c0 = (a << 24) | (r << 16) | (g << 8) | b;
            c1 = c0;
            c2 = c0;
            c3 = c0;
        }
    }

    // Assign colors back to vertices
    g_renderQuad144[0].diffuseColor = c0;
    g_renderQuad144[1].diffuseColor = c1;
    g_renderQuad144[2].diffuseColor = c2;
    g_renderQuad144[3].diffuseColor = c3;

    setupRenderStateForVm(anmVm);
    writeSprite(g_renderQuad144);
    return 0;
}

uint32_t AnmManager::modulateColorComponent(uint16_t base, uint16_t factor)
{
    uint32_t i = (base & 0xff) * (factor & 0xff) >> 7;
    if (0xff < i)
        i = 0xff;
    return i;
}

// 0x450b00
int AnmManager::drawVmWithFog(AnmVm* vm)
{
    if (vm->projectQuadCornersThroughCameraViewport() != 0)
        return -1;

    float fogStart = g_supervisor.currentCam->f0;
    float fogEnd = g_supervisor.currentCam->f1;
    float cullDistance = g_supervisor.cam0.f0;  // Assuming this is the fog start or end, but based on logic it's the fog start for comparison

    D3DCOLOR color = (vm->m_flagsLow & 0x8000) ? vm->m_color1 : vm->m_color0;

    float x = vm->m_pos.x + vm->m_entityPos.x + vm->m_offsetPos.x - g_supervisor.currentCam->offset.x;
    float y = vm->m_pos.y + vm->m_entityPos.y + vm->m_offsetPos.y - g_supervisor.currentCam->offset.y;
    float z = vm->m_pos.z + vm->m_entityPos.z + vm->m_offsetPos.z - g_supervisor.currentCam->offset.z;

    float dist = sqrtf(x * x + y * y + z * z);

    if (m_rebuildColorFlag != 0) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint8_t a = (color >> 24) & 0xFF;

        r = std::min(static_cast<uint32_t>(scaleR * r) >> 7, 255u);
        g = std::min(static_cast<uint32_t>(scaleG * g) >> 7, 255u);
        b = std::min(static_cast<uint32_t>(scaleB * b) >> 7, 255u);
        a = std::min(static_cast<uint32_t>(scaleA * a) >> 7, 255u);

        color = (a << 24) | (r << 16) | (g << 8) | b;
    }

    if (dist <= cullDistance)
        g_renderQuad144[0].diffuseColor = color;
    else
    {
        float denominator = fogStart - fogEnd;
        float factor = (cullDistance - dist) / denominator;

        if (factor >= 1.0f) {
            return -1;
        }

        int fogR = static_cast<int>(g_supervisor.cam0.fogR);
        int fogG = static_cast<int>(g_supervisor.cam0.fogG);
        int fogB = static_cast<int>(g_supervisor.cam0.fogB);

        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8)  & 0xFF;
        uint8_t b = color & 0xFF;


        int subR = static_cast<int>(static_cast<float>(r - fogR) * factor);
        int subG = static_cast<int>(static_cast<float>(g - fogG) * factor);
        int subB = static_cast<int>(static_cast<float>(b - fogB) * factor);

        r -= subR;
        g -= subG;
        b -= subB;

        a = static_cast<uint8_t>(static_cast<float>(a) * (1.0f - factor));
        color = (a << 24) | (r << 16) | (g << 8) | b;
        g_renderQuad144[0].diffuseColor = color;
    }

    g_renderQuad144[1].diffuseColor = g_renderQuad144[0].diffuseColor;
    g_renderQuad144[2].diffuseColor = g_renderQuad144[0].diffuseColor;
    g_renderQuad144[3].diffuseColor = g_renderQuad144[0].diffuseColor;

    return drawVmSprite2D(2, vm);
}

/* Global Functions */

// 0x445460
void blitTextures()
{
    for (int i = 0; i < 4; ++i)
    {
        BlitParams* blitParams = &g_anmManager->blitParamsArray[i];
        if (blitParams->anmLoadedIndex >= 0)
        {
            g_anmManager->blitTextureToSurface(blitParams);
            blitParams->anmLoadedIndex = -1;
        }
    }
}
