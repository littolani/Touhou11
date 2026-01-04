#include "AnmVm.h"
#include "AnmLoaded.h"
#include "AnmManager.h"
#include "Chireiden.h"
#include "DebugGui.h"
#include "Globals.h"
#include "ThunkGenerator.h"
#include "FileAbstraction.h"
#include "SoundManager.h"
#include <conio.h>

typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT SDKVersion);
Direct3DCreate9_t Real_Direct3DCreate9 = nullptr;

HMODULE g_hOriginalDll = nullptr;

void LoadOriginalDll()
{
    if (g_hOriginalDll)
        return;

    // Get the path to the System32 folder where the real d3d9.dll lives
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\d3d9.dll");

    g_hOriginalDll = LoadLibraryA(path);
    if (!g_hOriginalDll)
    {
        MessageBoxA(NULL, "Could not load system d3d9.dll", "Chireiden Error", MB_ICONERROR);
        ExitProcess(1);
    }

    Real_Direct3DCreate9 = (Direct3DCreate9_t)GetProcAddress(g_hOriginalDll, "Direct3DCreate9");
}

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    LoadOriginalDll();

    // Eventually hook IDirect3D9::CreateDevice to initialize ImGui later.
    
    return Real_Direct3DCreate9(SDKVersion);
}

void logfThunk(const char* format, ...)
{
    // Get current time
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_s(&tm_now, &now);

    // Print timestamp
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "[%Y-%m-%d %H:%M:%S] ", &tm_now);
    fputs(timeBuf, stdout);

    // Print formatted message
    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);
}

void logThunk(const char* str)
{
    logfThunk("%s", str);
}

void setupConsole()
{
    if (!AllocConsole())
        return;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufferSize;
    bufferSize.X = 4096;
    bufferSize.Y = 2000;
    SetConsoleScreenBufferSize(hOut, bufferSize);

    // Redirect stdout, stdin, and stderr
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    // Enable ANSI Colors
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    // Sync C++ streams (std::cout, std::cin, etc.)
    std::ios::sync_with_stdio(true);

    SetConsoleTitleA("Reimu's Debug Console");
}

