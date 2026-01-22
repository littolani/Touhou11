#include "Supervisor.h"
#include "AnmManager.h"
#include "AnmLoaded.h"
#include "Chain.h"
#include "Globals.h"
#include "Window.h"
#include "GameConfig.h"
#include "DebugGui.h"
#include <bit>

void Supervisor::releaseDinputIface()
{
    IDirectInput8A* iDirectInput8;
    iDirectInput8 = dInputInterface;
    if (iDirectInput8)
    {
        iDirectInput8->Release();
        dInputInterface = nullptr;
    }
}

void Supervisor::enterCriticalSection(size_t criticalSectionNumber)
{
    if (criticalSectionNumber >= 12)
        return;

    if ((criticalSectionFlag & 0x8000) != 0)
    {
        EnterCriticalSection(&criticalSections[criticalSectionNumber]);
        ++criticalSectionCounters[criticalSectionNumber];
    }
}

void Supervisor::leaveCriticalSection(size_t criticalSectionNumber)
{
    if (criticalSectionNumber >= 12)
        return;

    if ((criticalSectionFlag & 0x8000) != 0)
    {
        LeaveCriticalSection(&criticalSections[criticalSectionNumber]);
        --criticalSectionCounters[criticalSectionNumber];
    }
}

// 0x429eb0
int Supervisor::verifyGameConfig()
{
    size_t fileSize;
    byte* configFile = openFile("th11.cfg", &fileSize, 1);
    m_gameConfig = GameConfig();

    bool isValid = true;

    if (configFile == nullptr)
    {
        printf("Config file could not be found\n");
        isValid = false;
    }
    else
    {
        memcpy(&m_gameConfig, configFile, sizeof(GameConfig));
        game_free(configFile);

        if (m_gameConfig.colorDepth >= 2 ||
            m_gameConfig.sfxEnabled >= 3 ||
            m_gameConfig.startingBombs >= 2 ||
            m_gameConfig.displayMode >= 4 ||
            m_gameConfig.frameSkip >= 3 ||
            m_gameConfig.musicMode >= 3 ||
            m_gameConfig.version != 0x110003 ||
            fileSize != sizeof(GameConfig))
        {
            printf("Config file is invalid\n");
            isValid = false;
        }
    }

    if (!isValid)
        m_gameConfig = GameConfig(); // Reinitialize
    else
    {
        g_defaultGameConfig.shootKey = m_gameConfig.shootKey;
        g_defaultGameConfig.bombKey = m_gameConfig.bombKey;
        g_defaultGameConfig.focusKey = m_gameConfig.focusKey;
        g_defaultGameConfig.pauseKey = m_gameConfig.pauseKey;
        g_defaultGameConfig.upKey = m_gameConfig.upKey;
        g_defaultGameConfig.downKey = m_gameConfig.downKey;
        g_defaultGameConfig.leftKey = m_gameConfig.leftKey;
        g_defaultGameConfig.rightKey = m_gameConfig.rightKey;
        g_defaultGameConfig.refreshRate = m_gameConfig.refreshRate;
    }

    uint32_t flags = m_gameConfig.flags;

    if (flags & 0x1)
        printf("Using 16-bit textures.\n");

    if (m_d3dPresetParameters.Windowed != 0)
        printf("Starting in windowed mode.\n");

    if (flags & 0x2)
        printf("Force the reference rasterizer.\n");

    if (flags & 0x4)
        printf("Not using fog.\n");

    if (flags & 0x8)
        printf("DirectInput is not used for gamepad and keyboard input.\n");

    if (flags & 0x10)
        printf("Loading BGM into memory\n");

    m_noVerticalSyncFlag = 0;
    if (flags & 0x20) {
        printf("Vertical synchronization is not enabled.\n");
        m_noVerticalSyncFlag = 1;
    }

    if (flags & 0x40)
        printf("The text rendering environment is not automatically detected.\n");

    int writeStatus = writeToFile("th11.cfg", sizeof(GameConfig), &m_gameConfig);
    if (writeStatus != 0)
    {
        printf("Could not write file %s\n", "th11.cfg");
        printf("Is the disk full or write-protected?\n");
        return -1;
    }
    return 0;
}

