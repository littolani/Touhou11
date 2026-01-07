#pragma once
#include "Chireiden.h"
#include "Chain.h"
#include "Item.h"
#include "Macros.h"

class ItemManager
{
	uint32_t flags;
	uint32_t idk1;
	ChainElem* onTick;
	ChainElem* onDraw;
	uint32_t idk2;
	Item normalItems[150];
	Item cancelItems[2048];
	int numItemsAlive;
	int nextCancelItemIndex;
	int numCancelledItemsSpawnedThisFrame;
};
ASSERT_SIZE(ItemManager, 0x265e70);