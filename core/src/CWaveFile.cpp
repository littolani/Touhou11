#include "CWaveFile.h"
#include "Globals.h"

HRESULT CWaveFile::open(CWaveFile* This, ThBgmFormat* thBgmFormat, const char* filepath)
{
    if (!filepath)
        return E_INVALIDARG;

    printf("CWaveFile open: %s\n", filepath);

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

    This->m_fileHandle = hFile;
    if (hFile == INVALID_HANDLE_VALUE)
        return E_FAIL;

    This->thBgmFormat = thBgmFormat;
    This->audioFilename = filepath;
    if (This->m_readMode == 0)
    {
        if (hFile)
        {
            SetFilePointer(hFile, thBgmFormat->startOffset + g_soundManager.someWaveFileOffset, NULL, 0);
            This->mmckinfo.cksize = This->thBgmFormat->totalLength;
        }
    }
    else
    {
        This->m_memBufferCurrent = This->m_memBufferStart;
        if (thBgmFormat->totalLength > 0)
            This->m_memBufferLength = thBgmFormat->totalLength;
    }
    This->dwSize = This->mmckinfo.cksize;
    return 0;
}


HRESULT CWaveFile::reopen(CWaveFile* This, ThBgmFormat* thBgmFormat, int seekOffset)
{
    puts("CWaveFile reopen");
    if (This->m_readMode != 0)
        return E_FAIL;

    if (This->m_fileHandle == INVALID_HANDLE_VALUE)
    {
        This->dataSize = 1;
        This->m_readMode = 0;
        open(This, thBgmFormat, This->audioFilename);
        if (This->m_fileHandle == INVALID_HANDLE_VALUE)
            return E_FAIL;
    }
    This->thBgmFormat = thBgmFormat;
    resetFile(This, false, seekOffset);
    This->dwSize = (This->mmckinfo).cksize;
    return S_OK;
}

HRESULT CWaveFile::read(CWaveFile* This, BYTE* pBuffer, DWORD* pdwSizeRead, DWORD dwSizeToRead)
{
    if (This->m_readMode == 0)
    {
        HANDLE hFile = This->m_fileHandle;

        if (!hFile) return
            CO_E_NOTINITIALIZED;

        if (!pBuffer || !pdwSizeRead)
            return E_INVALIDARG;

        DWORD chunkSize = This->mmckinfo.cksize;

        if (dwSizeToRead > chunkSize)
            dwSizeToRead = chunkSize;

        This->mmckinfo.cksize = chunkSize - dwSizeToRead;
        DWORD bytesRead = 0;
        ReadFile(hFile, pBuffer, dwSizeToRead, &bytesRead, NULL);

        if (pdwSizeRead)
            *pdwSizeRead = bytesRead;

        return S_OK;
    }
    else
    {
        byte* currentPtr = This->m_memBufferCurrent;

        if (!currentPtr)
            return CO_E_NOTINITIALIZED;

        if (pdwSizeRead)
            *pdwSizeRead = 0;

        byte* bufferEnd = This->m_memBufferStart + This->m_memBufferLength;
        byte* targetEnd = currentPtr + dwSizeToRead;

        if (targetEnd > bufferEnd)
            dwSizeToRead = (DWORD)(bufferEnd - currentPtr);

        memcpy(pBuffer, currentPtr, dwSizeToRead);

        This->m_memBufferCurrent = currentPtr + dwSizeToRead;

        if (pdwSizeRead)
            *pdwSizeRead = dwSizeToRead;

        return S_OK;
    }
}

HRESULT CWaveFile::resetFile(CWaveFile* This, bool loop, int seekOffset)
{
    if (This->m_readMode == 0)
    {
        HANDLE hFile = This->m_fileHandle;
        if (hFile == NULL)
            return CO_E_NOTINITIALIZED;

        DWORD movePos = 0;
        DWORD chunkSize = 0;

        if (loop && This->thBgmFormat->introLength > 0)
        {
            movePos = This->thBgmFormat->startOffset + This->thBgmFormat->introLength;
            chunkSize = This->thBgmFormat->totalLength - This->thBgmFormat->introLength;
        }

        else if (seekOffset == 0)
        {
            movePos = This->thBgmFormat->startOffset;
            chunkSize = This->thBgmFormat->totalLength;
        }
        else
        {
            movePos = seekOffset;
            chunkSize = (This->thBgmFormat->totalLength + This->thBgmFormat->startOffset) - seekOffset;
        }

        movePos += g_soundManager.someWaveFileOffset;
        SetFilePointer(hFile, movePos, NULL, FILE_BEGIN);
        This->mmckinfo.cksize = chunkSize;
    }
    else
    {
        This->m_memBufferCurrent = This->m_memBufferStart;

        if (This->thBgmFormat->totalLength > 0)
            This->m_memBufferLength = This->thBgmFormat->totalLength;

        if (loop && This->thBgmFormat->introLength > 0)
            This->m_memBufferCurrent += This->thBgmFormat->introLength;
    }

    return S_OK;
}


