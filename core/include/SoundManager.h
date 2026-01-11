#pragma once
#include "Chireiden.h"
#include "CStreamingSound.h"
#include "Macros.h"

struct SoundConfig
{
    int idk;
    short paddingMaybe;
    short bufferIndex;
};
ASSERT_SIZE(SoundConfig, 0x8);

// lots of repeated stuff going on--may be errors
class SoundManager
{
public:
    IDirectSound8* dsound;                  // <0x0>
    IDirectSoundBuffer** soundBuffer;       // <0x4>
    LPDIRECTSOUNDBUFFER soundBuffersArray;  // <0x8>
    CWaveFile* cwaveFile;                   // <0xc>
    int idk0[22];                           // <0x10>
    DWORD writeCursor;                      // <0x68>
    int idk3;                               // <0x6c>
    DWORD writeCursorOffset;                // <0x70>
    int idk4[101];                          // <0x74>
    LPDIRECTSOUNDBUFFER dsoundBuffers[128]; // <0x208>
    uint32_t m_soundBufferIndices[128];     // <0x408>
    IDirectSoundBuffer* dSoundBuffer;       // <0x608>
    HWND hwnd;                              // <0x60c>
    LPDIRECTSOUND8* dsoundIface;            // <0x610>
    DWORD threadId;                         // <0x614>
    HANDLE threadHandle;                    // <0x618>
    int idk5;                               // <0x61c>
    int m_sfxSlots[12];                     // <0x620>
    int m_sfxActiveCounts[12];              // <0x650>
    int m_sfxInstanceData[12][128];         // <0x680>
    int idk6[16];
    int someArray;
    int idk7[15];
    int* someArray2;
    int idk8[31];
    int bgmFormatIndexMaybe;
    ThBgmFormat* bgmPreloadFmtData[16];     // <0x1f84>
    int idk9[3153];
    char bgmFilename[256];
    CStreamingSound* cStreamingSound;        // <0x5208>
    int idk11;
    int idk12;
    int someWaveFileOffset;
    int idk10[52];
    int bgmVolume;
    int sfxVolume;
    int adjustedSfxVolumeMaybe;

    /**
     * 0x449660
     * Notes: Assumes 4-byte aligned or even-sized chunks
     * This code only works on LE systems.
     * RIFF/WAVE specifies that all multi-byte integer fields are LE.
     * @brief Finds a specific chunk in a RIFF/WAVE buffer.
     * @param  ECX:4        searchPtr      Pointer to the start of the chunks to search.
     * @param  Stack[0x4]:4 targetId       4-character ID to look for (e.g., "fmt " or "data").
     * @param  EAX:4        remainingSize  The total size of the sub-container to search within.
     * @param  EDI:4        outChunkSize   Receives the size of the found chunk's data.
     * @return EAX:4                       Char pointer to the chunk's data, or nullptr if not found.
     */
    static char* findRiffChunk(char* searchPtr, const char* targetId, int remainingSize, int* outChunkSize);

    static void stopBombReimuAB();

    /**
     * 0x44a1e0
     * @brief
     * @param soundIndex ESI:4
     */
    static void playSoundCentered(int soundId);

    /**
     * 0x44a260
     * @brief
     * @param xOffsetFromCenter Stack[0x4]:4
     * @param soundId           EDI:4
     */
    static void playSoundWithPan(float xOffsetFromCenter, int soundId);
};
ASSERT_SIZE(SoundManager, 0x52f4);