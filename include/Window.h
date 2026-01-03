#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct FrameTimingData
{
    int sleepTimeMs;
    int lagCounter;
    int targetSleepTime;
};

#pragma pack(push, 1)
class Window
{
public:
    HWND hwnd;
    DWORD timeForCleanup;
    HINSTANCE hInstance;

    // Yes, it does seem like 4 options are used to represent two states
    BOOL isAppFocused;
    BOOL isAppUnfocused;

    char frameskipCounter;
    char idk0[3];
    LARGE_INTEGER performanceCounterFreq;
    LARGE_INTEGER performanceCounterValue;
    DWORD unusualLaunchFlag;
    DWORD primaryScreenWorkingArea;
    DWORD mouseSpeed;
    int idk1;
    uint32_t someFlag2;
    int idk2;
    double frameDeltaTime;
    double frameSkipDeltaTime;
    double deltaTime;
    int idk3[2];
    double someDouble;
    double k;
    int idk4;
    int someInt;
    int frameTimingDataIndex;
    int idk5;
    FrameTimingData frameTimingData[3];
    int idk6[2];

    // 0x445e00
    static LRESULT CALLBACK wndProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lpParam);

    // 0x446ae0
    static int initialize(HINSTANCE hInstance);

private:
    static void setDarkTitleBar(HWND hwnd);

    static constexpr LPCSTR TOUHOU_WINDOW_CLASS_NAME = "BASE";
    static constexpr LPCSTR TOUHOU_WINDOW_TITLE = "Subterranean Animism";
    static constexpr DWORD WINDOW_STYLE_WINDOWED = (WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    static constexpr DWORD WINDOW_STYLE_FULLSCREEN = (WS_POPUP | WS_VISIBLE);
};
#pragma pack(pop)
ASSERT_SIZE(Window, 0xac);