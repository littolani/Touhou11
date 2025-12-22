#pragma once
#include "Chireiden.h"
#include "FileAbstrction.h"
#include "RenderVertex.h"
#include "Rng.h"

class AnmManager;
class Supervisor;
class AsciiManager;
struct Chain;

extern HANDLE g_app;
extern HWND g_window;
extern DWORD g_unusualLaunchFlag;
extern HINSTANCE g_hInstance;
extern DWORD g_primaryScreenWorkingArea;
extern DWORD g_mouseSpeed;
extern DWORD g_screenWorkingArea;
extern LARGE_INTEGER g_performanceFrequency;
extern LARGE_INTEGER g_performanceCount;
extern float g_gameSpeed;
extern double g_time;
extern RngContext g_anmRngContext;
extern RngContext g_replayRngContext;

extern "C" {
    // These link directly to the labels defined in Symbols.asm
    extern RenderVertex144 g_renderQuad144[4];
    extern Supervisor g_supervisor;
    extern AnmManager* g_anmManager;
}

extern D3DFORMAT g_d3dFormats[];
extern uint32_t g_bytesPerPixelLookupTable[];


double getDeltaTime();
int fileExists(LPCSTR filePath);
int createDirectory(LPCSTR pathName);
int writeToFile(LPCSTR fileName, DWORD numBytes, LPVOID bytes);
byte* openFile(const char* filename, size_t* outSize, BOOL isExternalResource);