// 0x447270
void Supervisor::resetRenderState()
{
    // Depth Buffer (Z-Buffer)
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

    // Lighting & Culling
    g_supervisor.d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); // Render both sides of triangles

    // Alpha Blending (Standard Transparency)
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    // Shading
    g_supervisor.d3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

    // Alpha Testing (Cutout transparency)
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0x01);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

    // Fog Settings
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, 0xFFA0A0A0); // A:255, R:160, G:160, B:160
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGDENSITY, std::bit_cast<DWORD>(1.0f));
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(1000.0f));
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(5000.0f));

    // Antialiasing
    g_supervisor.d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);


    // --- Texture Stage States (Stage 0) ---

    // Color Operations: Modulate (Multiply) Texture * TFACTOR
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    // Magic number 3 is D3DTA_TFACTOR. If you intended to mix with vertex color, use D3DTA_DIFFUSE or D3DTA_CURRENT.
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

    // Alpha Operations: Modulate Texture * TFACTOR
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

    // Coordinates
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    g_supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);


    // --- Sampler States (Stage 0) ---

    // Filtering (Trilinear interpolation logic looks intended here)
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);   // No Mipmap filtering
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR); // Linear Magnification
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR); // Linear Minification

    // Addressing (Wrap UV, Clamp W)
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_supervisor.d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

    if (g_anmManager)
    {
        g_anmManager->m_renderStateMode = 7;
        g_anmManager->m_unk_4355c1 = -1;
        g_anmManager->m_haveFlushedSprites = -1;
        g_anmManager->m_anmLoadedD3D = nullptr;
        g_anmManager->m_unk_4355c4 = -1;
    }
    return;
}

BOOL CALLBACK enumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef)
{
    // Check if the object is an axis (either Absolute or Relative)
    // DIDFT_AXIS = 3. The check (lpddoi->dwType & 3) verifies this.
    if ((lpddoi->dwType & DIDFT_AXIS) != 0) {
        DIPROPRANGE diprg;

        diprg.diph.dwSize = sizeof(DIPROPRANGE); // 0x18
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); // 0x10
        diprg.diph.dwObj = lpddoi->dwType;      // The Object ID (Type)
        diprg.diph.dwHow = DIPH_BYID;           // 2 = By Identifier
        diprg.lMin = -1000;               // 0xFFFFFC18
        diprg.lMax = 1000;                // 0x000003E8

        // Set the range property on the global joystick instance
        // Assuming g_supervisor is the global instance of Supervisor
        if (FAILED(g_supervisor.joystick->SetProperty(DIPROP_RANGE, &diprg.diph))) {
            return DIENUM_STOP; // 0
        }
    }
    return DIENUM_CONTINUE; // 1
}

// 0x00447cb0
BOOL CALLBACK enumJoysticksCallback(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
    // Only create a device if we haven't found one yet
    if (g_supervisor.joystick == nullptr)
    {
        HRESULT hr = g_supervisor.dInputInterface->CreateDevice(
            lpddi->guidInstance,
            &g_supervisor.joystick,
            nullptr
        );

        if (FAILED(hr))
            return DIENUM_CONTINUE;
    }

    // If successful (or if we somehow already had a joystick), return 0 (DIENUM_STOP)
    // to stop enumerating. We only support one controller.
    return DIENUM_STOP;
}

