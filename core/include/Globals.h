#pragma once
#include "Chireiden.h"
#include "Shottypes.h"
#include "Timer.h"
#include "RenderVertex.h"
#include "Lzss.h"
#include "Rng.h"
#include "Supervisor.h"
#include "SoundManager.h"

class AnmManager;
class Supervisor;
class AsciiManager;
struct Chain;
class Window;
struct PbgArchive;
class Spellcard;
struct FpsCounter;
extern RngContext g_anmRngContext;
extern RngContext g_replayRngContext;
class Player;

struct Globals
{
    int scoreLimit;
    int currentScore;
    int currentPower;
    int u1;
    int currentPiv;
    int u2;
    int u3;
    Timer timer;
    CharacterId character;
    SubshotId subshot;
    int currentLives;
    int currentLifeFragments;
    int difficulty;
    int u4;
    int currentStage;
    int currentStageCopy;
    int u5;
    int timeInStage;
    int timeInChapter;
    int continuesUsed;
    int alsoContinuesUsed;
    int rank;
    int maxPower;
    int powerPerLevel;
    int livesMaybe;
    int graze;
};
ASSERT_SIZE(Globals, 0x78);

// These link directly to the labels defined in symbols.asm
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
    extern RenderVertexSq g_squareVertices[4];
    extern float g_gameSpeed;
    extern Globals g_globals;
    extern Player* g_player;
    extern SoundManager g_soundManager;
    extern SoundConfig g_soundConfigTable[56];

    void __cdecl game_free(void* memory);
    void* __cdecl game_malloc(size_t size);
    void* __cdecl game_new(size_t size);
}

extern D3DFORMAT g_d3dFormats[];
extern uint32_t g_bytesPerPixelLookupTable[];

int fileExists(LPCSTR filePath);
int createDirectory(LPCSTR pathName);
int writeToFile(LPCSTR fileName, DWORD numBytes, LPVOID bytes);

// 0x40b9d0
void projectMagnitudeToVectorComponents(D3DXVECTOR3* vec, float theta, float scale);

/**
 * 0x458400
 * @brief  Returns a memory-mapped file
 * @param  filename           EAX:4
 * @param  outSize            Stack[0x4]:4
 * @param  isExternalResource Stack[0x8]:4
 * @return byte*              EAX:4
 */
byte* openFile(const char* filename, size_t* outSize, BOOL isExternalResource);