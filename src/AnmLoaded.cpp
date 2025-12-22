#include "AnmLoaded.h"
#include "Globals.h"

// 0x4540f0
int AnmLoadedD3D::createTextureFromAtR(AnmLoadedD3D* This)
{
    This->m_flags |= 1;
    g_supervisor.d3dDevice->CreateTexture(
        g_supervisor.m_d3dPresetParameters.BackBufferWidth,
        g_supervisor.m_d3dPresetParameters.BackBufferHeight,
        1,
        1,
        g_supervisor.m_d3dPresetParameters.BackBufferFormat,
        D3DPOOL_DEFAULT,
        &This->m_texture,
        NULL
    );
    This->m_bytesPerPixel = g_supervisor.m_d3dPresetParameters.BackBufferFormat == D3DFMT_X8R8G8B8 * 2 + 2;
    return 0;
}

// 0x4540a0
int AnmLoadedD3D::createTextureFromAt(AnmLoadedD3D* This, uint32_t width, uint32_t height, int i)
{
    This->m_flags = -1;
    D3DXCreateTexture(
        g_supervisor.d3dDevice,
        width,
        height,
        1,
        0,
        g_d3dFormats[i],
        D3DPOOL_MANAGED,
        &This->m_texture
    );
    This->m_bytesPerPixel = g_bytesPerPixelLookupTable[i];
    return width * height * g_bytesPerPixelLookupTable[i];
}

// 0x4545a0
int AnmLoaded::createTextureForEntry(AnmLoaded* This, int i, int globalSpriteOffset, int globalScriptOffset, AnmHeader* anmHeader)
{
    if (anmHeader == nullptr)
    {
        printf("Anm header is null!\n");
        return -1;
    }

    if (anmHeader->version != 7)
    {
        printf("Anm version is incorrect!\n");
        return -1;
    }

    char* name = (char*)anmHeader + anmHeader->nameOffset;
    int result;

    if (anmHeader->hasData == 0)
    {
        if (name[0] == '@')
        {
            if (name[1] == 'R')
                This->m_anmLoadedD3D[i].createTextureFromAtR(&This->m_anmLoadedD3D[i]);
            else
            {
                result = This->m_anmLoadedD3D[i].createTextureFromAt(
                    &This->m_anmLoadedD3D[i],
                    anmHeader->w,
                    anmHeader->h,
                    anmHeader->formatIndex
                );
                This->m_texturesCreated += result;
            }
        }
        else
        {
            result = This->m_anmLoadedD3D[i].createTextureFromImage(
                &This->m_anmLoadedD3D[i],
                anmHeader->x,
                anmHeader->w,
                anmHeader->h,
                anmHeader->formatIndex
            );

            if (result < 0)
            {
                printf(" %s \n", name);
                return -1;
            }
            This->m_texturesCreated += result;
        }
    }
    else
    {
        result = This->m_anmLoadedD3D[i].createTextureFromThtx(
            &This->m_anmLoadedD3D[i],
            anmHeader->w,
            anmHeader->h,
            anmHeader->formatIndex,
            (char*)anmHeader + anmHeader->thtxOffset
        );

        if (result < 0)
        {
            printf("\n");
            return -1;
        }
        This->m_texturesCreated += result;
    }

    D3DSURFACE_DESC desc;
    This->m_anmLoadedD3D[i].m_texture->GetLevelDesc(0, &desc);

    float xScale = static_cast<float>(desc.Width) / static_cast<float>(anmHeader->w);
    float yScale = static_cast<float>(desc.Height) / static_cast<float>(anmHeader->h);

    // The AnmHeader is only the first part of the ANM file loaded
    // After the header (first 64 bytes), we have the rest of the ANM data
    uint32_t* spriteOffsets = (uint32_t*)((char*)anmHeader + sizeof(AnmHeader));

    for (int s = 0; s < anmHeader->numSprites; ++s)
    {
        uint32_t txIdx = spriteOffsets[s];
        SpriteData* spriteData = (SpriteData*)((char*)anmHeader + txIdx);

        float rawStartX = spriteData->x;
        float rawStartY = spriteData->y;
        float rawWidth = spriteData->w;
        float rawHeight = spriteData->h;

        float uvStartX = rawStartX * xScale;
        float uvStartY = rawStartY * yScale;
        float uvEndX = (rawStartX + rawWidth) * xScale;
        float uvEndY = (rawStartY + rawHeight) * yScale;

        AnmLoadedSprite* sprite = &This->m_keyframeData[globalSpriteOffset + s];
        sprite->anmSlot = This->m_anmSlotIndex;
        sprite->spriteNumber = globalSpriteOffset + s;
        sprite->anmLoadedD3D = &This->m_anmLoadedD3D[i];
        sprite->startPixelInclusive.x = rawStartX;
        sprite->startPixelInclusive.y = rawStartY;
        sprite->endPixelExclusive.x = rawStartX + rawWidth;
        sprite->endPixelExclusive.y = rawStartY + rawHeight;
        sprite->bitmapWidth = static_cast<float>(desc.Width);
        sprite->bitmapHeight = static_cast<float>(desc.Height);
        sprite->uvStart.x = uvStartX;
        sprite->uvStart.y = uvStartY;
        sprite->uvEnd.x = uvEndX;
        sprite->uvEnd.y = uvEndY;
        sprite->spriteWidth = rawWidth;
        sprite->spriteHeight = rawHeight;
        sprite->bitmapScale.x = 1.0f;
        sprite->bitmapScale.y = 1.0f;
        sprite->m_idk = 0;
    }

    uint32_t* scriptOffsets = (uint32_t*)((char*)anmHeader + sizeof(AnmHeader) + anmHeader->numSprites * 4);

    for (int s = 0; s < anmHeader->numScripts; ++s)
    {
        uint32_t txIdx = scriptOffsets[s];
        char* scriptData = (char*)anmHeader + txIdx;
        ((char**)This->m_spriteData)[globalScriptOffset + s] = scriptData;
    }

    return 1;
}

