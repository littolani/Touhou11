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
#include "StdVm.h"
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
    vprintf(format, args);
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

void snapshot()
{
    const char* a = reinterpret_cast<char*>(0x4c9928);
    puts(a);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        setupConsole();

        installHook(0x4540f0, createLtoThunk<ESI>(AnmLoadedD3D::createTextureFromAtR, 0));


        //installHook(0x455a70, createLtoThunk<Stack<0x4>, Stack<0x8>, Stack<0xc>, Stack<0x10>, EAX>(AsciiManager::spawnAnm, 0x10));
        // 
        // 
        //installHook(0x4014e0, createLtoThunk<ESI, ECX, EBX>(AsciiManager::loadAsciiStrings, 0));
        //
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
        installHook(0x453220, AnmManager::setupRenderSquare);
        installHook(0x4569c0, AnmManager::allocateVm);

        // Already hooked through internal calls
        //installHook(0x4503d0, createLtoThunk<EAX, Stack<0x4>, EBX, EDI, ESI>(AnmVm::applyZRotationToQuadCorners, 0x4));
        //installHook(0x4500f0, createLtoThunk<Stack<0x4>, EBX, EDI, EDX, ESI>(AnmVm::writeSpriteCharacters, 0x4));
        //installHook(0x450700, createLtoThunk<Returns<RegCode::EAX>, EAX>(AnmVm::projectQuadCornersThroughCameraViewport, 0));
        

        //installHook(0x452420, createLtoThunk<Returns<RegCode::EAX>, ECX>(AnmVm::onTick, 0));


        installHook(0x40a280, createLtoThunk<EDX>(Bullet::step_ex_00_speedup, 0));
        installHook(0x40a300, createLtoThunk<EAX>(Bullet::step_ex_02_accel, 0));

        installHook(0x45b220, createLtoThunk<Returns<RegCode::EAX>, ESI, EBX, EDI, EAX>(CWaveFile::open, 0));
        installHook(0x45b480, createLtoThunk<Returns<RegCode::EAX>, ESI, Stack<0x4>, Stack<0x8>, EAX>(CWaveFile::read, 0x8));
        installHook(0x45b3a0, createLtoThunk<Returns<RegCode::EAX>, ESI, BL, EDI>(CWaveFile::resetFile, 0));
        installHook(0x45b2d0, createLtoThunk<Returns<RegCode::EAX>, ECX, EAX, Stack<0x4>>(CWaveFile::reopen, 0x4));

        installHook(0x449660, createLtoThunk<Returns<RegCode::EAX>, ECX, Stack<0x4>, EAX, EDI>(SoundManager::findRiffChunk, 0x4));
        installHook(0x44a1e0, createLtoThunk<ESI>(SoundManager::playSoundCentered, 0));
        installHook(0x44a260, createLtoThunk<Stack<0x4>, EDI>(SoundManager::playSoundWithPan, 0x4));

        installHook(0x457080, createLtoThunk<EDX, ECX>(Chain::cut, 0));
        installHook(0x456cb0, createLtoThunk<Returns<RegCode::EAX>, EBX>(Chain::runCalcChain, 0));
        installHook(0x456e10, createLtoThunk<Returns<RegCode::EAX>>(Chain::runDrawChain, 0));
        installHook(0x456c10, createLtoThunk<Returns<RegCode::EAX>, ESI, EBX>(Chain::registerDrawChain, 0));

        installHook(0x441760, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, ECX>(PbgArchive::readDecompressEntry, 0x8));
        installHook(0x4418d0, createLtoThunk<Returns<RegCode::EAX>, EAX, EBX>(PbgArchive::findEntry, 0));
        installHook(0x4426c0, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, Stack<0xc>, EAX>(Lzss::decompress, 0xc));
        installHook(0x4423a0, createLtoThunk<Returns<RegCode::EAX>, Stack<0x4>, Stack<0x8>, Stack<0xc>>(Lzss::compress, 0xc));
        installHook(0x4581c0, createLtoThunk<Stack<0x4>, Stack<0x8>, AL, Stack<0xc>, Stack<0x10>, Stack<0x14>>(FileUtils::decrypt, 0x14));

        installHook(0x437500, createLtoThunk<Returns<RegCode::EAX>, ECX, Stack<0x4>>(ScoreManager::calculateChecksum, 0x4));

        installHook(0x403950, createLtoThunk<Returns<RegCode::EAX>, EAX, EDX, Stack<0x4>, ECX>(StdVm::checkEntityVisibility, 0x4));

        installHook(0x446d30, createLtoThunk<Returns<RegCode::EAX>, EDI>(Supervisor::initD3d9Devices, 0));

        installHook(0x459270, createLtoThunk<Returns<RegCode::EAX>, ESI>(Timer::increment, 0));

        installHook(0x446ae0, createLtoThunk<Returns<RegCode::EAX>, EBX>(Window::initialize, 0));
        installHook(0x4461f0, createLtoThunk<ESI>(Window::update, 0));

        // Globals
        installHook(0x458400, createLtoThunk<Returns<RegCode::EAX>, EAX, Stack<0x4>, Stack<0x8>>(openFile, 0x8));
        installHook(0x40b9d0, createLtoThunk<ECX, Stack<0x4>, Stack<0x8>>(projectMagnitudeToVectorComponents, 0x8));
        installHook(0x459bc0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x441c80, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x44ab00, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x456ad0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x45b5a0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x41f6e0, createLtoThunk<Stack<0x4>, Stack<0x8>>(logfThunk, 0));
        installHook(0x429ca0, snapshot);
        installHook(0x458a10, createLtoThunk<Stack<0x4>>(logThunk, 0));
        installHook(0x458af0, createLtoThunk<Stack<0x4>>(logThunk, 0));

        //waitForDebugger();
    }
    return TRUE;
}