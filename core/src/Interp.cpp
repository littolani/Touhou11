#include "Interp.h"

template <typename T>
void Interp<T>::step(Interp<T>* This)
{
    if (This->endTime > 0)
    {
        Timer::increment(&This->timer);

        if (This->endTime <= This->timer.current)
        {
            This->timer->set(This->endTime);
            This->m_endTime = 0;

            if (This->method != InterpMethod::CubicHermite)
                return This->goal;
        }
    }

    switch (This->method)
    {
        case InterpMethod::Linear:
            // "goal" acts as "speed" here.
            This->initial += This->goal;

        case InterpMethod::Physics:
            // initial = Position
            // bezier2 = Velocity
            // goal    = Acceleration

            This->initial += This->bezier2; // Pos += Vel
            This->bezier2 += This->goal;    // Vel += Accel

        case InterpMethod::CubicHermite:
        {
            float t = This->timer.currentF / (float)This->endTime;
            float t2 = t * t;
            float t3 = t2 * t;
            float h00 = (2.0f * t3) - (3.0f * t2) + 1.0f; // H00 = 2t^3 - 3t^2 + 1
            float h10 = t3 - (2.0f * t2) + t;             // H10 = t^3 - 2t^2 + t
            float h01 = (-2.0f * t3) + (3.0f * t2);       // H01 = -2t^3 + 3t^2
            float h11 = t3 - t2;                          // H11 = t^3 - t^2

            T term1 = h00 * This->initial; // P0
            T term2 = h10 * This->bezier1; // M0
            T term3 = h01 * This->goal;    // P1
            T term4 = h11 * This->bezier2; // M1
            This->initial = term1 + term2 + term3 + term4;
        }

        default:
        {
            float alpha = interpolate(This);
            T delta = This->goal - This->initial;
            T offset = delta * alpha;
            This->initial += offset;
        }
    }
}

template <typename T>
float Interp<T>::interpolate(Interp<T>* This)
{
    float t = m_timer.m_currentF / m_endTime;

    switch(m_method)
    {
    case InterpMethod::In2:
        return pow2(t);
    case InterpMethod::In3:
        return pow3(t);
    case InterpMethod::In4:
        return pow4(t);
    case InterpMethod::Out2:
        return 1.0f - pow2(1.0f - t);
    case InterpMethod::Out3:
        return 1.0f - pow3(1.0f - t);
    case InterpMethod::Out4:
        return 1.0f - pow4(1.0f - t);
    case InterpMethod::InOut2:
        t *= 2.0f;
        if (t < 1.0f) return pow2(t) / 2.0f;
        else return (2.0f - pow2(2.0f - t)) / 2.0f;
    case InterpMethod::InOut3:
        t *= 2.0f;
        if (t < 1.0f) return pow3(t) / 2.0f;
        else return (2.0f - pow3(2.0f - t)) / 2.0f;
    case InterpMethod::InOut4:
        t *= 2.0f;
        if (t < 1.0f) return pow4(t) / 2.0f;
        else return (2.0f - pow4(2.0f - t)) / 2.0f;
    case InterpMethod::OutIn2:
        t *= 2.0f;
        if (t < 1.0f) return 0.5f - pow2(1.0f - t) / 2.0f;
        else return 0.5f + pow2(t - 1.0f) / 2.0f;
    case InterpMethod::OutIn3:
        t *= 2.0f;
        if (t < 1.0f) return 0.5f - pow3(1.0f - t) / 2.0f;
        else return 0.5f + pow3(t - 1.0f) / 2.0f;
    case InterpMethod::OutIn4:
        t *= 2.0f;
        if (t < 1.0f) return 0.5f - pow4(1.0f - t) / 2.0f;
        else return 0.5f + pow4(t - 1.0f) / 2.0f;
    case InterpMethod::Delayed:
        return 0.0f;
    case InterpMethod::Instant:
        return 1.0f;
    default:
        return t;
    }
}