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
    BOOL m_readMode;
    byte* m_memBufferStart;
    byte* m_memBufferCurrent;
    DWORD m_memBufferLength;
    HANDLE m_fileHandle;
    ThBgmFormat* thBgmFormat;
    LPCSTR audioFilename;
    int filePointer;

    /**
     * 0x45b220
     * @brief
     * @param  This        ESI:4
     * @param  thBgmFormat EBX:4
     * @param  filepath    EDI:4
     * @return HRESULT     EAX:4
     */
    static HRESULT open(CWaveFile* This, ThBgmFormat* thBgmFormat, const char* filepath);

    /**
     * 0x45b480
     * @brief
     * @param  This         ESI:4
     * @param  pBuffer      Stack[0x4]:4
     * @param  pdwSizeRead  Stack[0x8]:4
     * @param  dwSizeToRead EAX:4
     * @return HRESULT      EAX:4
     */
    static HRESULT read(CWaveFile* This, BYTE* pBuffer, DWORD* pdwSizeRead, DWORD dwSizeToRead);

    /**
     * 0x45b2d0
     * @brief
     * @param  This        ECX:4
     * @param  thBgmFormat EAX:4
     * @param  seekOffset  Stack[0x4]:4
     * @return HRESULT     EAX:4
     */
    static HRESULT reopen(CWaveFile* This, ThBgmFormat* thBgmFormat, int seekOffset);

    /**
     * 0x45b3a0
     * @brief
     * @param  This         ESI:4
     * @param  loop         BL:1
     * @param  seekOffset   EDI:4
     * @return HRESULT      EAX:4
     */
    static HRESULT resetFile(CWaveFile* This, bool loop, int seekOffset);
};
ASSERT_SIZE(CWaveFile, 0x9c); // Verified