int Supervisor::initializeInputDevices(Supervisor* This)
{
    // 0x447ab3: Check Config Flags (Bit 3 usually indicates Input Disabled)
    if (This->m_gameConfig.flags & 8)
        return -1;

    // 0x447ad8: Create DirectInput8 Interface
    // Assembly 447aba loads g_hInstance into the register passed here
    HRESULT hr = DirectInput8Create(
        g_window.hInstance,
        DIRECTINPUT_VERSION, // 0x0800
        IID_IDirectInput8,
        (void**)&This->dInputInterface,
        nullptr
    );

    if (FAILED(hr)) {
        This->dInputInterface = nullptr;
        puts("DirectInput cannot be used\n");
        return -1;
    }

    // 0x447b13: Create Keyboard Device
    // 0x48cc5c is likely the address of GUID_SysKeyboard
    hr = This->dInputInterface->CreateDevice(GUID_SysKeyboard, &This->keyboard, nullptr);

    if (FAILED(hr)) {
        // Cleanup and Log
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = nullptr;
        }
        puts("Could not initialize DirectInput\n");
        return -1;
    }

    // 0x447b41: Set Keyboard Data Format
    hr = This->keyboard->SetDataFormat(&c_dfDIKeyboard);

    if (FAILED(hr)) {
        // Cleanup Keyboard
        if (This->keyboard) {
            This->keyboard->Release();
            This->keyboard = nullptr;
        }
        // Cleanup Interface
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = nullptr;
        }
        puts("DirectInput SetDataFormat failed\n");
        return -1;
    }

    hr = This->keyboard->SetCooperativeLevel(
        This->appWindow,
        DISCL_NOWINKEY | DISCL_FOREGROUND | DISCL_NONEXCLUSIVE
    );

    if (FAILED(hr)) {
        // Cleanup Keyboard
        if (This->keyboard) {
            This->keyboard->Release();
            This->keyboard = nullptr;
        }
        // Cleanup Interface
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = nullptr;
        }
        puts("DirectInput SetCooperativeLevel Failed\n");
        return -1;
    }

    This->keyboard->Acquire();
    puts("DirectInput Initialized\n");

    This->dInputInterface->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        enumJoysticksCallback,
        nullptr,
        DIEDFL_ATTACHEDONLY
    );

    if (This->joystick != nullptr)
    {
        This->joystick->SetDataFormat(&c_dfDIJoystick);
        This->joystick->SetCooperativeLevel(
            This->appWindow,
            DISCL_BACKGROUND | DISCL_NONEXCLUSIVE
        );

        This->controllerCaps->dwSize = 0x2C;
        This->joystick->GetCapabilities(This->controllerCaps);
        This->joystick->EnumObjects(enumDeviceObjectsCallback, nullptr, DIDFT_ALL);
        
        puts("Found a valid gamepad\n");
    }

    return 0;
}

