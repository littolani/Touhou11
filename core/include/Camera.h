#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct Camera
{
    D3DXVECTOR3 offset;
    D3DXVECTOR3 target;
    D3DXVECTOR3 up;
    D3DXVECTOR3 v3;
    D3DXVECTOR3 callibration;
    D3DXVECTOR3 eye;
    float fovY;
    D3DXMATRIX viewMatrix;
    D3DXMATRIX projectionMatrix;
    D3DVIEWPORT9 viewport;
    uint32_t idk;
    float m_globalRenderQuadOffsetX;
    float m_globalRenderQuadOffsetY;
    D3DXVECTOR3 interpAmt;
    float fogEnd;
    float fogStart;
    float fogB;
    float fogG;
    float fogR;
    DWORD renderStateValue0;
    DWORD renderStateValue1;
};
ASSERT_SIZE(Camera, 0x118);