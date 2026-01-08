#pragma once
#include "Chireiden.h"

struct RenderVertex144
{
    D3DXVECTOR3 pos;
    float rhw;
    D3DCOLOR diffuse;
    D3DXVECTOR2 uv;
};
ASSERT_SIZE(RenderVertex144, 0x1c);

struct RenderVertexSq
{
    D3DXVECTOR3 position;
    D3DXVECTOR2 uv;
};
ASSERT_SIZE(RenderVertexSq, 0x14);