// 0x4544c0
void AnmLoaded::setupTextures(AnmLoaded* This)
{
    int status;
    int spritesLoaded;
    AnmHeader* anmHeader;
    int scriptsLoaded;
    int i;
    int anm;
    bool success;

    printf("::postloadAnim : %d, %d\n", This->m_anmSlotIndex, This->m_anmsLoading);
    anmHeader = This->m_header;
    spritesLoaded = 0;
    anm = 0;
    scriptsLoaded = 0;
    i = 0;
    success = false;
    while (true)
    {
        if (anm == This->m_anmsLoading - 1)
        {
            status = createTextureForEntry(This, i, spritesLoaded, scriptsLoaded, anmHeader);
            if (status < 0)
            {
                This->m_anmsLoading = 0;
                return;
            }
            success = true;
        }
        scriptsLoaded = scriptsLoaded + anmHeader->numScripts;
        spritesLoaded = spritesLoaded + anmHeader->numSprites;
        ++i;
        if (anmHeader->nextOffset == 0)
            break;
        anm = anm + 1;
        anmHeader = (AnmHeader*)((int)&anmHeader + anmHeader->nextOffset);
        if ((anm == This->m_anmsLoading) || (success)) {
            ++This->m_anmsLoading;
            return;
        }
    }
    This->m_anmsLoading = 0;
    return;
}

// 0x453f80
int AnmLoadedD3D::createTextureFromThtx(AnmLoadedD3D* This, uint32_t texWidth, uint32_t texHeight, int formatIndex, void* thtxData)
{
    HRESULT result;
    IDirect3DSurface9* d3dSurface = nullptr;
    int formatIndexAdjusted = formatIndex;

    if (g_supervisor.m_gameConfig.flags & 1) // Use 16-bit textures
    {
        D3DFORMAT d3dFormat = g_d3dFormats[formatIndex];
        if (d3dFormat == D3DFMT_A8R8G8B8 || d3dFormat == D3DFMT_UNKNOWN)
            formatIndexAdjusted = 5;

        else if (d3dFormat == D3DFMT_R8G8B8)
            formatIndexAdjusted = 3;
    }

    // Clear flag bit 0
    This->m_flags &= 0xfffffffe;

    // Create the texture
    result = D3DXCreateTexture(
        g_supervisor.d3dDevice,
        texWidth,
        texHeight,
        1,
        0,
        g_d3dFormats[formatIndexAdjusted],
        D3DPOOL_MANAGED,
        &This->m_texture
    );

    if (result != S_OK)
        return -1;

    // Get the surface from the texture
    result = This->m_texture->GetSurfaceLevel(0, &d3dSurface);
    if (result != S_OK)
    {
        This->m_texture->Release();
        This->m_texture = nullptr;
        return -1;
    }

    // Load THTX pixel data into the surface
    uint16_t thtxFormat = *(uint16_t*)((char*)thtxData + 6);
    uint16_t thtxWidth = *(uint16_t*)((char*)thtxData + 8);
    uint16_t thtxHeight = *(uint16_t*)((char*)thtxData + 10);
    uint32_t pitch = thtxWidth * g_bytesPerPixelLookupTable[thtxFormat];

    result = D3DXLoadSurfaceFromMemory(
        d3dSurface,
        nullptr,
        nullptr, // Full destination surface
        (char*)thtxData + 16, // Pixel data offset
        g_d3dFormats[thtxFormat],
        pitch,
        nullptr,
        nullptr, // Full source surface
        D3DX_FILTER_NONE,
        0
    );

    // Release the surface
    if (d3dSurface)
        d3dSurface->Release();

    // Set bytes per pixel and return texture size
    This->m_bytesPerPixel = g_bytesPerPixelLookupTable[formatIndexAdjusted];
    return texWidth * texHeight * This->m_bytesPerPixel;
}

