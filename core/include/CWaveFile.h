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
    WAVEFORMATEX* waveFormat;
    MMCKINFO mmckinfo;
    int idk0[5];
    DWORD dwSize;
    int idk1[18];
    ULONG dataSize;
    DWORD someNumber;
    uint32_t m_ulDataSize;
    byte* pbDataCurrent;
    int bgmLength;
    HANDLE fileHandle;
    ThBgmFormat* thBgmFormat;
    LPCSTR audioFilename;
    int filePointer;
};
ASSERT_SIZE(CWaveFile, 0x9c);