#include "Chireiden.h"
#include "AnmManager.h"
#include "AnmVm.h"
#include "Chain.h"
#include "FileAbstrction.h"
#include "Globals.h"

#define MAX_PATH_LEN 260

void resolveLnkShortcut(LPCSTR shortcutPath, LPSTR targetPath)
{
    if (targetPath == nullptr)
        return;

    // Initialize COM
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return;

    IShellLinkA *shellLink = nullptr;
    IPersistFile *persistFile = nullptr;
    WCHAR *wideShortcutPath = nullptr;

    // Set targetPath to empty string initially to indicate failure if not filled
    targetPath[0] = '\0';

    // Create IShellLinkA instance
    hr = CoCreateInstance(
        CLSID_ShellLink,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IShellLinkA,
        (void**)&shellLink
    );
    if (FAILED(hr))
        goto Cleanup;

    // Query for IPersistFile interface
    hr = shellLink->QueryInterface(IID_IPersistFile, (void**)&persistFile);
    if (FAILED(hr))
        goto Cleanup;

    // Allocate memory for wide-character shortcut path
    wideShortcutPath = new WCHAR[MAX_PATH_LEN];
    if (wideShortcutPath == nullptr)
        goto Cleanup;

    // Convert shortcutPath to wide-character string
    MultiByteToWideChar(
        CP_ACP,
        0,
        shortcutPath,
        -1,
        wideShortcutPath, MAX_PATH_LEN);

    // Load the shortcut file
    hr = persistFile->Load(wideShortcutPath, 0);
    if (SUCCEEDED(hr)) {
        // Get the target path
        shellLink->GetPath(targetPath, MAX_PATH_LEN, nullptr, 0);
    }

Cleanup:
    if (wideShortcutPath) free(wideShortcutPath);
    if (persistFile) persistFile->Release();
    if (shellLink) shellLink->Release();
    CoUninitialize();
}

int getLaunchInfo()
{
    STARTUPINFOA startupInfo;
    char resolvedPath[264];
    char moduleFilename[268];
    char* fileExtensionString;

    startupInfo.cb = 0x44;
    memset(&startupInfo.lpReserved, 0, 0x40);
    GetModuleFileNameA(nullptr, moduleFilename, 0x105);
    GetConsoleTitleA(resolvedPath, 0x105);
    GetStartupInfoA(&startupInfo);

    if (startupInfo.lpTitle == nullptr)
        g_supervisor.criticalSectionFlag |= 0x40;
    else
    {
        fileExtensionString = strrchr(startupInfo.lpTitle, '.');
        if (fileExists(startupInfo.lpTitle) && fileExtensionString != nullptr)
        {
            if (_stricmp(fileExtensionString, ".lnk") == 0)
            {
                do {
                    resolveLnkShortcut(startupInfo.lpTitle, resolvedPath);
                    fileExtensionString = strrchr(resolvedPath, '.');
                } while (_stricmp(fileExtensionString, ".lnk") == 0);
            } 
            else
                strcpy_s(resolvedPath, startupInfo.lpTitle);

            if (strcmp(resolvedPath, moduleFilename) != 0)
                g_unusualLaunchFlag = 1;
        }
        g_supervisor.criticalSectionFlag &= ~0x40;
    }
    return (g_app != nullptr) ? 0 : -1;
}

void retrieveSystemStats()
{
  SystemParametersInfoA(0x10, 0, &g_primaryScreenWorkingArea, 0);
  SystemParametersInfoA(0x53, 0, &g_mouseSpeed, 0);
  SystemParametersInfoA(0x54, 0, &g_screenWorkingArea, 0);
  SystemParametersInfoA(0x11, 0, nullptr, 2);
  SystemParametersInfoA(0x55, 0, nullptr, 2);
  SystemParametersInfoA(0x56, 0, nullptr, 2);
  QueryPerformanceFrequency(&g_performanceFrequency);
  QueryPerformanceCounter(&g_performanceCount);
}

