#include "AnmManager.h"
#include "AnmVm.h"
#include <Chireiden.h>
#include <cstring>

AnmManager* g_anmManager;

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
    if ((this->vertexBuffers).leftoverSpriteCount == 0)
        return;

    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0);
    g_supervisor.d3dDevice->SetFVF(0x144);

    g_supervisor.d3dDevice->DrawPrimitiveUP(
        D3DPT_TRIANGLELIST,
        this->vertexBuffers.leftoverSpriteCount * 2,
        this->vertexBuffers.spriteRenderCursor,
        0x1c
    );

    ++this->refreshCounter;
    this->vertexBuffers.spriteRenderCursor = this->vertexBuffers.spriteWriteCursor;
    this->vertexBuffers.leftoverSpriteCount = 0;
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
    RECT srcRect;
    srcRect.left = blitParams->srcRect.left;
    srcRect.top = blitParams->srcRect.top;
    srcRect.right = blitParams->srcRect.left + blitParams->srcRect.right;
    srcRect.bottom = blitParams->srcRect.top + blitParams->srcRect.bottom;

    // Set up destination rectangle (assuming r2.right and r2.bottom are width and height)
    RECT dstRect;
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

int AnmManager::updateWorldMatrixAndProjectQuadCorners(AnmVm* vm)
{
    int loopCounter;
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

    worldMatrix.m[3][0] = entityPosX + posX + vm->m_pos2.x + worldMatrix.m[3][0];
    worldMatrix.m[3][1] = vm->m_entityPos.y + vm->m_pos.y + vm->m_pos2.y + worldMatrix.m[3][1];
    worldMatrix.m[3][2] = vm->m_entityPos.z + vm->m_pos.z + vm->m_pos2.z;

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

/* Global Functions */

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