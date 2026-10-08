#include "hooks.h"
#include "renderer.h"
#include "webview_bridge.h"
#include <Windows.h>
#include <dxgi.h>
#include <stdio.h>
#include "MinHook.h" // Требуется MinHook

namespace Hooks {
    Present_t oPresent = nullptr;
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pContext = nullptr;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (!g_pd3dDevice) {
            pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
            g_pd3dDevice->GetImmediateContext(&g_pContext);
            
            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
        
        g_pContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        
        RenderFovCircle(g_pContext, nullptr, WebViewBridge::g_UIState.fov, 
                        WebViewBridge::g_UIState.fovColorInner, 
                        WebViewBridge::g_UIState.fovColorOuter);
                        
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    void InitDX11Hook() {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"DX", NULL, NULL };
        RegisterClassEx(&wc);
        HWND hWnd = CreateWindow(wc.lpszClassName, L"DX", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* pSwapChain;
        ID3D11Device* pDevice;
        ID3D11DeviceContext* pContext;
        
        D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, NULL, &pContext);

        void** pVTable = *reinterpret_cast<void***>(pSwapChain);
        oPresent = (Present_t)pVTable[8];

        MH_Initialize();
        MH_CreateHook(oPresent, hkPresent, (void**)&oPresent);
        MH_EnableHook(MH_ALL_HOOKS);

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
        DestroyWindow(hWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

    void DestroyDX11Hook() {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
}