// 0x445510
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    DWORD error;
    int launchInfo;
    BYTE keyboardState[256];
    MSG msg;
    HRESULT hr;
    int i;
    BOOL hasMessage;
    float deltaTime;

    g_hInstance = hInstance;
    timeBeginPeriod(1);

    // g_memoryTest = (DWORD*)malloc(4);
    // if (g_memoryTest) {
    //     *g_memoryTest = 0;
    // } else {
    //     g_memoryTest = nullptr;
    // }

    g_supervisor.criticalSectionFlag |= 0x8000;

    // Initialize critical sections
    for (i = 0; i < 12; ++i) {
        InitializeCriticalSection(&g_supervisor.criticalSections[i]);
    }

    printf("東方動作記録 ---------------------------------------------\n");

    // Create app mutex to prevent multiple instances
    g_app = CreateMutexA(nullptr, TRUE, "Touhou 11 App");
    error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) {
        printf("二つは起動できません\n");
        return 0;  // Exit early
    }

    launchInfo = getLaunchInfo();
    if (launchInfo == -1) {
        return 0;  // Exit early on failure
    }

    g_supervisor.hInst = hInstance;
    retrieveSystemStats();

    if (g_supervisor.processGameConfig() != 0)
    {
        printf("Game config error!");
        return 0;  // Exit on config failure
    }

    GetKeyboardState(keyboardState);
    // if ((g_supervisor.gameCfg._56_4_ & 0x100) || (keyboardState[VK_MENU] & 0x80)) {  // Assuming VK_MENU for Alt key
    //     DialogBoxParamA(hInstance, MAKEINTRESOURCE(203), nullptr, dialogBoxProcedure, 0);
    // }

    g_windowConfigFlags = (g_windowConfigFlags & ~0xC) | (g_supervisor.gameCfg.windowSettings * 4);

    calculateSelfChecksum();

    // Main restart loop (for config changes requiring restart)
    while (1)
    {
        // Cleanup phase
        // g_soundManager.isSleeping = 2;
        // waitAndCloseGlobalHandles();
        // releaseSounds();

        if (g_anmManager)
        {
            g_anmManager->~AnmManager();
            delete g_anmManager;
            g_anmManager = nullptr;
        }

        if (g_supervisor.surfaceR0)
        {
            g_supervisor.surfaceR0->Release();
            g_supervisor.surfaceR0 = nullptr;
        }

        if (g_supervisor.surfaceR1)
        {
            g_supervisor.surfaceR1->Release();
            g_supervisor.surfaceR1 = nullptr;
        }

        if (g_supervisor.backBuffer) {
            g_supervisor.backBuffer->Release();
            g_supervisor.backBuffer = nullptr;
        }

        if (g_supervisor.d3dDevice) {
            g_supervisor.d3dDevice->Release();
            g_supervisor.d3dDevice = nullptr;
        }

        if (g_supervisor.d3dInterface0) {
            g_supervisor.d3dInterface0->Release();
            g_supervisor.d3dInterface0 = nullptr;
        }

        if (g_window)
        {
            ShowWindow(g_window, SW_HIDE);
            MoveWindow(g_window, 0, 0, 0, 0, FALSE);
            DestroyWindow(g_window);
            g_window = nullptr;
        }

        // Show cursor
        while (ShowCursor(TRUE) < 0);

        if ((g_windowConfigFlags & 0x60) != 0)
        {
            // Final cleanup on no-restart
            writeToFile("th11.cfg", 0x3C, &g_supervisor.gameCfg);
            timeEndPeriod(1);
            g_supervisor.criticalSectionFlag &= ~0x8000;

            // Delete critical sections
            for (i = 0; i < 12; ++i)
                DeleteCriticalSection(&g_supervisor.criticalSections[i]);

            SystemParametersInfoA(SPI_SETMOUSESPEED, g_primaryScreenWorkingArea, nullptr, SPIF_SENDCHANGE);
            SystemParametersInfoA(SPI_SETMOUSE, g_mouseSpeed, nullptr, SPIF_SENDCHANGE);
            SystemParametersInfoA(0x56, g_screenWorkingArea, nullptr, SPIF_SENDCHANGE);
            WINNLSEnableIME(nullptr, TRUE);

            // if (g_memoryTest) {
            //     free(g_memoryTest);
            // }
            return 0;
        }

        // Restart preparation
        printf("再起動を要するオプションが変更されたので再起動します\n");

        if (!g_supervisor.d3dPresetParameters.Windowed)
            WINNLSEnableIME(nullptr, TRUE);

        // Process pending messages
        for (i = 0; i < 60; ++i) {
            if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        g_supervisor.criticalSectionFlag &= ~0x180;

        // Initialization phase
        g_chain = new Chain;
        if (!g_chain)
        {
            printf("Could not allocate g_chain!\n");
            g_chain = nullptr;
        }

        checkJoystickAvailability();
        normalizeKeyboardState();
        g_supervisor.criticalSectionFlag &= ~0xC00;

        Supervisor::initializeDevices(&g_supervisor);

        g_supervisor.criticalSectionFlag = (g_supervisor.criticalSectionFlag & ~0x400) | ((g_supervisor.keyboard != nullptr) << 10);
        g_supervisor.criticalSectionFlag = (g_supervisor.criticalSectionFlag & ~0x800) | ((g_supervisor.joystick != nullptr) << 11);

        Supervisor::closeThread((LoadingThread*)&g_loadingThread);

        // Create D3D interface
        g_supervisor.d3dInterface0 = Direct3DCreate9(D3D_SDK_VERSION);
        if (!g_supervisor.d3dInterface0) {
            printf("Direct3D オブジェクトは何故か作成出来なかった\n");
            continue;  // Retry init
        }

        // Init main window
        if (FN_initMainWindow(hInstance) != 0) {
            continue;  // Retry on failure
        }

        FN_createloadSoundsThread(g_window);

        // Init supervisor and D3D
        if (NC_supervisorAndD3D(D3DFMT_UNKNOWN) != 0) {
            continue;  // Retry on failure
        }

        g_anmManager = new AnmManager;
        if (g_anmManager)
            g_anmManager->initialize();

        if (!g_supervisor.d3dPresetParameters.Windowed) {
            WINNLSEnableIME(nullptr, FALSE);
            while (ShowCursor(FALSE) >= 0);
            SetCursor(nullptr);
        }

        g_time = 0.0f;
        deltaTime = getDeltaTime();
        g_time1 = deltaTime;
        g_time2 = deltaTime;
        g_time0 = deltaTime;
        deltaTime = getDeltaTime();
        g_time4 = deltaTime;
        g_time3 = deltaTime;

        if (!SetForegroundWindow(g_window)) {
            g_windowConfigFlags |= 1;
            g_windowRelatedFlag = 0xFC;
        }

        Supervisor::initialize();

        // Game loop
        while (!g_timeForCleanup) {
            // Process messages
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }

            hr = g_supervisor.d3dDevice->TestCooperativeLevel();
            if (hr == D3D_OK)
            {
                if ((g_windowConfigFlags & 2) == 0)
                {
                    if ((g_windowConfigFlags & 0x10) == 0)
                    {
                        if (g_supervisor.d3dPresetParameters.PresentationInterval == D3DPRESENT_INTERVAL_ONE && g_supervisor.gameCfg.frameSkip == 0)
                            frameIdkWhatVariationThisIs(&g_window, 0, nullptr, 1, D3DFMT_UNKNOWN);
    
                        else
                            Window::frameFrameskip(&g_window);
                    } 
                    else
                        Window::frame(&g_window);
                
                    g_supervisor.criticalSectionFlag &= ~0x10;
                }
            } 
            else if (hr != D3DERR_INVALIDCALL)
            {
                continue;
            } 
            else
            {
                INT_004c3dc4 = 10;
                if ((g_windowConfigFlags & 2) != 0)
                {
                    // Update presentation parameters for reset
                    if ((g_windowConfigFlags & 0xC) == 0) {
                        GetWindowRect(g_window, &g_supervisor.windowDimensions);
                        memcpy(&g_supervisor.d3dPresetParameters, &g_supervisor.unknown1, sizeof(D3DPRESENT_PARAMETERS));
                        g_supervisor.d3dPresetParameters.PresentationInterval = ((g_windowConfigFlags & 0x10) == 0) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
                        g_supervisor.d3dPresetParameters.FullScreen_RefreshRateInHz = 60;
                        g_supervisor.d3dPresetParameters.Windowed = FALSE;
                        g_supervisor.d3dPresetParameters.BackBufferFormat = (g_supervisor.gameCfg.idk6 != 0) ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;  // Assuming based on context
                    } else {
                        g_supervisor.d3dPresetParameters.BackBufferFormat = g_supervisor.d3dPresentBackBuferFormat;
                        g_supervisor.d3dPresetParameters.FullScreen_RefreshRateInHz = 0;
                        g_supervisor.d3dPresetParameters.PresentationInterval = ((g_windowConfigFlags & 0x10) == 0 && g_supervisor.d3dPresentationIntervalRelatedFlag == 60) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
                        g_supervisor.d3dPresetParameters.Windowed = TRUE;
                    }
                }

                // Attempt device reset
                Supervisor::releaseSurfaces();
                AnmManager::releaseTextures();

                do {
                    hr = g_supervisor.d3dDevice->Reset(&g_supervisor.d3dPresetParameters);
                    if (hr == D3D_OK) {
                        Supervisor::resetRenderState();
                        AnmManager::createD3DTextures();
                        g_supervisor.criticalSectionFlag |= 0x10;
                        g_supervisor.q = 3;

                        if ((g_windowConfigFlags & 2) != 0) {
                            int mode = (g_windowConfigFlags >> 2) & 3;
                            if (mode == 0)
                            {
                                SetWindowLongA(g_window, GWL_STYLE, WS_VISIBLE | WS_POPUP);
                                SetWindowPos(g_window, nullptr, 0, 0, 640, 480, SWP_NOZORDER);
                                WINNLSEnableIME(nullptr, FALSE);
                                while (ShowCursor(FALSE) >= 0);
                                SetCursor(nullptr);
                                g_isAppUnfocused = 0;
                            }
                            else
                            {
                                RECT rect;
                                int width, height;
                                if (mode == 3)
                                {
                                    width = GetSystemMetrics(SM_CXSCREEN) * 2 + 1280;  // Example, adjust based on decomp
                                    height = GetSystemMetrics(SM_CYSCREEN) * 2 + 960;
                                }
                                else if (mode == 2)
                                {
                                    width = GetSystemMetrics(SM_CXSCREEN) * 2 + 960;
                                    height = GetSystemMetrics(SM_CYSCREEN) * 2 + 720;
                                } else
                                {
                                    width = GetSystemMetrics(SM_CXSCREEN) * 2 + 640;
                                    height = GetSystemMetrics(SM_CYSCREEN) * 2 + 480;
                                }
                                int captionHeight = GetSystemMetrics(SM_CYCAPTION);

                                SetWindowLongA(
                                    g_window,
                                    GWL_STYLE,
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE
                                );

                                SetWindowPos(
                                    g_window,
                                    nullptr,
                                    g_supervisor.windowDimensions.left,
                                    g_supervisor.windowDimensions.top,
                                    width,
                                    captionHeight + height,
                                    SWP_SHOWWINDOW | SWP_NOZORDER
                                );

                                ShowWindow(g_window, SW_SHOW);
                                WINNLSEnableIME(nullptr, TRUE);
                                while (ShowCursor(TRUE) < 0);
                            }
                        }

                        g_supervisor->setupCameras();
                        g_windowConfigFlags &= ~2;
                        break;
                    }
                } while (hr == D3DERR_DEVICELOST);
            }
        }

        // Post-game loop cleanup
        g_supervisor.gameCfg.windowSettings = (BYTE)((g_windowConfigFlags >> 2) & 3);
        if (g_supervisor.gameCfg.windowSettings != 0) {
            GetWindowRect(g_window, &g_supervisor.windowDimensions);
            g_supervisor.gameCfg.screenPosX = g_supervisor.windowDimensions.left;
            g_supervisor.gameCfg.screenPosY = g_supervisor.windowDimensions.top;
        }

        Supervisor::cleanup(&g_supervisor);

        if (g_chain)
        {
            Chain::release(g_chain);
            delete g_chain;
            g_chain = nullptr;
        }
    }

    return 0;
}