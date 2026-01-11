#include "SoundManager.h"
#include "Globals.h"

char* SoundManager::findRiffChunk(char* searchPtr, const char* targetId, int remainingSize, int* outChunkSize)
{
    if (remainingSize != 0)
    {
        do {
            // The size of the current chunk is at offset 4
            int currentChunkSize = *(int*)(searchPtr + 4);
            *outChunkSize = currentChunkSize;

            // Compare the 4-character ID at offset 0
            if (strncmp(searchPtr, targetId, 4) == 0)           
                return searchPtr + 8; // Return pointer to the data (skipping ID and Size fields)

            // Move to the next chunk: current pointer + 8-byte header + data size
            searchPtr += (currentChunkSize + 8);
            remainingSize -= (currentChunkSize + 8);

        } while (remainingSize != 0);
    }

    return nullptr;
}

// 0x44a300
void SoundManager::stopBombReimuAB()
{
    SoundManager* This = &g_soundManager;
    const int BOMBLOOP_LOGICAL_ID = 0x31; // 0x31 is the Logical ID for the Reimu AB Bomb Loop.

    int* pSlotId = This->m_sfxSlots;
    int slotIndex = 0;

    // Try to find the running loop instance
    while (*pSlotId != -1)
    {
        if (*pSlotId == BOMBLOOP_LOGICAL_ID)
            This->m_sfxActiveCounts[slotIndex] = -1; // Setting active count to -1 signals the mixer to release the buffer immediately.

        pSlotId++;
        slotIndex++;

        if (slotIndex >= 12)
            return; // Not found, nothing to kill
    }

    // If we didn't find it playing, but we have a free slot,
    // claim the slot for ID 0x31 anyway.
    if (slotIndex < 12)
        This->m_sfxSlots[slotIndex] = BOMBLOOP_LOGICAL_ID;
}

void SoundManager::playSoundCentered(int soundId)
{
    printf("Playing sound %d centered\n", soundId);
    SoundManager* This = &g_soundManager;
    int16_t bufferConfig = g_soundConfigTable[soundId].bufferIndex;

    int slotIndex = 0;
    int* pSlotId = This->m_sfxSlots;

    while (true)
    {
        int currentSlotId = *pSlotId;

        // Found empty slot
        if (currentSlotId < 0)
        {
            if (slotIndex < 12)
            {
                This->m_sfxSlots[slotIndex] = soundId;
                This->m_soundBufferIndices[soundId] = bufferConfig;
                This->m_sfxInstanceData[slotIndex][0] = 0;
                This->m_sfxActiveCounts[slotIndex]++;
            }
            return;
        }

        // This sound is already active in This slot. We just add another instance (polyphony).
        if (currentSlotId == soundId)
            break;

        pSlotId++;
        slotIndex++;

        // End of array check (12 slots max)
        if (slotIndex >= 12)
            return; // No room left in the queue, sound is dropped.
    }

    // Check polyphony limit (Max 128 instances per sound)
    int currentCount = This->m_sfxActiveCounts[slotIndex];
    if (currentCount >= 128)
        return; // Too many copies of This sound playing. Drop it.

    This->m_sfxInstanceData[slotIndex][currentCount] = 0;
    This->m_sfxActiveCounts[slotIndex]++;
}

void SoundManager::playSoundWithPan(float xOffsetFromCenter, int soundId)
{
    printf("Playing sound %d at offset %f\n", soundId, xOffsetFromCenter);

    SoundManager* This = &g_soundManager;

    // Get the buffer index (WAV file ID) from the config table
    short bufferIndex = g_soundConfigTable[soundId].bufferIndex;

    // Calculate Stereo Panning
    // Map the Game World X (-192 to +192) to DirectSound Pan (-1000 to +1000)
    // 192.0f is the half-width of the gameplay field.
    int panValue = static_cast<int>((xOffsetFromCenter * 1000.0f) / 192.0f);
    int slotIndex = 0;
    int* pSlotId = This->m_sfxSlots;

    while (true)
    {
        // Found an empty slot (-1)
        if (*pSlotId < 0)
        {
            if (slotIndex < 12)
            {
                // Claim slot
                This->m_sfxSlots[slotIndex] = soundId;

                // Store Physical Buffer ID
                This->m_soundBufferIndices[soundId] = (int)bufferIndex;

                // Store the Calculated Pan Value
                // This array stores the "instance data" (Pan) for the active sound
                This->m_sfxInstanceData[slotIndex][0] = panValue;
                This->m_sfxActiveCounts[slotIndex]++;
            }
            return;
        }

        // Sound is already playing in this slot, add polyphonic instance
        if (*pSlotId == soundId)
            break;

        pSlotId++;
        slotIndex++;
        if (slotIndex >= 12)
            return;
    }

    // Check Polyphony limit (128 instances)
    if (This->m_sfxActiveCounts[slotIndex] >= 128)
        return;

    // Add new instance with the calculated Pan
    This->m_sfxInstanceData[slotIndex][This->m_sfxActiveCounts[slotIndex]] = panValue;
    This->m_sfxActiveCounts[slotIndex]++;
}

