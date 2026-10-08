#pragma once
#include <d3d11.h>

namespace Hooks {
    void InitDX11Hook();
    void DestroyDX11Hook();
    
    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    extern Present_t oPresent;
}
