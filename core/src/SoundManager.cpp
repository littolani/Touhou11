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

void SoundManager::playSoundCentered(int soundIndex)
{
    printf("Playing sound %d centered\n", soundIndex);
    SoundManager* This = &g_soundManager;
    int16_t bufferConfig = g_soundConfigTable[soundIndex].bufferIndex;

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
                This->m_sfxSlots[slotIndex] = soundIndex;
                This->m_soundBufferIndices[soundIndex] = bufferConfig;
                This->m_sfxInstanceData[slotIndex][0] = 0;
                This->m_sfxActiveCounts[slotIndex]++;
            }
            return;
        }

        // This sound is already active in This slot. We just add another instance (polyphony).
        if (currentSlotId == soundIndex)
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