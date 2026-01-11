#include "CWaveFile.h"
#include "Globals.h"

HRESULT CWaveFile::openWavFile(CWaveFile* This, ThBgmFormat* thBgmFormat, const char* filepath)
{
    if (!filepath)
        return E_INVALIDARG;

    printf("CWaveFile Constructor: %s\n", filepath);

    HANDLE hFile = CreateFileA(
        filepath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL |
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    This->fileHandle = hFile;
    if (hFile == INVALID_HANDLE_VALUE)
        return E_FAIL;

    This->thBgmFormat = thBgmFormat;
    This->audioFilename = filepath;
    if (This->someNumber == 0)
    {
        if (hFile)
        {
            SetFilePointer(hFile, thBgmFormat->startOffset + g_soundManager.someWaveFileOffset, NULL, 0);
            This->mmckinfo.cksize = This->thBgmFormat->totalLength;
        }
    }
    else
    {
        This->totalSize = This->m_ulDataSize;
        if (thBgmFormat->totalLength > 0)
            This->bgmLength = thBgmFormat->totalLength;
    }
    This->dwSize = This->mmckinfo.cksize;
    return 0;
}

