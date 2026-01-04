#pragma once

#include "Chireiden.h"
#include "Macros.h"

class SoundManager
{
public:

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
};