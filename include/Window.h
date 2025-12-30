#pragma once

#include "Chireiden.h"
#include "Macros.h"

class Window
{
public:
    HWND hwnd;
    DWORD timeForCleanup;
    HINSTANCE hInstance;
};