void Supervisor::renderFrameWithReset()
{
    g_supervisor.d3dDevice->Clear(0, nullptr, 3, 0xff000000, 1.0, 0);
    HRESULT hr = g_supervisor.d3dDevice->Present(
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (FAILED(hr))
        g_supervisor.d3dDevice->Reset(&g_supervisor.m_d3dPresetParameters);

    g_supervisor.d3dDevice->Clear(
        0,
        nullptr,
        3,
        0xff000000,
        1.0,
        0
    );
    hr = g_supervisor.d3dDevice->Present(
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    if (FAILED(hr))
        g_supervisor.d3dDevice->Reset(&g_supervisor.m_d3dPresetParameters);
}

void Supervisor::setupViewport()
{
    if (g_anmManager)
        AnmManager::flushSprites(g_anmManager);
   
    g_supervisor.d3dViewport.MinZ = 0.0;
    g_supervisor.d3dViewport.X = 0;
    g_supervisor.d3dViewport.Y = 0;
    g_supervisor.d3dViewport.MaxZ = 1.0;
    g_supervisor.d3dViewport.Width = 640;
    g_supervisor.d3dViewport.Height = 480;
    g_supervisor.d3dDevice->SetViewport(&g_supervisor.d3dViewport);
    renderFrameWithReset();
}

// 0x446d30
int Supervisor::initD3d9Devices(D3DFORMAT d3dFormat)
{
    printf("initD3d9Devices\n");
    HRESULT hr;
    D3DDISPLAYMODE currentDisplayMode;
    D3DPRESENT_PARAMETERS d3dpp;

    g_supervisor.d3dInterface0->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &currentDisplayMode);
    g_supervisor.currentDisplayModeWidth = currentDisplayMode.Width;
    g_supervisor.currentDisplayModeHeight = currentDisplayMode.Height;
    g_supervisor.d3dPresentationIntervalFlag = currentDisplayMode.RefreshRate;
    g_supervisor.d3dPresentBackBuferFormat = currentDisplayMode.Format;

    int presentationInterval = D3DPRESENT_INTERVAL_DEFAULT; // Default (usually matches RefreshRate or 60)

    // 2. Validate Refresh Rate (0x3c = 60Hz)
    if (g_supervisor.m_gameConfig.displayMode != 0) // Windowed mode check?
    {
        if (currentDisplayMode.RefreshRate != 60)
        {
            printf("Refresh rate is not 60Hz\n");
            g_window.someFlag2 &= ~0x10;
        }

        // Logic for setting presentation interval based on config/flags
        //currentDisplayMode.Format = (D3DFORMAT)currentDisplayMode.Width; // (Ghidra artifact likely, ignored)

        bool lowLatency = (g_window.someFlag2 & 0x10) || (g_supervisor.m_gameConfig.latencyMode == 3);
        if (lowLatency || currentDisplayMode.RefreshRate != 60)
            presentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // No VSync
        else
            presentationInterval = D3DPRESENT_INTERVAL_ONE; // VSync

        goto SETUP_PRESENT_PARAMS;
    }

    // Color Depth Configuration
    if ((g_supervisor.m_gameConfig.flags & 1) == 0)
    {
        if (g_supervisor.m_gameConfig.colorDepth == 0xFF)
        {
            currentDisplayMode.Format = D3DFMT_X8R8G8B8;
            g_supervisor.m_gameConfig.colorDepth = 0;
            printf("First launch, D3D Device at %p initialized screen to 32Bits\n", g_supervisor.d3dDevice);
        }
        else
            currentDisplayMode.Format = (g_supervisor.m_gameConfig.colorDepth != 0) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
    }
    else
    {
        currentDisplayMode.Format = D3DFMT_R5G6B5;
        g_supervisor.m_gameConfig.colorDepth = 1;
    }

    // VSync / Latency Logic
    if (g_window.unusualLaunchFlag == 0)
    {
        if (g_supervisor.m_noVerticalSyncFlag == 0)
        {
            if ((g_window.someFlag2 & 0x10) == 0)
            {
                presentationInterval = (g_supervisor.m_gameConfig.latencyMode != 3) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            }
            else
                presentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

            printf("Attempting to change refresh rate to 60Hz of D3D Device at %p\n", g_supervisor.d3dDevice);
            goto SETUP_PRESENT_PARAMS;
        }
    }
    else
    {
        g_supervisor.m_noVerticalSyncFlag = 1;
    }

    presentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    printf("Attempting VSync async possible check of D3D Device at %p\n", g_supervisor.d3dDevice);

SETUP_PRESENT_PARAMS:
    g_supervisor.criticalSectionFlag |= 2;
    memset(&d3dpp, 0, sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = currentDisplayMode.Format;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = g_window.hwnd;
    d3dpp.Windowed = TRUE;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16; // Standard depth buffer
    d3dpp.Flags = 0;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = presentationInterval;

    g_supervisor.idk8[0] = 1;
    bool isResetAttempted = false;

    while (true)
    {
        // Try Hardware Vertex Processing (T&L HAL)
        if ((g_supervisor.m_gameConfig.flags & 2) == 0)
        {
            hr = g_supervisor.d3dInterface0->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                g_window.hwnd,
                D3DCREATE_HARDWARE_VERTEXPROCESSING,
                &d3dpp,
                &g_supervisor.d3dDevice
            );

            if (SUCCEEDED(hr))
            {
                printf("Running D3D device at %p with T&L HAL\n", g_supervisor.d3dDevice);
#ifdef IMGUI_DEBUG_WINDOW
                InitDebugWindow(g_supervisor.d3dDevice);  //ImGuiHook::getInstance().hook(g_supervisor.d3dDevice);
                printf("ImGui Hook Attached.\n");
#endif
                g_supervisor.criticalSectionFlag |= 1;
                goto DEVICE_CREATED;
            }

            if (isResetAttempted)
                printf("T&L HAL seems unusable for D3D device at %p\n", g_supervisor.d3dDevice);

            // Fallback to Software Vertex Processing (HAL)
            hr = g_supervisor.d3dInterface0->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                g_window.hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &d3dpp,
                &g_supervisor.d3dDevice
            );

            if (SUCCEEDED(hr))
            {
                printf("Running D3D Device at %p with HAL\n", g_supervisor.d3dDevice);
#ifdef IMGUI_DEBUG_WINDOW
                InitDebugWindow(g_supervisor.d3dDevice); //ImGuiHook::getInstance().hook(g_supervisor.d3dDevice);
                printf("ImGui Hook Attached.\n");
#endif
                g_supervisor.criticalSectionFlag &= ~1;
                goto DEVICE_CREATED;
            }

            if (isResetAttempted)
                printf("HAL also seems unusable for D3D Device at %p\n", g_supervisor.d3dDevice);
        }

        // Fallback to Reference Rasterizer (REF) - extremely slow
        hr = g_supervisor.d3dInterface0->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_REF,
            g_window.hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &d3dpp,
            &g_supervisor.d3dDevice
        );

        if (SUCCEEDED(hr))
            break; // Found a device (REF)

        // Error Handling
        if (g_supervisor.m_noVerticalSyncFlag == 0)
        {
            printf("Cannot change refresh rate of D3D Device at %p\n", g_supervisor.d3dDevice);
            g_supervisor.idk8[0] = 0;
            isResetAttempted = true;
        }
        else
        {
            if (d3dpp.PresentationInterval != D3DPRESENT_INTERVAL_ONE)
            {
                printf("Failed to initialize Direct3D, cannot play game\n");
                if (g_supervisor.d3dInterface0)
                {
                    g_supervisor.d3dInterface0->Release();
                    g_supervisor.d3dInterface0 = nullptr;
                }
                return 1;
            }
            d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
    }

    printf("Running D3D Device at %p with REF, probably too slow to play...\n", g_supervisor.d3dDevice);
#ifdef IMGUI_DEBUG_WINDOW
    //ImGuiHook::getInstance().hook(g_supervisor.d3dDevice);
    InitDebugWindow(g_supervisor.d3dDevice);
    printf("ImGui Hook Attached.\n");
#endif
    g_supervisor.criticalSectionFlag &= ~1;

DEVICE_CREATED:
    
    g_supervisor.m_d3dPresetParameters = d3dpp; // Copy successful presentation parameters to Supervisor member

    // Set View Matrix (LookAtLH)
    D3DXVECTOR3 cameraPosition(320.0f, -240.0f, 0.0f);
    D3DXVECTOR3 cameraTarget(320.0f, -240.0f, 0.0f);
    D3DXVECTOR3 cameraUp(0.0f, 1.0f, 0.0f);

    // Calculate Z position for 30 degree FOV to fit 480 height

    float fovY = D3DXToRadian(30.0f); // 0x3e860a92 is approx 15 degrees in radians
    float tanHalfFov = tanf(fovY / 2.0f);
    cameraPosition.z = -(240.0f / tanHalfFov);

    D3DXMatrixLookAtLH(&g_supervisor.d3dMatrix1, &cameraPosition, &cameraTarget, &cameraUp);

    // Set Projection Matrix (PerspectiveFovLH)
    D3DXMatrixPerspectiveFovLH(&g_supervisor.d3dMatrix2, fovY, 4.0f / 3.0f, 100.0f, 10000.0f);

    g_supervisor.d3dDevice->SetTransform(D3DTS_VIEW, &g_supervisor.d3dMatrix1);
    g_supervisor.d3dDevice->SetTransform(D3DTS_PROJECTION, &g_supervisor.d3dMatrix2);
    g_supervisor.d3dDevice->GetViewport(&g_supervisor.d3dViewport);

    // Check Capabilities
    D3DCAPS9 caps;
    g_supervisor.d3dDevice->GetDeviceCaps(&caps);

    if (!(caps.TextureOpCaps & D3DTEXOPCAPS_ADD))
        printf("D3DTEXOPCAPS_ADD not supported for D3D Device at %p, running in color add emulate mode\n", g_supervisor.d3dDevice);

    if (caps.MaxTextureWidth < 256 || caps.MaxTextureHeight < 256) // 0x101 check implies 257? likely 256 check
        printf("D3D Device at %p does not support 512+ textures. Images will be blurry.\n", g_supervisor.d3dDevice);

    // Check Texture Format Support (D3DFMT_A8R8G8B8)
    if ((g_supervisor.m_gameConfig.flags & 1) == 0 && (d3dFormat >> 24) != 0)
    {
        hr = g_supervisor.d3dInterface0->CheckDeviceFormat(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            currentDisplayMode.Format,
            0,
            D3DRTYPE_TEXTURE,
            D3DFMT_A8R8G8B8
        );

        if (hr == D3D_OK)
            g_supervisor.criticalSectionFlag |= 4;
        else
        {
            g_supervisor.criticalSectionFlag &= ~4;
            g_supervisor.m_gameConfig.flags |= 1;
            printf("D3DFMT_A8R8G8B8 not supported on D3D Device at %p, running in reduced color mode\n", g_supervisor.d3dDevice);
        }
    }

    resetRenderState();
    setupViewport();
    g_window.timeForCleanup = 0;
    g_supervisor.idk8[1] = 0;
    return 0;
}

