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

    // idek anymore--they probably represent more than that 
    // or else why would there be 4 possible states
    DWORD isAppFocused;
    DWORD isAppUnfocused;

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
};
#pragma pack(pop)
ASSERT_SIZE(Window, 0xac);