#pragma once
#include "Chireiden.h"

struct Timer
{
public:
    //Timer();
    int m_previous;
    int m_current;
    float m_currentF;
    float* m_gameSpeed;
    uint32_t m_isInitialized;

    static void addf(Timer* This, float amount);
    static void setCurrent(Timer* This, int time);
    static void increment(Timer* This);
    static void set(Timer* This, int time);
};
