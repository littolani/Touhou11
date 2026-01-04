#include "DebugGui.h"
#include "Supervisor.h"

IDirect3DSwapChain9* g_pDebugSwapChain = NULL;
HWND g_hDebugWindow = NULL;
D3DPRESENT_PARAMETERS g_Debugd3dpp = {};

// Forward declare the ImGui WndProc
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI DebugWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        // Handle swap chain resize here if you want the window to be resizable
        return 0;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE); // Don't destroy, just hide
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void InitDebugWindow(IDirect3DDevice9* pDevice)
{
    // 1. Register Window Class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DebugWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "TH11_Debug", NULL };
    RegisterClassEx(&wc);

    // 2. Create the Window
    g_hDebugWindow = CreateWindowA("TH11_Debug", "Reimu's Debugger",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL);

    // 3. Setup Presentation Parameters for the Swap Chain
    ZeroMemory(&g_Debugd3dpp, sizeof(g_Debugd3dpp));
    g_Debugd3dpp.Windowed = TRUE;
    g_Debugd3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_Debugd3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Match device
    g_Debugd3dpp.EnableAutoDepthStencil = TRUE;
    g_Debugd3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_Debugd3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // No VSync for debugger

    // 4. Create the Swap Chain
    pDevice->CreateAdditionalSwapChain(&g_Debugd3dpp, &g_pDebugSwapChain);

    // 5. Initialize ImGui for this window
    // Note: We share the same D3D Device, but ImGui needs to know about the window
    // You might need to re-init ImGui_ImplWin32 with this new HWND, 
    // or manually forward events. 
    ImGui_ImplWin32_Init(g_hDebugWindow);
    ImGui_ImplDX9_Init(pDevice);

    ShowWindow(g_hDebugWindow, SW_SHOWDEFAULT);
    UpdateWindow(g_hDebugWindow);
}

void RenderDebugWindow()
{
    if (!g_pDebugSwapChain) return;

    // 1. Get the Back Buffer from our Debug Swap Chain
    IDirect3DSurface9* pBackBuffer = NULL;
    g_pDebugSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

    // 2. Save the game's current Render Target (so we don't break the game)
    IDirect3DSurface9* pOldRenderTarget = NULL;
    g_supervisor.d3dDevice->GetRenderTarget(0, &pOldRenderTarget);

    // 3. Set our Debug Window as the target
    g_supervisor.d3dDevice->SetRenderTarget(0, pBackBuffer);

    // Clear and Begin Scene
    g_supervisor.d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);
    g_supervisor.d3dDevice->BeginScene();

    // 4. Draw ImGui
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // --- DRAW YOUR DEBUG UI HERE ---
    ImGui::Begin("VM Inspector");
    ImGui::Text("AnmVm Count: %d", 5);
    ImGui::End();
    // -------------------------------

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    g_supervisor.d3dDevice->EndScene();

    // 5. Present the Swap Chain to the Debug Window
    g_pDebugSwapChain->Present(NULL, NULL, NULL, NULL, 0);

    // 6. Restore the Game's Render Target
    g_supervisor.d3dDevice->SetRenderTarget(0, pOldRenderTarget);

    // Cleanup refs
    if (pBackBuffer) pBackBuffer->Release();
    if (pOldRenderTarget) pOldRenderTarget->Release();
}

// --------
//ImGuiConsole g_console;
//ImGuiHook::EndScene ImGuiHook::originalEndScene = nullptr;
//ImGuiHook::Reset ImGuiHook::originalReset = nullptr;
//WNDPROC ImGuiHook::originalWndProc;
//HWND ImGuiHook::currentHwnd;