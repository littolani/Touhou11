#pragma once
#include "Chireiden.h"
#include "AnmManager.h"
#include "Timer.h"
#include "Macros.h"

struct Item
{
	AnmVm vm;
	D3DXVECTOR3 vec3;
	D3DXVECTOR2 vec2;
	int someInt_0x448;
	int someInt_0x44c;
	Timer timer;
	int someInt_0x464;
	int someInt_0x468;
	int someInt_0x46c;
	int shotFileRelated;
	int someInt_0x474;
};
ASSERT_SIZE(Item, 0x478);