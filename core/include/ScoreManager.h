#pragma once
#include "Chireiden.h"
#include "Macros.h"
#include "Score.h"

class ScoreManager
{
public:

    /**
	 * 0x437500
	 * @brief Used to validate the scoreth11.dat file
	 * @param  data        ECX:4
	 * @param  size        Stack[0x4]:4
	 * @return int         EAX:4
	 */
	static int calculateChecksum(uint8_t* data, int size);
};