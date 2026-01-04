#pragma once
#define NOMINMAX            // Remove min and max macros from windef.h
#define DIRECTINPUT_VERSION 0x0800
//#define IMGUI_DEBUG_WINDOW

// Windows Header Files
#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <shobjidl.h>
#include <objbase.h>
#include <shlguid.h>
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9math.h>
#include <dinput.h>
#include <d3d9caps.h>
#include <timeapi.h>
#include <winnls32.h>
#include <dsound.h>

// C RunTime Header Files
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tchar.h>

// C++ STL Header Files
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// Utilities
#include "Macros.h"