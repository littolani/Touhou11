/**
 * @file DistortionMesh.h
 * This structure renders an animated ripple or wave-bulge distortion effect 
 * over a textured background using a 17x17 grid of vertices.
*/

#pragma once
#include "AnmVm.h"
#include "Macros.h"
#include "Chain.h"

class AnmVm;

struct DistortionMesh
{
    int m_numRows;               // <0x00>
    int m_numColumns;            // <0x04>
    AnmId* m_anmIds;             // <0x08> array[numRows-1] of AnmId (int VM IDs)
    AnmVm** m_anmVmStrips;       // <0x0c> array[numRows-1] of AnmVm* (VM pointers for per-strip animation)
    RenderVertex144* m_meshData; // <0x10> flat array[numRows * numColumns] (full vertices)
    D3DXVECTOR3* m_positions;    // <0x14> flat array[numRows * numColumns] (positions)

    DistortionMesh(int rows, int columns, BOOL mode);
    ~DistortionMesh();
    static void setupSpecialRenderData(AnmId* anmId, int numColumns);
    static void setupSpecialRenderDataAlt(AnmId* anmId, int numColumns);
    //ChainCallbackResult onDraw(DistortionMesh* This);
};
ASSERT_SIZE(DistortionMesh, 0x18);

