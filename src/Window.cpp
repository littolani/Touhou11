#include "Window.h"
#include "Globals.h"
#include "Supervisor.h"

LRESULT Window::wndProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lpParam)
{
    switch (uMsg)
    {
    case WM_ACTIVATEAPP:
        g_window.isAppUnfocused = (wParam == FALSE);
        g_window.isAppFocused = wParam;
        break;

    case WM_SIZE:
        if (wParam == SIZE_MAXIMIZED && (g_window.someFlag2 & 1))
        {
            g_window.someFlag2 = (g_window.someFlag2 & ~0xC) | 2;
            return DefWindowProcA(hwnd, WM_SIZE, SIZE_MAXIMIZED, lpParam);
        }
        break;

    case WM_CLOSE:
        g_supervisor.criticalSectionFlag = (g_supervisor.criticalSectionFlag & ~0x100) | 0x80;
        return 1;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR:
        if (g_supervisor.m_d3dPresetParameters.Windowed)
        {
            SetCursor(LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_ARROW)));
            ShowCursor(TRUE);
            return 1;
        }

        // If fullscreen but unfocused (e.g., alt-tabbed), ensure cursor is visible
        if (g_window.isAppUnfocused)
        {
            SetCursor(LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_ARROW)));
            // Force show cursor counter to be >= 0
            while (ShowCursor(TRUE) < 0);
            return 1;
        }

        // If fullscreen and focused, hide the cursor
        // Force show cursor counter to be < 0
        while (ShowCursor(FALSE) >= 0);
        SetCursor(NULL);
        return 1;

    case WM_SYSCOMMAND:
        // Filter out system menu activation to prevent game interruption
        if ((wParam & 0xFFF0) == SC_MOUSEMENU || (wParam & 0xFFF0) == SC_KEYMENU)
            return 1; // Eat the command
        break;

    case WM_LBUTTONDOWN:
        SetForegroundWindow(hwnd);
        break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lpParam);
}

int Window::initialize(HINSTANCE hInstance)
{
    WNDCLASSA windowClass;
    windowClass.style = 0;
    windowClass.lpfnWndProc = NULL;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = NULL;
    windowClass.hIcon = NULL;
    windowClass.hCursor = NULL;
    windowClass.hbrBackground = NULL;
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = NULL;
    windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_ARROW));
    windowClass.lpfnWndProc = /*(WNDPROC)0x445e00;*/  wndProcCallback;
    windowClass.lpszClassName = TOUHOU_WINDOW_CLASS_NAME;
    windowClass.hInstance = hInstance;
    RegisterClassA(&windowClass);

    // Yes, it does seem like 4 options are used to represent two states
    g_window.isAppFocused = 1;
    g_window.isAppUnfocused = 0;

    uint32_t modeBits = (g_supervisor.m_gameConfig.displayMode << 2);
    g_window.someFlag2 = (g_window.someFlag2 & ~0x0C) | (modeBits & 0x0C);

    BOOL isWindowedMode = (g_window.someFlag2 & 0x0C) != 0;
    g_supervisor.m_d3dPresetParameters.Windowed = isWindowedMode;

    if ((g_supervisor.m_gameConfig.frameSkip == 0) && (g_supervisor.m_gameConfig.latencyMode == 2))
        g_window.someFlag2 |= 0x10;
    else
        g_window.someFlag2 &= ~0x10;

    g_window.idk5 = 15;
    g_window.frameTimingData[0].sleepTimeMs = 15;
    g_window.frameTimingData[0].lagCounter = 0;
    g_window.frameTimingData[0].targetSleepTime = 12;
    g_window.frameTimingData[1].sleepTimeMs = 12;
    g_window.frameTimingData[1].lagCounter = 0;
    g_window.frameTimingData[1].targetSleepTime = 12;
    g_window.frameTimingData[2].sleepTimeMs = 12;
    g_window.frameTimingData[2].lagCounter = 0;
    g_window.frameTimingData[2].targetSleepTime = 8;
    g_window.idk6[0] = 8;
    g_window.idk6[1] = 0;
    g_window.frameTimingDataIndex = 0;

    if (isWindowedMode)
    {
        uint32_t windowScaleMode = (g_window.someFlag2 >> 2) & 3;
        int borderW = GetSystemMetrics(SM_CXFIXEDFRAME);
        int borderH = GetSystemMetrics(SM_CYFIXEDFRAME);
        int windowHeight;
        int windowWidth;

        if (windowScaleMode == 3)
        {
            windowWidth = borderW * 2 + 1280;
            windowHeight = borderH * 2 + 960;
        }
        else if (windowScaleMode == 2)
        {
            windowWidth = borderW * 2 + 960;
            windowHeight = borderH * 2 + 720;
        }
        else
        {
            windowWidth = borderW * 2 + 640;
            windowHeight = borderH * 2 + 480;
        }

        int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);

        g_window.hwnd = CreateWindowExA(
            0,
            TOUHOU_WINDOW_CLASS_NAME,
            TOUHOU_WINDOW_TITLE,
            WINDOW_STYLE_WINDOWED,
            g_supervisor.m_gameConfig.windowPosX,
            g_supervisor.m_gameConfig.windowPosY,
            windowWidth,
            titleBarHeight + windowHeight,
            NULL,
            NULL,
            hInstance,
            NULL
        );
    }
    else
    {
        // Fullscreen Mode (Always 640x480 resolution for the window rect)
        g_window.hwnd = CreateWindowExA(
            0,
            TOUHOU_WINDOW_CLASS_NAME,
            TOUHOU_WINDOW_TITLE,
            WINDOW_STYLE_FULLSCREEN,
            0,
            0,
            640,
            480,
            NULL,
            NULL,
            hInstance,
            NULL
        );
    }

    GetWindowRect(g_window.hwnd, &g_supervisor.windowDimensions);
    g_supervisor.appWindow = g_window.hwnd;

    if (g_window.hwnd == NULL)
        return 1;

    SendMessageA(g_window.hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    Sleep(16);
    SendMessageA(g_window.hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    //setDarkTitleBar(g_window.hwnd);
    return 0;
}

void Window::setDarkTitleBar(HWND hwnd)
{
    //BOOL enable = TRUE;

    //// Try the newer attribute first
    //if (FAILED(DwmSetWindowAttribute(
    //    hwnd,
    //    20, // DWMWA_USE_IMMERSIVE_DARK_MODE
    //    &enable,
    //    sizeof(enable))))
    //{
    //    // Fallback for older Windows 10
    //    DwmSetWindowAttribute(
    //        hwnd,
    //        19, // DWMWA_USE_IMMERSIVE_DARK_MODE_OLD
    //        &enable,
    //        sizeof(enable));
    //}
}