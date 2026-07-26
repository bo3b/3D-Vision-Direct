#include "Overlay.h"

#include "Utils.h"
#include "Globals.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <Windows.h>
#include <cfloat>

//--------------------------------------------------------------------------------------
// ImGui debug overlay.
//
// Rendered on the game thread (from copy_to_handoff), NOT the presenter thread- the
// presenter is deliberately stateless (Copy + Present only) so it can run concurrent
// with our draws under SetMultithreadProtected. ImGui's stateful draw sequence would
// corrupt that, so it lives here.
//
// We draw into the scene source array (g_LR_tex) BEFORE it is copied to the handoff,
// so the single CopyResource carries scene+overlay together as one atomic write. If
// we drew into the handoff instead, the presenter's copy could interleave mid-draw
// (or the scene copy could wipe us), which shows up as a flickering overlay. Both eye
// slices get the same draw data- zero parallax, so it reads at screen depth. Note
// io.Framerate here measures the game rate.
//--------------------------------------------------------------------------------------

Overlay::Overlay(HWND game_window, ID3D11Device* game_device, ID3D11DeviceContext* game_immediate_context, ID3D11Texture2D* scene_LR)
{
    context_ = game_immediate_context;

    // One single-slice RTV per eye, so the overlay can be drawn into both layers
    // of the scene array (scene_LR is ArraySize=2).
    D3D11_TEXTURE2D_DESC td;
    scene_LR->GetDesc(&td);
    for (UINT slice = 0; slice < 2; slice++)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc  = {};
        rtv_desc.Format                         = td.Format;
        rtv_desc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtv_desc.Texture2DArray.MipSlice        = 0;
        rtv_desc.Texture2DArray.FirstArraySlice = slice;
        rtv_desc.Texture2DArray.ArraySize       = 1;
        HR(game_device->CreateRenderTargetView(scene_LR, &rtv_desc, &eyeRTV_[slice]));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // no imgui.ini for a debug overlay
    ImGui::StyleColorsDark();

    // We render into a 4K buffer, so the default overlay is tiny. FontScaleMain
    // scales the text (crisply, via 1.92's dynamic font rasterization) and
    // ScaleAllSizes scales the padding/spacing/borders to match.
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(2.1f);
    style.FontScaleMain = 2.1f;
    ImGui_ImplWin32_Init(game_window);
    ImGui_ImplDX11_Init(game_device, game_immediate_context);
}

Overlay::~Overlay()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for each (ID3D11RenderTargetView* rtv in eyeRTV_)
        rtv->Release();
}

// Draw the overlay into both eyes of the scene array (pre-copy). The same draw data
// goes into each slice- zero parallax, so it reads at screen depth in both eyes.
void Overlay::Render()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // FPS on left
    ImGuiIO& io = ImGui::GetIO();


    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    {
        ImGui::Text("Game: %.0f FPS  (%.1f ms)", io.Framerate, 1000.0f / io.Framerate);

        // Push RAW instantaneous FPS into the history ring. io.Framerate smooths over ~60 frames
        const float fps_now            = (io.DeltaTime > 0.0f) ? 1.0f / io.DeltaTime : 0.0f;
        fps_history_[fps_history_idx_] = fps_now;
        fps_history_idx_               = (fps_history_idx_ + 1) % kFpsHistorySize;
        // Scale 0..60 so the y-range tracks the actual data.  Values_offset makes this a ring buffer.
        ImGui::PlotLines("##fps", fps_history_, kFpsHistorySize, fps_history_idx_, nullptr, 0.0f, 120.0f, ImVec2(400, 100));
        ImGui::Text("Rows: %i", g_cube_rows);
        ImGui::Text("Cube Load: %i", g_cube_iterations);
    }
    ImGui::End();

    // Dropped frames and timing on right
    ImGui::SetNextWindowPos(ImVec2(2000, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Dropped", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    {
        ImGui::Text("Dropped: %i", g_DroppedFrames);
        ImGui::Text("Iterations: %i", g_load_iterations);
        ImGui::Text("Iterations: %i", g_stall_iterations);
        // Scale 0..20 ms
        const float load_now             = g_LoadTimer->LastMs();
        load_history_[load_history_idx_] = load_now;
        load_history_idx_                = (load_history_idx_ + 1) % kFpsHistorySize;

        // Peak-hold of what's currently in the ring, to confirm spikes are being sampled
        // even if a single-frame needle isn't visually obvious on the graph.
        float peak = 0.0f;
        for each (float v in load_history_)
            if (v > peak) peak = v;

        ImGui::Text("Load ms: %4.1f  peak %.1f", load_now, peak);
        ImGui::PlotLines("##ms", load_history_, kFpsHistorySize, load_history_idx_, nullptr, 0.0f, 20.0f, ImVec2(400, 100));
    }
    ImGui::End();

    ImGui::Render();

    for each (ID3D11RenderTargetView* rtv in eyeRTV_)
    {
        ID3D11RenderTargetView* rtv_array[] = { rtv };
        context_->OMSetRenderTargets(1, rtv_array, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}
