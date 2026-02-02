#include "AsciiManager.h"
#include "AnmVm.h"
#include "AnmLoaded.h"
#include "AnmManager.h"
#include "Bullet.h"
#include "Chireiden.h"
#include "DebugGui.h"
#include "Globals.h"
#include "ThunkGenerator.h"
#include "FileAbstraction.h"
#include "ScoreManager.h"
#include "SoundManager.h"

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
    wideShortcutPath = new WCHAR[260];
    if (wideShortcutPath == nullptr)
        goto Cleanup;

    // Convert shortcutPath to wide-character string
    MultiByteToWideChar(
        CP_ACP,
        0,
        shortcutPath,
        -1,
        wideShortcutPath, 260);

    // Load the shortcut file
    hr = persistFile->Load(wideShortcutPath, 0);
    if (SUCCEEDED(hr)) {
        // Get the target path
        shellLink->GetPath(targetPath, 260, nullptr, 0);
    }

Cleanup:
    if (wideShortcutPath)
        delete[] wideShortcutPath;
    if (persistFile)
        persistFile->Release();
    if (shellLink)
        shellLink->Release();
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
                g_window.unusualLaunchFlag = 1;
        }
        g_supervisor.criticalSectionFlag &= ~0x40;
    }
    return (g_app != nullptr) ? 0 : -1;
}

// Define Control IDs for readability
#define IDC_CHK_ALWAYS_SHOW 0xCA
#define IDC_RAD_FS_640      0xCC
#define IDC_RAD_WIN_640     0xCD
#define IDC_RAD_WIN_960     0xCE
#define IDC_RAD_WIN_1280    0xCF
#define IDC_BTN_START       0xD0

// Flag definitions for g_window.someFlag2
#define WND_FLAG_DIALOG_CANCELLED 0x20
#define WND_FLAG_DIALOG_ACTIVE    0x40
#define WND_FLAG_DIALOG_MASK      0x60

BOOL CALLBACK chooseResolutionDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Setup Checkbox state based on config
        // Flag 0x100 determines if this dialog is shown on startup
        UINT checkState = (g_supervisor.m_gameConfig.flags & 0x100) ? BST_CHECKED : BST_UNCHECKED;
        CheckDlgButton(hDlg, IDC_CHK_ALWAYS_SHOW, checkState);

        // Select the correct Radio Button based on current display mode
        int radioIdToSelect = IDC_RAD_FS_640;
        switch (g_supervisor.m_gameConfig.displayMode)
        {
        case 0: radioIdToSelect = IDC_RAD_FS_640; break;
        case 1: radioIdToSelect = IDC_RAD_WIN_640; break;
        case 2: radioIdToSelect = IDC_RAD_WIN_960; break;
        case 3: radioIdToSelect = IDC_RAD_WIN_1280; break;
        default: break;
        }
        CheckRadioButton(hDlg, IDC_RAD_FS_640, IDC_RAD_WIN_1280, radioIdToSelect);

        // Update Window State Flag
        // Clear 0x20, Set 0x40 (Mark dialog as Active)
        g_window.someFlag2 = (g_window.someFlag2 & ~WND_FLAG_DIALOG_CANCELLED) | WND_FLAG_DIALOG_ACTIVE;

        return TRUE;
    }

    case WM_COMMAND:
    {
        // Check if the source is the "Start" button (0xD0)
        if (LOWORD(wParam) == IDC_BTN_START)
        {
            // Read Checkbox
            if (IsDlgButtonChecked(hDlg, IDC_CHK_ALWAYS_SHOW) == BST_CHECKED)
                g_supervisor.m_gameConfig.flags |= 0x100;
            else
                g_supervisor.m_gameConfig.flags &= ~0x100;

            // Read Radio Buttons to set display mode
            if (IsDlgButtonChecked(hDlg, IDC_RAD_FS_640) == BST_CHECKED)
                g_supervisor.m_gameConfig.displayMode = 0;
            else if (IsDlgButtonChecked(hDlg, IDC_RAD_WIN_640) == BST_CHECKED)
                g_supervisor.m_gameConfig.displayMode = 1;
            else if (IsDlgButtonChecked(hDlg, IDC_RAD_WIN_960) == BST_CHECKED)
                g_supervisor.m_gameConfig.displayMode = 2;
            else
                g_supervisor.m_gameConfig.displayMode = 3;

            // Clear Dialog Flags (Success path)
            g_window.someFlag2 &= ~WND_FLAG_DIALOG_MASK;

            EndDialog(hDlg, 6);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE: // 0x10
        if ((g_window.someFlag2 & WND_FLAG_DIALOG_MASK) == WND_FLAG_DIALOG_ACTIVE)
            g_window.someFlag2 = (g_window.someFlag2 & ~WND_FLAG_DIALOG_ACTIVE) | WND_FLAG_DIALOG_CANCELLED;

        EndDialog(hDlg, 6);
        return TRUE;
    }

    return FALSE;
}