void waitForDebugger()
{
    if (!IsDebuggerPresent()) {
        printf("Waiting for debugger to attach to th11.exe (or press any key to skip)...\n");

        while (!IsDebuggerPresent())
        {
            // Check if a key has been pressed in the console
            if (_kbhit())
            {
                _getch(); // Consume the key so it doesn't appear in the next input
                printf("Key pressed. Continuing without debugger.\n");
                return;
            }
            Sleep(100);
        }

        // If the loop exited because a debugger attached
        printf("Debugger attached!\n");
        Sleep(500);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        setupConsole();
        //g_console.log("hi");

        installHook(0x459bc0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x441c80, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x44ab00, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x456ad0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x45b5a0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x41f6e0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x458a10, createLtoThunk<Stack<0x4>>(logThunk, 0));
        installHook(0x458af0, createLtoThunk<Stack<0x4>>(logThunk, 0));
        
        installHook(0x449660, createLtoThunk<Returns<RegCode::EAX>, ECX, Stack<0x4>, EAX, EDI>(SoundManager::findRiffChunk, 0x4));
        
        installHook(0x44fd10, createLtoThunk<ESI>(AnmManager::flushSprites, 0));
        installHook(0x44f710, createLtoThunk<EAX, EDI>(AnmManager::setupRenderStateForVm, 0));
        installHook(0x44f4b0, createLtoThunk<EAX, Stack<0x4>>(AnmManager::applyRenderStateForVm, 0x4));
        installHook(0x44fda0, createLtoThunk<EAX, EDX>(AnmManager::writeSprite, 0));
        installHook(0x44f880, createLtoThunk<ECX, Stack<0x4>, EAX>(AnmManager::drawVmSprite2D, 0x4));
        installHook(0x4543e0, createLtoThunk<Returns<RegCode::EAX>, ECX, Stack<0x4>, EBX>(AnmManager::openAnmLoaded, 0x4));
        installHook(0x454190, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, ECX>(AnmManager::preloadAnmFromMemory, 0x8));
        installHook(0x454360, createLtoThunk<Returns<RegCode::EAX>, ECX, EBX>(AnmManager::preloadAnm, 0));
        installHook(0x450e20, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, EAX>(AnmManager::updateWorldMatrixAndProjectQuadCorners, 0x4));
        installHook(0x4513a0, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, EBX>(AnmManager::drawVmWithTextureTransform, 0x4));
        installHook(0x450b00, createLtoThunk<Returns<RegCode::EAX>, EDI, ESI>(AnmManager::drawVmWithFog, 0));
        installHook(0x451ef0, createLtoThunk<ECX, EAX>(AnmManager::drawVm, 0));

        installHook(0x457080, createLtoThunk<EDX, ECX>(Chain::cut, 0));
        installHook(0x456cb0, createLtoThunk<Returns<RegCode::EAX>, EBX>(Chain::runCalcChain, 0));
        installHook(0x456e10, createLtoThunk<Returns<RegCode::EAX>>(Chain::runDrawChain, 0));
        installHook(0x456c10, createLtoThunk<Returns<RegCode::EAX>, ESI, EBX>(Chain::registerDrawChain, 0));

        installHook(0x4503d0, createLtoThunk<EAX, Stack<0x4>, EBX, EDI, ESI>(AnmVm::applyZRotationToQuadCorners, 0x4));
        installHook(0x4500f0, createLtoThunk<Stack<0x4>, EBX, EDI, EDX, ESI>(AnmVm::writeSpriteCharacters, 0x4));
        installHook(0x450700, createLtoThunk<Returns<RegCode::EAX>, EAX>(AnmVm::projectQuadCornersThroughCameraViewport, 0));
        installHook(0x452420, createLtoThunk<Returns<RegCode::EAX>, ECX>(AnmVm::onTick, 0));

        installHook(0x4540f0, createLtoThunk<ESI>(AnmLoadedD3D::createTextureFromAtR, 0));

        installHook(0x458400, createLtoThunk<Returns<RegCode::EAX>, EAX, Stack<0x4>, Stack<0x8>>(openFile, 0x8));
        installHook(0x441760, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, ECX>(PbgArchive::readDecompressEntry, 0x8));
        installHook(0x4418d0, createLtoThunk<Returns<RegCode::EAX>, EAX, EBX>(PbgArchive::findEntry, 0));
        installHook(0x4426c0, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, Stack<0xc>, EAX>(Lzss::decompress, 0xc));
        installHook(0x4423a0, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, Stack<0xc>>(Lzss::compress, 0xc));
        installHook(0x4581c0, createLtoThunk<Stack<0x4>, Stack<0x8>, AL, Stack<0xc>, Stack<0x10>, Stack<0x14>>(FileUtils::decrypt, 0x14));

        installHook(0x446d30, createLtoThunk<Returns<RegCode::EAX>, EDI>(Supervisor::initD3d9Devices, 0));

        //installHook(0x445e00, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, Stack<0xc>, Stack<0x10>>(Window::wndProcCallback, 0x10));
        installHook(0x446ae0, createLtoThunk<Returns<RegCode::EAX>, EBX>(Window::initialize, 0));

        //waitForDebugger();
    }
    return TRUE;
}

#if 0
void resolveLnkShortcut(LPCSTR shortcutPath, LPSTR targetPath)
{
    if (targetPath == nullptr)
        return;

    // Initialize COM
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return;

    IShellLinkA* shellLink = nullptr;
    IPersistFile* persistFile = nullptr;
    WCHAR* wideShortcutPath = nullptr;

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
    for (i = 0; i < 12; ++i)
        InitializeCriticalSection(&g_supervisor.criticalSections[i]);

    // Create app mutex to prevent multiple instances
    g_app = CreateMutexA(nullptr, TRUE, "Touhou 11 App");
    error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        printf("Another instance of the app is already running.\n");
        return 0;
    }

    launchInfo = getLaunchInfo();
    if (launchInfo == -1) {
        return 0;
    }

    g_supervisor.hInst = hInstance;
    retrieveSystemStats();

    if (g_supervisor.processGameConfig() != 0)
    {
        printf("Game config error!");
        return 0;
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

        if (g_supervisor.backBuffer)
        {
            g_supervisor.backBuffer->Release();
            g_supervisor.backBuffer = nullptr;
        }

        if (g_supervisor.d3dDevice)
        {
            g_supervisor.d3dDevice->Release();
            g_supervisor.d3dDevice = nullptr;
        }

        if (g_supervisor.d3dInterface0)
        {
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
        printf("Restart preparation\n");

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

        g_supervisor.initializeDevices();

        g_supervisor.criticalSectionFlag = (g_supervisor.criticalSectionFlag & ~0x400) | ((g_supervisor.keyboard != nullptr) << 10);
        g_supervisor.criticalSectionFlag = (g_supervisor.criticalSectionFlag & ~0x800) | ((g_supervisor.joystick != nullptr) << 11);

        Supervisor::closeThread((LoadingThread*)&g_loadingThread);

        // Create D3D interface
        g_supervisor.d3dInterface0 = Direct3DCreate9(D3D_SDK_VERSION);
        if (!g_supervisor.d3dInterface0) {
            printf("Direct3D reinitializing\n");
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
                    }
                    else {
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
                                }
                                else
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
#endif