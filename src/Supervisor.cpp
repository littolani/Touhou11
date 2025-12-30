#include "Supervisor.h"
#include "AnmManager.h"
#include "Globals.h"
#include "Window.h"
#include "GameConfig.h"

int Supervisor::initializeDevices()
{
    return 0;
}

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
        delete[] configFile;

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
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGDENSITY, bit_cast<DWORD>(1.0f));
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, bit_cast<DWORD>(1000.0f));
    g_supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, bit_cast<DWORD>(5000.0f));

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
    if (g_supervisor.joystick == NULL)
    {
        HRESULT hr = g_supervisor.dInputInterface->CreateDevice(
            lpddi->guidInstance,
            &g_supervisor.joystick,
            NULL
        );

        if (FAILED(hr))
            return DIENUM_CONTINUE;
    }

    // If successful (or if we somehow already had a joystick), return 0 (DIENUM_STOP)
    // to stop enumerating. We only support one controller.
    return DIENUM_STOP;
}

int Supervisor::initializeDevices(Supervisor* This)
{
    HRESULT hr;

    // 0x447ab3: Check Config Flags (Bit 3 usually indicates Input Disabled)
    if (This->m_gameConfig.flags & 8) {
        return -1;
    }

    // 0x447ad8: Create DirectInput8 Interface
    // Assembly 447aba loads g_hInstance into the register passed here
    hr = DirectInput8Create(
        g_window.hInstance,
        DIRECTINPUT_VERSION, // 0x0800
        IID_IDirectInput8,
        (void**)&This->dInputInterface,
        NULL
    );

    if (FAILED(hr)) {
        This->dInputInterface = NULL;
        puts("DirectInput cannot be used\n");
        return -1;
    }

    // 0x447b13: Create Keyboard Device
    // 0x48cc5c is likely the address of GUID_SysKeyboard
    hr = This->dInputInterface->CreateDevice(GUID_SysKeyboard, &This->keyboard, NULL);

    if (FAILED(hr)) {
        // Cleanup and Log
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = NULL;
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
            This->keyboard = NULL;
        }
        // Cleanup Interface
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = NULL;
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
            This->keyboard = NULL;
        }
        // Cleanup Interface
        if (This->dInputInterface) {
            This->dInputInterface->Release();
            This->dInputInterface = NULL;
        }
        puts("DirectInput SetCooperativeLevel Failed\n");
        return -1;
    }

    This->keyboard->Acquire();
    puts("DirectInput Initialized\n");

    This->dInputInterface->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        enumJoysticksCallback,
        NULL,
        DIEDFL_ATTACHEDONLY
    );

    if (This->joystick != NULL)
    {
        This->joystick->SetDataFormat(&c_dfDIJoystick);
        This->joystick->SetCooperativeLevel(
            This->appWindow,
            DISCL_BACKGROUND | DISCL_NONEXCLUSIVE
        );

        This->controllerCaps->dwSize = 0x2C;
        This->joystick->GetCapabilities(This->controllerCaps);
        This->joystick->EnumObjects(enumDeviceObjectsCallback, NULL, DIDFT_ALL);
        
        puts("Found a valid gamepad\n");
    }

    return 0;
}