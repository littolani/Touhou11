#include "AsciiManager.h"

void AsciiManager::loadAsciiStrings(const char* str, D3DXVECTOR3* position)
{
    if (m_numStrings >= STRING_ARRAY_SIZE)
        return;

    AsciiString* asciiString = &m_strings[m_numStrings++];
    strcpy_s(asciiString->text, str);
    asciiString->pos = *position;
    asciiString->color = m_color;
    asciiString->scale = scale;
    asciiString->alignH = alignH;
    asciiString->fontId = fontId;
    asciiString->drawShadows = drawShadows;
    asciiString->renderGroup = renderGroup;
    asciiString->remainingTime = duration;
}

void AsciiManager::spawnUnknownEffect(float x, float y)
{
    D3DXVECTOR3 spawnPosition;
    spawnPosition.x = x;
    spawnPosition.y = y;
    spawnPosition.z = 0.0;

    if (this->anmId == 0)
    {
        AnmId id;
        spawnAnm(this->asciiAnm, &id, 16, 22, &spawnPosition);
        this->anmId = id.id;
    }
}


void AsciiManager::spawnAnm(AnmLoaded* anmLoaded, AnmId* outAnmId, int scriptNumber, int vmLayer, D3DXVECTOR3* spawnPosition)
{
    AnmVm* vm;
    g_supervisor.enterCriticalSection(9);
    vm = g_anmManager->allocateVm(g_anmManager);
    vm->m_flagsLow |= 0x40000000;
    vm->m_layer = vmLayer;
    vm->m_entityPos.x = spawnPosition->x;
    vm->m_entityPos.y = spawnPosition->y;
    vm->m_entityPos.z = spawnPosition->z;
    g_anmManager->loadIntoAnmVm(vm, anmLoaded, scriptNumber);
    g_anmManager->putInVmList(g_anmManager, vm, outAnmId);
    g_supervisor.leaveCriticalSection(9);
}

