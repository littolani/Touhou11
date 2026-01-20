#include "Bullet.h"
#include "Globals.h"

void Bullet::step_ex_00_speedup(Bullet* This)
{
    if (This->exSpeedup.timer.m_current < 17)
    {
        projectMagnitudeToVectorComponents(
            &This->velocity,
            This->angle,
            This->speed + (5.0 - This->exSpeedup.timer.m_currentF * 0.3125)
        );
    }
    else
        This->active_ex_flags = This->active_ex_flags ^ 1;

    Timer::increment(&This->exSpeedup.timer);
}

void Bullet::step_ex_02_accel(Bullet* This)
{
    if (This->exAccel.timer.m_current < This->exAccel.duration)
    {
        This->speed = This->exAccel.accelNorm * g_gameSpeed + This->speed;
        This->velocity.x = This->velocity.x + g_gameSpeed * This->exAccel.accelVector.x;
        This->velocity.y = This->velocity.y + g_gameSpeed * This->exAccel.accelVector.y;
        This->velocity.z = This->velocity.z + g_gameSpeed * This->exAccel.accelVector.z;

        if (std::fabs(This->velocity.x) > 0.0001 || std::fabs(This->velocity.y) > 0.0001)
            This->angle = std::atan2(This->velocity.y, This->velocity.x);
    }
    else
        This->active_ex_flags = This->active_ex_flags & 0xfffffffb;

    Timer::increment(&(This->exAccel).timer);
}
    