void Supervisor::setupCameras(Supervisor* This)
{
    if (This->surfaceR0 == nullptr)
    {
        IDirect3DTexture9* tex = This->someAnmLoaded->m_anmLoadedD3D[2].m_texture;
        tex->GetSurfaceLevel(0, &This->surfaceR0);
        This->arcadeVm0->loadIntoAnmVm(This->arcadeVm0, This->someAnmLoaded, 0x51);
        if (This->surfaceR0 == nullptr)
            memcpy(&This->stageCam, &This->cam0, sizeof(Camera));
    
        tex = This->someAnmLoaded->m_anmLoadedD3D[3].m_texture;
        tex->GetSurfaceLevel(0, &This->surfaceR1);
        This->arcadeVm1->loadIntoAnmVm(This->arcadeVm1, This->someAnmLoaded, 0x52);
        This->arcadeVm2->loadIntoAnmVm(This->arcadeVm2, This->someAnmLoaded, 0x51);
        This->d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &This->backBuffer);
    }
}

void Supervisor::swapCameraTransformMatrices(Camera* cam)
{
    // Touhou 11 uses a fixed FOV of PI/10 (approx 18 degrees) for this camera mode
    const float FOV_RADIANS = D3DX_PI / 10.0f;
    const float Z_NEAR = 1.0f;
    const float Z_FAR = 10000.0f;

    if (g_anmManager)
        AnmManager::flushSprites(g_anmManager);

    float viewportW = static_cast<float>(static_cast<uint32_t>(cam->viewport.Width));
    float viewportH = static_cast<float>(static_cast<uint32_t>(cam->viewport.Height));

    float halfW = viewportW * 0.5f;
    float halfH = viewportH * 0.5f;

    // Calculate Eye Z position
    // The camera is positioned so that the viewport height aligns exactly with the FOV at z=0.
    // Formula: distance = (height / 2) / tan(fov / 2)
    float tanHalfFov = std::tan(FOV_RADIANS * 0.5f);
    float eyeZ = halfH / tanHalfFov;

    // Setup Camera Vectors
    D3DXVECTOR3 eyeVec(halfW, halfH, eyeZ);
    D3DXVECTOR3 atVec(halfW, halfH, 0.0f);
    D3DXVECTOR3 upVec(0.0f, -1.0f, 0.0f);

    // Build View Matrix
    D3DXMatrixLookAtLH(&cam->viewMatrix, &eyeVec, &atVec, &upVec);

    // Build Projection Matrix
    D3DXMatrixPerspectiveFovLH(
        &cam->projectionMatrix,
        FOV_RADIANS,
        viewportW / viewportH,
        Z_NEAR,
        Z_FAR
    );

    // Apply Transforms
    if (g_supervisor.d3dDevice)
    {
        g_supervisor.d3dDevice->SetTransform(D3DTS_VIEW, &cam->viewMatrix);
        g_supervisor.d3dDevice->SetTransform(D3DTS_PROJECTION, &cam->projectionMatrix);
    }

    // Apply Global Offsets (likely camera shake or scrolling)
    if (g_anmManager)
    {
        g_anmManager->m_globalRenderQuadOffsetX = cam->m_globalRenderQuadOffsetX;
        g_anmManager->m_globalRenderQuadOffsetY = cam->m_globalRenderQuadOffsetY;
    }
}

