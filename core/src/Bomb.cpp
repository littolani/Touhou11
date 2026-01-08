#include "Bomb.h"
#include "Shottypes.h"

ChainCallbackResult Bomb::onTick(Bomb* This)
{
    if (This->isUsingBomb == FALSE)
        return CHAIN_CALLBACK_RESULT_CONTINUE;

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
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    // Increment the bomb timer.
    // This runs if the bomb is active (isUsingBomb != 0) and did not finish this tick.
    Timer::increment(&This->timer0);
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

int Bomb::onTickReimuA(Bomb* This)
{
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