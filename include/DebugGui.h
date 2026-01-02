#pragma once
#include "Chireiden.h"
#include "Globals.h"
#include "Window.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <vector>
#include <string>

class ImGuiConsole
{
    std::vector<std::string> m_items;
    bool m_scrolledToBottom = true;

public:
    void log(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
        m_items.push_back(buf);
        if (m_items.size() > 500)
            m_items.erase(m_items.begin()); // Cap history
        m_scrolledToBottom = true;
    }

    friend class ImGuiHook;
private:
    void draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        // Copy Button
        if (ImGui::Button("Copy to Clipboard"))
        {
            ImGui::LogToClipboard();
            for (const auto& item : m_items) ImGui::LogText("%s\n", item.c_str());
            ImGui::LogFinish();
        }
        ImGui::Separator();

        // Scrollable region
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild(
            "ScrollingRegion",
            ImVec2(0, -footer_height_to_reserve),
            false,
            ImGuiWindowFlags_HorizontalScrollbar
        );

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        for (const auto& item : m_items)
            ImGui::TextUnformatted(item.c_str());

        if (m_scrolledToBottom)
            ImGui::SetScrollHereY(1.0f);
        m_scrolledToBottom = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::End();
    }
};
extern ImGuiConsole g_console;

class ImGuiHook
{
public:
    static ImGuiHook& getInstance()
    {
        static ImGuiHook instance;
        return instance;
    }

    static void SetupRobustRenderState(IDirect3DDevice9* device, ImDrawData* draw_data)
    {
        // 1. Setup Viewport to match the window
        D3DVIEWPORT9 vp;
        vp.X = vp.Y = 0;
        vp.Width = (DWORD)draw_data->DisplaySize.x;
        vp.Height = (DWORD)draw_data->DisplaySize.y;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        device->SetViewport(&vp);

        // 2. CRITICAL: Disable Programmable Shaders
        // This is what fixes the pixelation/glitching in Touhou
        device->SetPixelShader(NULL);
        device->SetVertexShader(NULL);

        // 3. Force Fixed-Function State
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE); // Draw on top of everything
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);

        // 4. Reset Texture Stages (Touhou messes with these heavily)
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

        // Ensure texture transforms are off (Touhou uses these for scrolling backgrounds)
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

        // 5. Setup Orthographic Projection for UI
        float L = draw_data->DisplayPos.x + 0.5f;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
        float T = draw_data->DisplayPos.y + 0.5f;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;

        // Standard Identity Matrix
        D3DMATRIX mat_identity;
        memset(&mat_identity, 0, sizeof(mat_identity));
        mat_identity.m[0][0] = 1.0f; mat_identity.m[1][1] = 1.0f;
        mat_identity.m[2][2] = 1.0f; mat_identity.m[3][3] = 1.0f;

        // Ortho Matrix
        D3DMATRIX mat_projection;
        memset(&mat_projection, 0, sizeof(mat_projection));
        mat_projection.m[0][0] = 2.0f / (R - L);
        mat_projection.m[1][1] = 2.0f / (T - B);
        mat_projection.m[2][2] = 0.5f;
        mat_projection.m[3][3] = 1.0f;
        mat_projection.m[3][0] = (L + R) / (L - R);
        mat_projection.m[3][1] = (T + B) / (B - T);
        mat_projection.m[3][2] = 0.5f;

        device->SetTransform(D3DTS_WORLD, &mat_identity);
        device->SetTransform(D3DTS_VIEW, &mat_identity);
        device->SetTransform(D3DTS_PROJECTION, &mat_projection);
    }

    static HRESULT STDMETHODCALLTYPE hookedEndScene(IDirect3DDevice9* pDevice)
    {
        static bool init = false;
        if (!init) {
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(g_window.hwnd);
            ImGui_ImplDX9_Init(pDevice);
            init = true;
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static bool showConsole = true;
        ImGuiViewport* imGuiviewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(imGuiviewport->Pos.x, imGuiviewport->Pos.y + imGuiviewport->Size.y - 250), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(imGuiviewport->Size.x, 250), ImGuiCond_FirstUseEver);
        g_console.draw("Reimu's Debug Log", &showConsole);

        ImGui::EndFrame();
        ImGui::Render();
        
        IDirect3DStateBlock9* stateBlock = nullptr;

        // 1. Capture Game State (Shaders, Transforms, RenderStates)
        if (pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock) == D3D_OK)
        {
            stateBlock->Capture();

            // 2. Apply our "Nuclear Option" state setup
            ImDrawData* drawData = ImGui::GetDrawData();
            SetupRobustRenderState(pDevice, drawData);

            // 3. Render ImGui using the standard backend logic (or custom)
            // If you are using standard ImGui_ImplDX9_RenderDrawData, you might need 
            // to modify it to NOT try to set state again, OR just rely on SetupRobustRenderState
            // running immediately before the draw calls.
            ImGui_ImplDX9_RenderDrawData(drawData);

            // 4. Restore Game State
            stateBlock->Apply();
            stateBlock->Release();
        }

        return originalEndScene(pDevice);
    }

    // DX9 requires calling Invalidate and Create device objects when the device is Reset
    // If not, ImGui will crash the game when resizing the window
    // because it is holding onto old textures/buffers that are no longer valid.
    static HRESULT STDMETHODCALLTYPE hookedReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
    {
        // Tell ImGui to dump its old fonts/textures before the device resets
        ImGui_ImplDX9_InvalidateDeviceObjects();

        // Call the original Reset
        HRESULT hr = originalReset(pDevice, pPresentationParameters);

        // Re-create ImGui assets after the device has recovered
        ImGui_ImplDX9_CreateDeviceObjects();

        return hr;
    }

    void hook(IDirect3DDevice9* device)
    {
        void** vtable = *(void***)device;
        originalEndScene = (EndScene)vtable[42];

        DWORD oldProtect;
        VirtualProtect(&vtable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        vtable[42] = &hookedEndScene;
        VirtualProtect(&vtable[42], sizeof(void*), oldProtect, &oldProtect);

        originalReset = (Reset)vtable[16];
        VirtualProtect(&vtable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        vtable[16] = &hookedReset;
        VirtualProtect(&vtable[16], sizeof(void*), oldProtect, &oldProtect);
    }

    ImGuiHook(const ImGuiHook&) = delete;
    ImGuiHook& operator=(const ImGuiHook&) = delete;
private:
    ImGuiHook() {}
    ~ImGuiHook() {}

    typedef HRESULT(STDMETHODCALLTYPE* EndScene)(IDirect3DDevice9*);
    typedef HRESULT(STDMETHODCALLTYPE* Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    static EndScene originalEndScene;
    static Reset originalReset;
};