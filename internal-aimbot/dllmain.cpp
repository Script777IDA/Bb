#include <Windows.h>
#include "hooks.h"
#include "aimbot.h"
#include "webview_bridge.h"

bool g_Running = true;

void Init() {
    Hooks::InitDX11Hook();
    
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        while (g_Running) {
            RunSilentAim();
            Sleep(1);
        }
        return 0;
    }, nullptr, 0, nullptr);
    
    WebViewBridge::Init(GetModuleHandle(NULL), SW_SHOW);
    WebViewBridge::RunMessageLoop();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Init();
            return 0;
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}
