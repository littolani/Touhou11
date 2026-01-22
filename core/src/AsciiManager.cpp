#include "AsciiManager.h"

void AsciiManager::loadAsciiStrings(AsciiManager* This, const char* string, D3DXVECTOR3* position)
{
    if (This->m_numStrings >= STRING_ARRAY_SIZE)
        return;

    AsciiString* asciiString = &This->m_strings[This->m_numStrings++];
    strcpy_s(asciiString->text, string);
    asciiString->pos = *position;
    asciiString->color = This->m_color;
    asciiString->scale = This->scale;
    asciiString->alignH = This->alignH;
    asciiString->fontId = This->fontId;
    asciiString->drawShadows = This->drawShadows;
    asciiString->renderGroup = This->renderGroup;
    asciiString->remainingTime = This->duration;
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
    vm = g_anmManager->allocateVm();
    vm->m_flagsLow |= 0x40000000;
    vm->m_layer = vmLayer;
    vm->m_entityPos.x = spawnPosition->x;
    vm->m_entityPos.y = spawnPosition->y;
    vm->m_entityPos.z = spawnPosition->z;
    g_anmManager->loadIntoAnmVm(vm, anmLoaded, scriptNumber);
    g_anmManager->putInVmList(g_anmManager, vm, outAnmId);
    g_supervisor.leaveCriticalSection(9);
}

