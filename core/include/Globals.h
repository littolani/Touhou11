#pragma once
#include "Chireiden.h"
#include "RenderVertex.h"
#include "Lzss.h"
#include "Rng.h"

class AnmManager;
class Supervisor;
class AsciiManager;
struct Chain;
class Window;
struct PbgArchive;
class Spellcard;
struct FpsCounter;

extern float g_gameSpeed;
extern RngContext g_anmRngContext;
extern RngContext g_replayRngContext;

// These link directly to the labels defined in Symbols.asm
extern "C"
{
    extern RenderVertex144 g_renderQuad144[4];
    extern Supervisor g_supervisor;
    extern AnmManager* g_anmManager;
    extern Chain* g_chain;
    extern Window g_window;
    extern PbgArchive g_pbgArchive;
    extern PbgArchive* g_pbgArchives;
    extern int g_numEntriesInDatFile;
    extern LzssTreeNode g_lzssTree[0x2001];
    extern byte g_lzssDict[0x2000];
    extern Spellcard* g_spellcard;
    extern FpsCounter* g_fpsCounter;

    void __cdecl game_free(void* memory);
    void* __cdecl game_malloc(size_t size);
    void* __cdecl game_new(size_t size);
}

extern D3DFORMAT g_d3dFormats[];
extern uint32_t g_bytesPerPixelLookupTable[];

int fileExists(LPCSTR filePath);
int createDirectory(LPCSTR pathName);
int writeToFile(LPCSTR fileName, DWORD numBytes, LPVOID bytes);

/**
 * 0x458400
 * @brief  Returns a memory-mapped file
 * @param  filename           EAX:4
 * @param  outSize            Stack[0x4]:4
 * @param  isExternalResource Stack[0x8]:4
 * @return byte*              EAX:4
 */
byte* openFile(const char* filename, size_t* outSize, BOOL isExternalResource);