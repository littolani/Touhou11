#include "DebugGui.h"
#include "Supervisor.h"

ImGuiConsole g_console;
ImGuiHook::EndScene ImGuiHook::originalEndScene = nullptr;
ImGuiHook::Reset ImGuiHook::originalReset = nullptr;