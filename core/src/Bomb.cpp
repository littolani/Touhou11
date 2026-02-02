#include "AnmVm.h"
#include "AnmManager.h"
#include "Bomb.h"
#include "Player.h"
#include "Shottypes.h"
#include "SoundManager.h"

ChainCallbackResult Bomb::onTick(Bomb* This)
{
    puts("bomb on tick\n");
    if (This->isUsingBomb == FALSE)
        return Continue;

    int result = 0;
    switch (g_globals.character)
    {
    case CharacterId::Reimu:
        switch (g_globals.subshot)
        {
        case SubshotId::TypeA:
            result = onTickReimuA(This);
            break;
        case SubshotId::TypeB:
            result = onTickReimuB(This);
            break;
        case SubshotId::TypeC:
            result = onTickReimuC(This);
            break;
        }
        break;

    case CharacterId::Marisa:
        switch (g_globals.subshot)
        {
        case SubshotId::TypeA:
            result = onTickMarisaA(This);
            break;
        case SubshotId::TypeB:
            result = onTickMarisaB(This);
            break;
        case SubshotId::TypeC:
            result = onTickMarisaC(This);
            break;
        }
        break;
    }

    // If the bomb tick function returns non-zero, the bomb sequence has finished.
    if (result != 0)
    {
        This->isUsingBomb = FALSE;
        return Continue;
    }

    // Increment the bomb timer.
    // This runs if the bomb is active (isUsingBomb != 0) and did not finish this tick.
    Timer::increment(&This->timer0);
    return Continue;
}

int Bomb::onTickReimuA(Bomb* This)
{
    AnmVm* vm = g_anmManager->getVmWithId(g_anmManager, (This->vmId).id);
    if (!vm)
    {
        This->vmId.id = 0;
        g_player->vm0.loadIntoAnmVm(&g_player->vm0, g_player->playerAnm, 0);
        g_player->someFlag &= 0xfffffffd;
        g_player->attemptedVelocityInternal.x = 0;
        g_player->attemptedVelocityInternal.y = 0;
        g_player->setIframes(0x28);
        return -1;
    }

    int currentTime = This->timer0.m_current;
    if (currentTime > 179)
    {
        //if (currentTime == 180)
        //{
        //    g_soundManager.playSoundCentered(0x26);
        //    g_soundManager.stopBombReimuAB();
        //    g_player->createDamageSource(g_player, &This->playerPos, 32.0, 10.0, 0x1e, 0x32);
        //}
        //This->sub_0045df10(This);
        //(This->someVec2).x = (This->someVec2).y + (This->someVec2).x;
    }
    return 0;
}

int Bomb::onTickReimuB(Bomb* This)
{
    return 0;
}

int Bomb::onTickReimuC(Bomb* This)
{
    return 0;
}

int Bomb::onTickMarisaA(Bomb* This)
{
    return 0;
}

int Bomb::onTickMarisaB(Bomb* This)
{
    return 0;
}

int Bomb::onTickMarisaC(Bomb* This)
{
    return 0;
}