// 0x445510
#if 0
int APIENTRY main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    int launchInfo;
    int initStatus;
    int result3;
    int result4;
    AnmManager* anmManagerBuf;
    int cursor_status_2;
    int res;
    int hr_;
    HRESULT hr;
    uint32_t windowHeight;
    int system_metrics;
    int windowHeight1;
    int cursorStatus;
    int cursor_status_3;
    int cursor_status_1;
    BOOL isMessageAvailable;
    int check_cookie_result;
    int i;
    HRESULT result;
    DWORD idk;
    int windowHeight2;
    LPCRITICAL_SECTION critical_section;
    D3DPRESENT_PARAMETERS* pDVar1;
    int message_loop_counter;
    void* d3dFormat; // code*
    uint32_t* puVar2;
    D3DFORMAT d3dformat;
    float time;
    int someWindowProcessedFlag;
    tagMSG tagMsg;
    BYTE keyboardBuffer[16];
    byte bStack_100;
    uint32_t local_c;
    int window_count;
    Chain* chain;
    IDirect3DDevice9** ppReturnedDeviceInterface;
    AnmManager* anmManager;
    ChainElem** calcChainNextElem;

    someWindowProcessedFlag = 0;
    g_window.hInstance = hInstance;

    timeBeginPeriod(1);

    g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag | 0x8000;
    for (int i = 0; i < 12; ++i)
        InitializeCriticalSection(&g_supervisor.criticalSections[i]);

    puts("---------- Touhou 11 Startup Log ----------\n");

    g_app = CreateMutexA(NULL, 1, "Touhou 11 App");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxA(
            NULL,
            "Another instance of the game is already running",
            "Error",
            MB_OK | MB_ICONERROR
        );
    }
    else
    {
        launchInfo = getLaunchInfo();
        if (launchInfo != -1)
        {
            g_supervisor.hInstance = hInstance;
            g_window.retrieveSystemStats();
            if (g_supervisor.verifyGameConfig() == 0)
            {
                GetKeyboardState(keyboardBuffer);
                if (((g_supervisor.m_gameConfig.flags & 0x100) != 0) || ((bStack_100 & 0x80) != 0))
                {
                    DialogBoxParamA(hInstance, (LPCSTR)0xcb, NULL, chooseResolutionDialog, 0);
                }
                if ((g_window.someFlag2 & 0x60) == 0)
                {
                    g_window.someFlag2 =
                        g_window.someFlag2 ^
                        ((uint)g_supervisor.m_gameConfig.displayMode * 4 ^ g_window.someFlag2) & 0xc;
                    // g_supervisor.calculateSelfChecksum(); Never used anyways
                    d3dFormat = (code*)0x0;
                    goto InitChain;
                }
            }
        }
    }
    do {
        do {
            do {
                while (true) {
                    g_soundManager.someState = 2;
                    FN_waitAndCloseGlobalHandles();
                    releaseSounds();
                    anmManager = g_anmManager;
                    if (g_anmManager != (AnmManager*)0x0) {
                        g_anmManager->~AnmManager();
                        free(anmManager);
                    }
                    g_anmManager = nullptr;
                    if (g_supervisor.surfaceR0 != nullptr)
                    {
                        (*(g_supervisor.surfaceR0)->lpVtbl->Release)(g_supervisor.surfaceR0);
                        g_supervisor.surfaceR0 = nullptr;
                    }
                    if (g_supervisor.surfaceR1 != nullptr)
                    {
                        g_supervisor.surfaceR1->Release();
                        g_supervisor.surfaceR1 = nullptr;
                    }
                    if (g_supervisor.backBuffer != nullptr)
                    {
                        g_supervisor.backBuffer->Release();
                        g_supervisor.backBuffer = nullptr;
                    }
                    if (g_supervisor.d3dDevice != nullptr)
                    {
                        g_supervisor.d3dDevice->Release();
                        g_supervisor.d3dDevice = nullptr;
                    }
                    if (g_supervisor.d3dInterface0 != nullptr)
                    {
                        g_supervisor.d3dInterface0->Release();
                        g_supervisor.d3dInterface0 = nullptr;
                    }
                    if (g_window.hwnd != (HWND)0x0) {
                        ShowWindow(g_window.hwnd, 0);
                        MoveWindow(g_window.hwnd, 0, 0, 0, 0, 0);
                        DestroyWindow(g_window.hwnd);
                        g_window.hwnd = (HWND)0x0;
                    }
                    do {
                        cursor_status_1 = ShowCursor(1);
                    } while (cursor_status_1 < 0);
                    if (someWindowProcessedFlag != 2) {
                        FN_writeToFile("th11.cfg", 0x3c, &g_supervisor.m_gameConfig);
                        timeEndPeriod(1);
                        logFunctionIdc();
                        g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag & 0xffff7fff;
                        window_count = 0xc;
                        /* Delete 12 critical sections */
                        critical_section = g_supervisor.criticalSections;
                        do {
                            DeleteCriticalSection(critical_section);
                            critical_section = critical_section + 1;
                            window_count = window_count + -1;
                        } while (window_count != 0);
                        SystemParametersInfoA(0x11, g_window.primaryScreenWorkingArea, (PVOID)0x0, 2);
                        SystemParametersInfoA(0x55, g_window.mouseSpeed, (PVOID)0x0, 2);
                        SystemParametersInfoA(0x56, g_window.idk1, (PVOID)0x0, 2);
                        WINNLSEnableIME(0, 1);
                        if (g_memoryTest != (DWORD*)0x0) {
                            free(g_memoryTest);
                        }
                        check_cookie_result = 0;
                        __security_check_cookie(stackCanary ^ (uint)&stack0xfffffec4);
                        return check_cookie_result;
                    }
                    g_loggingBuffer.pointer = g_loggingBuffer.buffer;
                    g_loggingBuffer.buffer[0] = '\0';
                    logToGlobalBuffer(&g_loggingBuffer,
                        "再起動を要するオプションが変更されたので再起動します\r\n"
                        , vaList);
                    if (g_supervisor.d3dPresetParameters.Windowed == 0) {
                        WINNLSEnableIME(0, 1);
                    }
                    d3dFormat = PeekMessageA_exref;
                    message_loop_counter = 0x3c;
                    do {
                        isMessageAvailable = PeekMessageA(&tagMsg, (HWND)0x0, 0, 0, 1);
                        if (isMessageAvailable != 0) {
                            TranslateMessage(&tagMsg);
                            DispatchMessageA(&tagMsg);
                        }
                        message_loop_counter = message_loop_counter + -1;
                    } while (message_loop_counter != 0);
                    g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag & 0xfffffe7f;
                InitChain:
                    g_chain = (Chain*)operator_new(0x4c);
                    if (g_chain == (Chain*)0x0) {
                        g_chain = (Chain*)0x0;
                    }
                    else {
                        (g_chain->calcChain).jobRunDrawChainCallbackOrTrackerPrev.trackerPrev = (ChainElem*)0x0
                            ;
                        (g_chain->calcChain).registerChainCallback = (ChainCallback*)0x0;
                        (g_chain->calcChain).runCalcChainCallback = (ChainCallback*)0x0;
                        (g_chain->calcChain).trackerJobNodeOrjobPriority.jobPriority = 0;
                        calcChainNextElem = &(g_chain->calcChain).nextNode;
                        *calcChainNextElem = (ChainElem*)((uint)*calcChainNextElem & 0xfffffffe);
                        (g_chain->calcChain).embeddedTracker.trackerJobNode = (ChainElem*)g_chain;
                        (g_chain->calcChain).embeddedTracker.trackerNextNode = (ChainElem*)0x0;
                        (g_chain->calcChain).embeddedTracker.trackerPrevNode = (ChainElem*)0x0;
                        calcChainNextElem = &(g_chain->drawChain).nextNode;
                        *calcChainNextElem = (ChainElem*)((uint)*calcChainNextElem & 0xfffffffe);
                        (g_chain->drawChain).jobRunDrawChainCallbackOrTrackerPrev.trackerPrev = (ChainElem*)0x0
                            ;
                        (g_chain->drawChain).registerChainCallback = (ChainCallback*)0x0;
                        (g_chain->drawChain).runCalcChainCallback = (ChainCallback*)0x0;
                        (g_chain->drawChain).trackerJobNodeOrjobPriority.jobPriority = 0;
                        (g_chain->drawChain).embeddedTracker.trackerJobNode = &g_chain->drawChain;
                        (g_chain->drawChain).embeddedTracker.trackerNextNode = (ChainElem*)0x0;
                        (g_chain->drawChain).embeddedTracker.trackerPrevNode = (ChainElem*)0x0;
                        g_chain->timeToRemove = 0;
                    }
                    checkJoystickAvailability();
                    normalizeKeyboardState();
                    g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag & 0xfffff3ff;
                    Supervisor::initializeInputDevices(&g_supervisor);
                    windowHeight = g_supervisor.criticalSectionFlag ^
                        ((uint)(g_supervisor.keyboard != (IDirectInputDevice8*)0x0) << 10 ^
                            g_supervisor.criticalSectionFlag) & 0x400;
                    g_supervisor.criticalSectionFlag =
                        windowHeight ^
                        ((uint)(g_supervisor.joystick != (IDirectInputDevice8*)0x0) << 0xb ^ windowHeight) &
                        0x800;
                    /* idek
                       no one seems to use this thing anyways */
                    ThreadInf::closeThread((ThreadInf*)&g_loadingThread);
                    g_supervisor.d3dInterface0 = Direct3DCreate9(0x20);
                    if (g_supervisor.d3dInterface0 != (IDirect3D9*)0x0) break;
                    CL_logError("Direct3D オブジェクトは何故か作成出来なかった\r\n");
                }
                result3 = Window::initialize(hInstance_);
            } while (result3 != 0);
            SoundManager::createThread(g_window.hwnd);
            result4 = Supervisor::initD3d9Devices((D3DFORMAT)d3dFormat);
        } while (result4 != 0);
        anmManagerBuf = (AnmManager*)operator_new(0x7bd894);
        if (anmManagerBuf == (AnmManager*)0x0) {
            g_anmManager = (AnmManager*)0x0;
        }
        else {
            g_anmManager = AnmManager::initialize(anmManagerBuf);
        }
        if (g_supervisor.d3dPresetParameters.Windowed == 0) {
            WINNLSEnableIME(0, 0);
            do {
                cursor_status_2 = ShowCursor(0);
            } while (-1 < cursor_status_2);
            SetCursor((HCURSOR)0x0);
        }
        g_window.someDouble = 0.0;
        g_window.frameDeltaTime = Window::getDeltaTime();
        g_window.frameSkipDeltaTime = g_window.frameDeltaTime;
        g_window.predictedDeltaTime = g_window.frameDeltaTime;
        g_window.timeSinceLastFrame = Window::getDeltaTime();
        g_window.deltaTime = g_window.timeSinceLastFrame;
        res = SetForegroundWindow(g_window.hwnd);
        Supervisor::initialize();
        if (res == 0) {
            g_window.someFlag2 = g_window.someFlag2 | 1;
            someWindowProcessedFlag = 0;
            g_window.frameskipCounter = -4;
        joined_r0x0044581d:
            do {
                while (true) {
                    if (g_window.timeForCleanup != 0) goto Cleanup;
                    isMessageAvailable = PeekMessageA(&tagMsg, (HWND)0x0, 0, 0, 1);
                    if (isMessageAvailable == 0) break;
                    TranslateMessage(&tagMsg);
                    DispatchMessageA(&tagMsg);
                }
                hr_ = (*(g_supervisor.d3dDevice)->lpVtbl->TestCooperativeLevel)(g_supervisor.d3dDevice);
                if (hr_ == 0) {
                    if ((g_window.someFlag2 & 2) == 0) {
                        if ((g_window.someFlag2 & 0x10) == 0) {
                            if ((g_supervisor.d3dPresetParameters.PresentationInterval == 1) &&
                                (g_supervisor.m_gameConfig.frameSkip == '\0')) {
                                hr_ = Window::frameIdkWhatVariationThisIs(&g_window);
                            }
                            else {
                                hr_ = Window::frameFrameskip(&g_window);
                            }
                        }
                        else {
                            Window::frame(&g_window);
                        }
                        someWindowProcessedFlag = hr_;
                        if (hr_ != 0) break;
                        g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag & 0xffffffef;
                        goto joined_r0x0044581d;
                    }
                }
                else if (hr_ != -0x7789f797) goto joined_r0x0044581d;
                g_window.idk2 = 10;
                if ((g_window.someFlag2 & 2) != 0) {
                    if ((g_window.someFlag2 & 0xc) == 0) {
                        GetWindowRect(g_window.hwnd, &g_supervisor.windowDimensions);
                        pDVar1 = &g_supervisor.d3dPresetParameters;
                        puVar2 = g_supervisor.idk3;
                        for (i = 0xe; i != 0; i = i + -1) {
                            *puVar2 = pDVar1->BackBufferWidth;
                            pDVar1 = (D3DPRESENT_PARAMETERS*)&pDVar1->BackBufferHeight;
                            puVar2 = puVar2 + 1;
                        }
                        g_supervisor.d3dPresetParameters.PresentationInterval =
                            (-(uint)((g_window.someFlag2 & 0x10) != 0) & 0x7fffffff) + 1;
                        g_supervisor.d3dPresetParameters.FullScreen_RefreshRateInHz = 0x3c;
                        g_supervisor.d3dPresetParameters.Windowed = 0;
                        g_supervisor.d3dPresetParameters.BackBufferFormat =
                            (uint)(g_supervisor.m_gameConfig.colorDepth != '\0') + D3DFMT_X8R8G8B8;
                    }
                    else {
                        g_supervisor.d3dPresetParameters.BackBufferFormat =
                            g_supervisor.d3dPresentBackBuferFormat;
                        g_supervisor.d3dPresetParameters.FullScreen_RefreshRateInHz = 0;
                        if ((g_window.someFlag2 & 0x10) == 0) {
                            g_supervisor.d3dPresetParameters.PresentationInterval =
                                (-(uint)(g_supervisor.d3dPresentationIntervalFlag != 0x3c) & 0x7fffffff) + 1;
                        }
                        else {
                            g_supervisor.d3dPresetParameters.PresentationInterval = 0x80000000;
                        }
                        g_supervisor.d3dPresetParameters.Windowed = 1;
                    }
                }
                g_supervisor.releaseSurfaces();
                g_anmManager->releaseTextures();
                hr = g_supervisor.d3dDevice->Reset(&g_supervisor.m_d3dPresetParameters);

                if (SUCCEEDED(hr))
                {
                    g_supervisor.resetRenderState();
                    g_anmManager->createD3DTextures(g_anmManager);
                    g_supervisor.criticalSectionFlag = g_supervisor.criticalSectionFlag | 0x10;
                    g_supervisor.idk7[3] = 3;
                    if ((g_window.someFlag2 & 2) != 0)
                    {
                        windowHeight = g_window.someFlag2 >> 2 & 3;
                        if (windowHeight == 0)
                        {
                            SetWindowLongA(g_window.hwnd, -0x10, -0x70000000);
                            SetWindowPos(g_window.hwnd, (HWND)0x0, 0, 0, 0x280, 0x1e0, 0x20);
                            WINNLSEnableIME(0, 0);
                            do {
                                cursor_status_3 = ShowCursor(0);
                            } while (-1 < cursor_status_3);
                            SetCursor((HCURSOR)0x0);
                            g_window.isAppUnfocused = 0;
                        }
                        else {
                            if (windowHeight == 3) {
                                system_metrics = GetSystemMetrics(7);
                                d3dformat = system_metrics * 2 + 0x500;
                                windowHeight2 = GetSystemMetrics(8);
                                windowHeight2 = windowHeight2 * 2 + 0x3c0;
                            }
                            else if (windowHeight == 2) {
                                windowHeight2 = GetSystemMetrics(7);
                                d3dformat = windowHeight2 * 2 + 0x3c0;
                                windowHeight2 = GetSystemMetrics(8);
                                windowHeight2 = windowHeight2 * 2 + 0x2d0;
                            }
                            else {
                                windowHeight2 = GetSystemMetrics(7);
                                d3dformat = windowHeight2 * 2 + 0x280;
                                windowHeight2 = GetSystemMetrics(8);
                                windowHeight2 = windowHeight2 * 2 + 0x1e0;
                            }
                            windowHeight1 = GetSystemMetrics(4);
                            SetWindowLongA(g_window.hwnd, -0x10, 0x10cb0000);
                            SetWindowPos(g_window.hwnd, (HWND)0x0, g_supervisor.windowDimensions.left,
                                g_supervisor.windowDimensions.top, d3dformat, windowHeight1 + windowHeight2
                                , 0x60);
                            ShowWindow(g_window.hwnd, 1);
                            WINNLSEnableIME(0, 1);
                            do {
                                cursorStatus = ShowCursor(1);
                            } while (cursorStatus < 0);
                        }
                    }
                    Supervisor::setupCameras(&g_supervisor);
                    g_window.someFlag2 = g_window.someFlag2 & 0xfffffffd;
                    goto joined_r0x0044581d;
                }
            } while (hr == -0x7789f798);
        }
        else {
            someWindowProcessedFlag = res;
            if (res != -1) {
                someWindowProcessedFlag = 2;
            }
        }
    Cleanup:
        g_supervisor.m_gameConfig.displayMode = (byte)(g_window.someFlag2 >> 2) & 3;
        if ((g_window.someFlag2 >> 2 & 3) != 0)
        {
            GetWindowRect(g_window.hwnd, &g_supervisor.windowDimensions);
            g_supervisor.m_gameConfig.windowPosX = g_supervisor.windowDimensions.left;
            g_supervisor.m_gameConfig.windowPosY = g_supervisor.windowDimensions.top;
        }

        //TODO: Implement
        static auto supervisor_cleanup = createCustomCallingConvention<Signature<Supervisor*>, Storage<EBX>>(0x429650);
        supervisor_cleanup(&g_supervisor);
        //g_supervisor.cleanup(&g_supervisor);

        chain = g_chain;
        if (g_chain)
        {
            Supervisor::releaseChains();
            game_free(chain);
        }
        g_chain = nullptr;
    } while (true);
}
#endif