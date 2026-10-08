#include <Windows.h>
#include <atlbase.h>

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")


#include <undocumented.h>
#include <vtablehook.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <mod_menu.hpp>

// controller test includes
#include "..\\controller\\runtime\\controller.hpp"
#include "..\\controller\\features\\esp.hpp"
#include <atomic>

using namespace ImGui;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

decltype(&PresentDWM) fn_PresentDWM = nullptr;
decltype(&PresentMultiplaneOverlay) fn_PresentMultiplaneOverlay = nullptr;

static CComPtr<ID3D11Device>            g_Device = nullptr;
static CComPtr<ID3D11DeviceContext>     g_DeviceContext = nullptr;
static CComPtr<ID3D11RenderTargetView>  g_RTV = nullptr;
static CComPtr<IDXGISwapChainDWMLegacy> g_SwapChain = nullptr;

static HHOOK g_hMouseHook = nullptr;
static HHOOK g_hKeyboardHook = nullptr;

static bool g_menu_open = true;
static std::atomic<bool> g_panic{ false };

static void draw(IDXGISwapChainDWMLegacy* pSwapChain);
static void PanicShutdown();

static void draw(IDXGISwapChainDWMLegacy* pSwapChain)
{
    if (g_panic.load()) return;

    if (!ImGui::GetCurrentContext())
    {
        HRESULT hr = pSwapChain->GetDevice(IID_PPV_ARGS(&g_Device));
        if (FAILED(hr)) { return; }
        g_Device->GetImmediateContext(&g_DeviceContext);

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(GetDesktopWindow());
        ImGui_ImplDX11_Init(g_Device, g_DeviceContext);

        CComPtr<ID3D11Texture2D> pBackBuffer = nullptr;
        hr = pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (FAILED(hr)) { return; }
        hr = g_Device->CreateRenderTargetView(pBackBuffer, nullptr, &g_RTV);
        if (FAILED(hr)) { return; }

        mod_menu::initialize();
    }

    // one-time controller init/start
    {
        static bool ctl_started = false;
        if (!ctl_started) {
            if (ctl::runtime::instance().init(L"GTA5.exe")) {
                ctl::runtime::instance().start();
                ctl_started = true;
            }
        }
    }

    g_DeviceContext->OMSetRenderTargets(1, &g_RTV.p, nullptr);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    {
        auto& io = ImGui::GetIO();
        if (g_menu_open) {
            CURSORINFO ci{ sizeof(ci) };
            bool sysVisible = (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING));
            io.MouseDrawCursor = !sysVisible;
            if (!sysVisible)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        } else {
            io.MouseDrawCursor = false;
        }
    }

    ImGui::NewFrame();

    mod_menu::render(g_menu_open);

    ImGui::Render();


    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

