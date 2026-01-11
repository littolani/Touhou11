#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct ThBgmFormat
{
    char name[16];
    int startOffset;
    DWORD preloadAllocSize;
    int introLength;
    int totalLength;
    WAVEFORMATEX format;
};

struct CWaveFile
{
public:
    WAVEFORMATEX* waveFormat;
    MMCKINFO mmckinfo;
    int idk0[5];
    DWORD dwSize;
    int idk1[18];
    ULONG dataSize;
    DWORD someNumber;
    uint32_t m_ulDataSize;
    uint32_t totalSize;
    int bgmLength;
    HANDLE fileHandle;
    ThBgmFormat* thBgmFormat;
    LPCSTR audioFilename;
    int filePointer;

    // CWaveFile* This, ThBgmFormat* thBgmFormat, const char* filepath

    /**
     * 0x45b220
     * @brief
     * @param  This        ESI:4
     * @param  thBgmFormat EBX:4
     * @param  filepath    EDI:4
     * @return HRESULT     EAX:4
     */
    static HRESULT openWavFile(CWaveFile* This, ThBgmFormat* thBgmFormat, const char* filepath);
};
ASSERT_SIZE(CWaveFile, 0x9c); // Verified