void Supervisor::releaseSurfaces()
{
    if (g_supervisor.surfaceR0)
    {
        g_supervisor.surfaceR0->Release();
        g_supervisor.surfaceR0 = NULL;
    }
    if (g_supervisor.surfaceR1)
    {
        g_supervisor.surfaceR1->Release();
        g_supervisor.surfaceR1 = NULL;
    }
    if (g_supervisor.backBuffer)
    {
        g_supervisor.backBuffer->Release();
        g_supervisor.backBuffer = NULL;
    }
    g_supervisor.surfaceR0 = NULL;
    return;
}

void Supervisor::releaseChains()
{
    g_supervisor.thread.close(&g_supervisor.thread);
    g_chain->timeToRemove = 1;
    g_chain->runCalcChain(g_chain);
    g_chain->releaseSingleChain(g_chain, &g_chain->calcChain);
    g_chain->releaseSingleChain(g_chain, &g_chain->drawChain);
    g_chain->drawChain.jobRunDrawChainCallback = nullptr;
    g_chain->drawChain.registerChainCallback = nullptr;
    g_chain->drawChain.runCalcChainCallback = nullptr;
    g_chain->calcChain.jobRunDrawChainCallback = nullptr;
    g_chain->calcChain.registerChainCallback = nullptr;
    g_chain->calcChain.runCalcChainCallback = nullptr;
}

HRESULT Supervisor::disableD3dFog(Supervisor* This)
{
    if (This->d3dDisableFogFlag != 0)
    {
        AnmManager::flushSprites(g_anmManager);
        This->d3dDisableFogFlag = 0;
        return This->d3dDevice->SetRenderState(D3DRS_FOGENABLE, 0);
    }
    return 0;
}