static HRESULT STDMETHODCALLTYPE hk_PresentDWM(
    IDXGISwapChainDWMLegacy* pSwapChain,
    UINT SyncInterval,
    UINT PresentFlags,
    UINT DirtyRectsCount,
    const RECT* pDirtyRects,
    UINT ScrollRectsCount,
    const RECT* pScrollRects,
    IDXGIResource* pResource,
    UINT FrameIndex)
{
    __try { draw(pSwapChain); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return fn_PresentDWM(pSwapChain, SyncInterval, PresentFlags, DirtyRectsCount, pDirtyRects, ScrollRectsCount, pScrollRects, pResource, FrameIndex);
}

static HRESULT STDMETHODCALLTYPE hk_PresentMultiplaneOverlay(
    IDXGISwapChainDWMLegacy* pSwapChain,
    UINT SyncInterval,
    UINT PresentFlags,
    enum DXGI_HDR_METADATA_TYPE MetadataType,
    const void* pMetadata,
    UINT OverlayCount,
    const struct _DXGI_PRESENT_MULTIPLANE_OVERLAY* pOverlays)
{
    __try { draw(pSwapChain); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return fn_PresentMultiplaneOverlay(pSwapChain, SyncInterval, PresentFlags, MetadataType, pMetadata, OverlayCount, pOverlays);
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT* hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    if (wParam == WM_KEYUP && hookStruct->vkCode == static_cast<DWORD>(VK_F7)) {
        PanicShutdown();
        return -1;
    }

    if (!g_menu_open) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_WndProcHandler(GetDesktopWindow(), static_cast<UINT>(wParam), hookStruct->vkCode, hookStruct->scanCode);
    if (io.WantCaptureKeyboard) return -1;
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) return CallNextHookEx(nullptr, nCode, wParam, lParam);
    if (g_panic.load()) return CallNextHookEx(nullptr, nCode, wParam, lParam);
    if (!g_menu_open) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    const MSLLHOOKSTRUCT* hookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    const ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplWin32_WndProcHandler(GetDesktopWindow(), static_cast<UINT>(wParam), hookStruct->flags, MAKELPARAM(hookStruct->pt.x, hookStruct->pt.y));
    if (io.WantCaptureMouse && wParam != WM_MOUSEMOVE) return -1;
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}


static DWORD WINAPI MainThread(LPVOID)
{
    // init and start runtime controller
    ctl::runtime::instance().init(L"GTA5.exe");
    ctl::runtime::instance().start();
    CComPtr<IDXGIFactory> pFactory = nullptr;
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { return EXIT_FAILURE; }

    CComPtr<IDXGIAdapter> pAdapter = nullptr;
    hr = pFactory->EnumAdapters(0, &pAdapter);
    if (FAILED(hr) || !pAdapter) { return EXIT_FAILURE; }

    CComPtr<IDXGIOutput> pOutput = nullptr;
    hr = pAdapter->EnumOutputs(0, &pOutput);
    if (FAILED(hr) || !pOutput) { return EXIT_FAILURE; }

    CComPtr<IDXGIFactoryDWM> pFactoryDWM = nullptr;
    hr = pFactory->QueryInterface(IID_PPV_ARGS(&pFactoryDWM));
    if (FAILED(hr)) { return EXIT_FAILURE; }

    const D3D_FEATURE_LEVEL FeatureLevels[] { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_11_0 };

    CComPtr<ID3D11Device> pDevice = nullptr;
    CComPtr<ID3D11DeviceContext> pDeviceContext = nullptr;
    hr = D3D11CreateDevice(
        pAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        FeatureLevels, ARRAYSIZE(FeatureLevels),
        D3D11_SDK_VERSION, &pDevice, nullptr, &pDeviceContext);
    if (FAILED(hr)) { return EXIT_FAILURE; }

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 1;

    hr = pFactoryDWM->CreateSwapChain(pDevice, &desc, pOutput, &g_SwapChain);
    if (FAILED(hr)) { return EXIT_FAILURE; }

    fn_PresentDWM = reinterpret_cast<decltype(fn_PresentDWM)>(vtable::hook(g_SwapChain, &hk_PresentDWM, 16));
    fn_PresentMultiplaneOverlay = reinterpret_cast<decltype(fn_PresentMultiplaneOverlay)>(vtable::hook(g_SwapChain, &hk_PresentMultiplaneOverlay, 23));

    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc, nullptr, 0);
    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, nullptr, 0);
    if (!g_hMouseHook || !g_hKeyboardHook) { return EXIT_FAILURE; }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);
    if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
    return EXIT_SUCCESS;
}

static void PanicShutdown()
{
    g_panic.store(true);

    ImGuiIO* pIo = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
    if (pIo) {
        pIo->MouseDrawCursor = false;
    }

    if (g_SwapChain) {
        __try {
            if (fn_PresentDWM) vtable::hook(g_SwapChain, fn_PresentDWM, 16);
            if (fn_PresentMultiplaneOverlay) vtable::hook(g_SwapChain, fn_PresentMultiplaneOverlay, 23);
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
    if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = nullptr; }

    __try {
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    if (g_DeviceContext) { g_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr); g_DeviceContext->ClearState(); g_DeviceContext->Flush(); }
    if (g_RTV) { g_RTV.Release(); }
    if (g_DeviceContext) { g_DeviceContext.Release(); }
    if (g_Device) { g_Device.Release(); }
    if (g_SwapChain) { g_SwapChain.Release(); }

    PostQuitMessage(0);
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD reason, LPVOID)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hInstance);
    
            wchar_t path[MAX_PATH]{};
            if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
                const wchar_t* name = wcsrchr(path, L'\\');
                name = name ? name + 1 : path;
                if (_wcsicmp(name, L"dwm.exe") != 0) {
                    return TRUE;
                }
            }
    
            HANDLE hThread = CreateThread(nullptr, 0, &MainThread, nullptr, 0, nullptr);
            if (hThread) {
                CloseHandle(hThread);
            }
            break;
        }
        
        case DLL_PROCESS_DETACH:
            if (g_hMouseHook) { UnhookWindowsHookEx(g_hMouseHook); g_hMouseHook = nullptr; }
            if (g_hKeyboardHook) { UnhookWindowsHookEx(g_hKeyboardHook); g_hKeyboardHook = nullptr; }
            break;
    }
    return TRUE;
}
