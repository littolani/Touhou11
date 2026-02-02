#pragma once
#include "AnmVm.h"
#include "Chireiden.h"
#include "Macros.h"

struct Entity
{
    float idk;
    D3DXVECTOR3 position;
    float width;
    float height;
    float depth;
};

class StdVm
{
public:
    AnmVm tripletVms[3];
    AnmVm octetVms[8];

    /**
     * 0x403950
     * @brief
     * @param  camera         EAX:4
     * @param  vec            EDX:4
     * @param  cullDistanceSq Stack[0x4]:4
     * @param  entity         ECX
     */
    static BOOL checkEntityVisibility(Camera* camera, D3DXVECTOR3* vec, float cullDistanceSq, Entity* entity);
};