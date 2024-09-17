// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "./ext/ImGui/imgui.h"
#include "./ext/ImGui/imgui_impl_win32.h"
#include "./ext/ImGui/imgui_impl_dx11.h"
#include "./ext/minhook/MinHook.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")


// 定义函数参数
typedef long(__stdcall* DIGX_Present)(IDXGISwapChain*, UINT, UINT);
DIGX_Present originPresent;
DIGX_Present old_present;

bool getPresentPtr()
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //sd.BufferDesc.RefreshRate.Numerator = 60;         跟随游戏的fps
    //sd.BufferDesc.RefreshRate.Denominator = 1;        跟随游戏
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   //跟随游戏窗口
    sd.OutputWindow = GetForegroundWindow();            //跟随游戏窗口
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    IDXGISwapChain* swap_chain;
    ID3D11Device* device;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(
        nullptr, 
        D3D_DRIVER_TYPE_HARDWARE, 
        nullptr, 
        createDeviceFlags, 
        featureLevelArray, 
        2, 
        D3D11_SDK_VERSION, 
        &sd, 
        &swap_chain,
        &device,
        &featureLevel, 
        nullptr) == S_OK) {
        void** p_vtable = *reinterpret_cast<void***>(swap_chain);   //得到虚函数表
        swap_chain->Release();      //释放
        device->Release();          //释放
        old_present = (DIGX_Present)p_vtable[8];    //从虚函数表得到present函数
        return true;
    }
    return false;
}

WNDPROC oWndProc;
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static long __stdcall myPresent(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
    if (!init) {
        //从swap chain获得device
        if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice)))
        {
            //获得上下文
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            
            //
            DXGI_SWAP_CHAIN_DESC sd;
            p_swap_chain->GetDesc(&sd);
            window = sd.OutputWindow;
            ID3D11Texture2D* pBackBuffer;
            p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            //创建imgui上下文
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

            ImGui::StyleColorsDark();

            ImGuiStyle& style = ImGui::GetStyle();
            if (io.ConfigFlags)
            {
                style.WindowRounding = 0.0f;
                style.Colors[ImGuiCol_WindowBg].w = 1.0f;
            }

            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
            init = true;
        }
        else
            return originPresent(p_swap_chain, sync_interval, flags);
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::EndFrame();
    ImGui::Render();
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    // Update and Render additional Platform Windows
    //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    //{
    //    ImGui::UpdatePlatformWindows();
    //    ImGui::RenderPlatformWindowsDefault();
    //}

    //// Present
    //HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
    ////HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
    //g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    return originPresent(p_swap_chain, sync_interval, flags);
}

//"main" loop
int WINAPI main(HMODULE hModule) {

    // 1.获得函数
    if (!getPresentPtr())
        return 1;

    // 2.开始hook
    //  2.1 hook初始化
    if (MH_Initialize() != MH_OK)
        return 1;
    //  2.2 创建hook
    if (MH_CreateHook(reinterpret_cast<void**>(old_present), &myPresent, reinterpret_cast<void**>(&originPresent)) != MH_OK)
        return 1;
    //  2.3 启用hook
    if (MH_EnableHook(old_present) != MH_OK)
        return 1;
    // 3.等待 F1 退出
    while (true) {
        Sleep(50);

        if (GetAsyncKeyState(VK_F1)) {
            break;
        }
    }
    // 4.退出hook，清理
        //Cleanup
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
        return 1;
    if (MH_Uninitialize() != MH_OK)
        return 1;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, hModule, 0, NULL);
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

