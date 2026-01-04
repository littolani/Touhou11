#include "DistortionMesh.h"
#include "AnmManager.h"
#include "AnmLoaded.h"
#include "Supervisor.h"

#if 0
DistortionMesh::DistortionMesh(int numRows, int numColumns, BOOL mode)
{
    if (g_supervisor.surfaceR0 == nullptr)
    {
        m_numRows = 0;
        m_numColumns = 0;
        m_anmIds = nullptr;
        m_anmVmStrips = nullptr;
        m_meshData = nullptr;
        m_positions = nullptr;
        return;
    }

    m_numRows = numRows;
    m_numColumns = numColumns;
    int stripCount = numRows - 1;
    m_anmIds = new AnmId[stripCount];
    m_anmVmStrips = new AnmVm*[stripCount];
    m_meshData = new RenderVertex144[numRows * numColumns];
    m_positions = new D3DXVECTOR3[numRows * numColumns];

    void (*setupFunc)(AnmId*, int) = (mode == 0) ? setupSpecialRenderData : setupSpecialRenderDataAlt;

    if (stripCount > 0)
    {
        for (int j = 0; j < stripCount; ++j)
        {
            AnmId id;
            setupFunc(&id, numColumns);
            m_anmIds[j] = id;
            AnmVm* vm = g_anmManager->getVmWithId(g_anmManager, m_anmIds[j].id);
            if (vm == nullptr)
                m_anmIds[j].id = 0;

            m_anmVmStrips[j] = vm;
            if (vm != nullptr) 
            {
                vm->m_onDraw = (ChainCallback)0x40e520; // onDrawStub;  // 0x40e520
                vm->m_distortionMesh = this;
                vm->m_flagsLow &= 0xffffff8f;  // Clear bits 4-6
            }
        }
    }
}

// 0x42b3d0
void DistortionMesh::setupSpecialRenderData(AnmId* anmId, int numColumns)
{
    g_supervisor.someAnmLoaded->makeVm(anmId, 0x51, 0x1b);
    AnmVm* vm = g_anmManager->getVmWithId(*anmId);
    if (vm == nullptr)
    {
        anmId->id = 0;
        return;
    }

    RenderVertex144* renderData = new RenderVertex144[numColumns * 2];
    vm->m_specialRenderData = renderData;

    if (numColumns < 3)
        vm->m_flagsLow &= 0xfc3fffff;  // Clear bits 22-25

    else
    {
        vm->m_flagsLow = (vm->m_flagsLow & 0xff3fffff) | 0x03000000;  // Clear 22-23, set 24-25
        vm->m_intVars[0] = numColumns;

        for (int i = 0; i < numColumns * 2; ++i)
        {
            renderData[i].pos.z = 0.0f;
            renderData[i].rhw = 1.0f;
            renderData[i].diffuse = 0xffffffff;
        }
    }
}

// 0x42b480
// Identical structure to default, but with different makeVm params (script 0x52, sprite 0x1c).
void DistortionMesh::setupSpecialRenderDataAlt(AnmId* anmId, int numColumns)
{
    g_supervisor.someAnmLoaded->makeVm(anmId, 0x52, 0x1c);
    AnmVm* vm = g_anmManager->getVmWithId(*anmId);
    if (vm == nullptr)
    {
        anmId->id = 0;
        return;
    }

    RenderVertex144* renderData = new RenderVertex144[numColumns * 2];
    vm->m_specialRenderData = renderData;

    vm->m_flagsLow = (vm->m_flagsLow & 0xff3fffff) | 0x03000000;  // Clear 22-23, set 24-25
    vm->m_intVars[0] = numColumns;

    if (numColumns < 3)
        vm->m_flagsLow &= 0xfc3fffff;  // Clear bits 22-25

    else
    {
        for (int i = 0; i < numColumns * 2; ++i)
        {
            renderData[i].pos.z = 0.0f;
            renderData[i].rhw = 1.0f;
            renderData[i].diffuse = 0xffffffff;
        }
    }
}

DistortionMesh::~DistortionMesh()
{
    
}
#endif