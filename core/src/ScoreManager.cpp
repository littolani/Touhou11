#include "ScoreManager.h"

int ScoreManager::calculateChecksum(uint8_t* data, int size)
{
    puts("Calculated score checksum\n");
    int sumOdd = 0;
    int sumEven = 0;
    int i = 0;

    if (size - 8 > 1)
    {
        do {
            sumOdd += data[8 + i];
            sumEven += data[9 + i];
            i += 2;
        } while (i < size - 9);
    }

    uint32_t lastByte = 0;
    if (i < size - 8)
        lastByte = data[8 + i];

    return sumEven + sumOdd + lastByte;
}