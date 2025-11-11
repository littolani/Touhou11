#pragma once
#define WIN32_LEAN_AND_MEAN // Exclude uncommon headers
#define NOMINMAX            // Remove min and max macros from windef.h

// Windows Header Files
#include <windows.h>
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
