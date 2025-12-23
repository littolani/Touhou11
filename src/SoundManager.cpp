#include "SoundManager.h"

char* SoundManager::findRiffChunk(char* searchPtr, const char* targetId, int remainingSize, int* outChunkSize)
{
    //printf("findRiffChunk searching for %s at %p with remaining size %d\n", targetId, searchPtr, remainingSize);
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