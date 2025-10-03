#pragma once
#include "Chireiden.h"

struct Camera
{
    D3DXVECTOR3 offset;
    D3DXVECTOR3 target;
    D3DXVECTOR3 up;
    D3DXVECTOR3 v3;
    D3DXVECTOR3 v4;
    D3DXVECTOR3 eye;
    float fovY;
    D3DXMATRIX viewMatrix;
    D3DXMATRIX projectionMatrix;
    D3DVIEWPORT9 viewport;
    uint32_t mystery[6];
    float f0;
    float f1;
    uint32_t g[5];
};
ASSERT_SIZE(Camera, 0x118);