// 0x453d90
int AnmLoadedD3D::createTextureFromImage(AnmLoadedD3D* This, int textureWidthOffset, uint32_t width, uint32_t height, int formatIndex)
{
    int adjustedFormatIndex = formatIndex;
    if (g_supervisor.m_gameConfig.flags & 1) // Use 16-bit textures
    {
        D3DFORMAT format = g_d3dFormats[formatIndex];
        if (format == D3DFMT_A8R8G8B8 || format == D3DFMT_UNKNOWN)
            adjustedFormatIndex = 5;
        else if (format == D3DFMT_R8G8B8)
            adjustedFormatIndex = 3;
    }

    This->m_flags &= 0xfffffffe; // Clear LSB in m_flags

    // Create texture from memory
    IDirect3DTexture9* loadedTexture = nullptr;
    HRESULT result = D3DXCreateTextureFromFileInMemoryEx(
        g_supervisor.d3dDevice,
        This->m_srcData,
        This->m_srcDataSize,
        0,
        0,
        0,
        0,
        g_d3dFormats[adjustedFormatIndex],
        D3DPOOL_MANAGED,
        1,
        -1,
        0,
        nullptr,
        nullptr,
        &loadedTexture
    );
    if (FAILED(result))
        return -1;

    // Get the surface level 0
    IDirect3DSurface9* sourceSurface = nullptr;
    result = loadedTexture->GetSurfaceLevel(0, &sourceSurface);
    if (FAILED(result)) {
        loadedTexture->Release();
        return -1;
    }

    // Get surface description to check dimensions
    D3DSURFACE_DESC desc;
    result = sourceSurface->GetDesc(&desc);
    if (FAILED(result))
    {
        sourceSurface->Release();
        loadedTexture->Release();
        return -1;
    }

    if (desc.Width == width && desc.Height == height)
    {
        // Dimensions match, use the loaded texture directly
        This->m_texture = loadedTexture;
        sourceSurface->Release();
    }
    else
    {
        // Create a new texture with specified dimensions
        IDirect3DTexture9* newTexture = nullptr;
        result = g_supervisor.d3dDevice->CreateTexture(
            width,
            height,
            1,
            0,
            g_d3dFormats[adjustedFormatIndex],
            D3DPOOL_MANAGED,
            &newTexture,
            nullptr
        );

        if (FAILED(result)) {
            sourceSurface->Release();
            loadedTexture->Release();
            return -1;
        }

        // Get the destination surface
        IDirect3DSurface9* destSurface = nullptr;
        result = newTexture->GetSurfaceLevel(0, &destSurface);
        if (FAILED(result))
        {
            newTexture->Release();
            sourceSurface->Release();
            loadedTexture->Release();
            return -1;
        }

        // Set up source rectangle based on assembly calculations
        RECT srcRect;
        srcRect.left = textureWidthOffset;
        srcRect.top = 0; // Assembly sets top later, but logically should start at 0
        srcRect.right = textureWidthOffset + (width + textureWidthOffset <= desc.Width ? width : desc.Width - textureWidthOffset);
        srcRect.bottom = height <= desc.Height ? height : desc.Height;

        // Copy pixel data from source to destination surface
        result = D3DXLoadSurfaceFromSurface(
            destSurface,
            nullptr,
            nullptr,
            sourceSurface,
            nullptr,
            &srcRect,
            D3DX_FILTER_NONE,
            0
        );

        if (FAILED(result))
        {
            destSurface->Release();
            newTexture->Release();
            sourceSurface->Release();
            loadedTexture->Release();
            return -1;
        }

        // Assign the new texture and clean up
        This->m_texture = newTexture;
        destSurface->Release();
        sourceSurface->Release();
        loadedTexture->Release();
    }

    // Set bytes per pixel and return texture size
    This->m_bytesPerPixel = g_bytesPerPixelLookupTable[adjustedFormatIndex];
    return width * height * This->m_bytesPerPixel;
}