#if 0
HRESULT initCStreamingSound(CStreamingSound** destPtr, IDirectSound8** lplpdsound8, GUID guid3D,
    int dwDsBufferSize, HANDLE bgmUpdateCallback, ThBgmFormat* thBgmFormat)
{
    // 0x459ec1: Check for valid DSound interface
    if (*lplpdsound8 == nullptr) {
        return 0x800401F0; // DSERR_UNINITIALIZED
    }

    // 0x459f28: Allocate CWaveFile
    CWaveFile* pWaveFile = (CWaveFile*)game_new(sizeof(CWaveFile));
    if (pWaveFile == nullptr) {
        // Assembly implies a crash or bad state if null, but standard behavior is OOM
        return E_OUTOFMEMORY;
    }

    // 0x459f41: Manual initialization of CWaveFile members
    memset(pWaveFile, 0, sizeof(CWaveFile));
    pWaveFile->dataSize = 1;

    // 0x459f66: Open "thbgm.dat"
    HRESULT hr = pWaveFile->openWavFile(thBgmFormat, "thbgm.dat");
    if (FAILED(hr)) {
        // 0x459f6f: Cleanup logic on failure
        if (pWaveFile->dataSize == 1 && pWaveFile->fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pWaveFile->fileHandle);
            pWaveFile->fileHandle = INVALID_HANDLE_VALUE;
        }
        game_free(pWaveFile);
        return 0x80004005; // E_FAIL (Assembly returns -0x7FFF'BFFB)
    }

    // 0x459f9f: Setup DSBUFFERDESC
    DSBUFFERDESC dsbd;
    memset(&dsbd, 0, sizeof(dsbd));
    dsbd.dwSize = sizeof(DSBUFFERDESC);

    // Flags 0x18188: 
    // DSBCAPS_LOCSOFTWARE (0x10000) | DSBCAPS_GLOBALFOCUS (0x8000) | 
    // DSBCAPS_CTRLPOSITIONNOTIFY (0x100) | DSBCAPS_CTRLVOLUME (0x80) | 
    // DSBCAPS_GETCURRENTPOSITION2 (0x10000 - wait, flag overlap? 
    // 0x18188 usually implies GETCURRENTPOSITION2 (0x10000) | GLOBALFOCUS (0x8000) | ...
    // Note: LOCSOFTWARE is 0x8. 0x18188 = 0x10000 + 0x8000 + 0x100 + 0x80 + 0x8.
    dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS |
        DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE;

    dsbd.dwBufferBytes = dwDsBufferSize * 16;
    dsbd.lpwfxFormat = (WAVEFORMATEX*)&pWaveFile->thBgmFormat->format;
    dsbd.guid3DAlgorithm = guid3D;

    IDirectSoundBuffer* pDSBuffer = nullptr;
    IDirectSoundNotify* pDSNotify = nullptr;

    // 0x459f7c: CreateSoundBuffer
    hr = (*lplpdsound8)->CreateSoundBuffer(&dsbd, &pDSBuffer, nullptr);
    if (FAILED(hr)) {
        game_free(pWaveFile);
        return 0x80004005;
    }

    // 0x459f8d: QueryInterface for IDirectSoundNotify
    // Assembly queries pDSBuffer for IID_IDirectSoundNotify (g_dsoundGuid)
    hr = pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (void**)&pDSNotify);
    if (FAILED(hr)) {
        pDSBuffer->Release();
        game_free(pWaveFile);
        return 0x80004005;
    }

    // 0x459fb0: Allocate notification positions (16 entries)
    // 16 * 8 bytes = 128 bytes (0x80)
    DSBPOSITIONNOTIFY* pNotifies = (DSBPOSITIONNOTIFY*)game_new(16 * sizeof(DSBPOSITIONNOTIFY));
    if (pNotifies == nullptr) {
        pDSNotify->Release();
        pDSBuffer->Release();
        game_free(pWaveFile);
        return 0x80004005;
    }

    // 0x45a090: Fill notification positions
    // Triggers at the end of every `dwDsBufferSize` chunk
    for (int i = 0; i < 16; ++i) {
        pNotifies[i].dwOffset = (dwDsBufferSize - 1) + (i * dwDsBufferSize);
        pNotifies[i].hEventNotify = bgmUpdateCallback;
    }

    // 0x45a0ac: SetNotificationPositions
    hr = pDSNotify->SetNotificationPositions(16, pNotifies);

    // 0x45a0d1: Cleanup notifications and interface
    game_free(pNotifies);
    pDSNotify->Release(); // Interface released after setting positions

    if (FAILED(hr)) {
        pDSBuffer->Release();
        game_free(pWaveFile);
        return 0x8007000E; // E_OUTOFMEMORY (Specific code from assembly 45a075)
    }

    // 0x45a0fc: Create CStreamingSound
    CStreamingSound* pStreamSound = (CStreamingSound*)game_new(sizeof(CStreamingSound));
    if (pStreamSound == nullptr) {
        pDSBuffer->Release();
        game_free(pWaveFile);
        return 0x80004005;
    }

    // 0x45a11f: Call Constructor
    // Using placement new assuming the user has the constructor available/linked
    new (pStreamSound) CStreamingSound(pDSBuffer, dwDsBufferSize * 16, pWaveFile, dwDsBufferSize);

    // 0x45a12d: Copy DSBUFFERDESC into the new object
    pStreamSound->csound.dsBufferDesc = dsbd;

    // 0x45a13e: Set remaining members
    pStreamSound->dsoundIface = lplpdsound8;
    pStreamSound->bgmUpdateCallback = bgmUpdateCallback;
    pStreamSound->soundFlag = 0;

    // 0x45a128: Assign to output pointer
    *destPtr = pStreamSound;

    return S_OK;
}
#endif