#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "hooks.h"
#include "renderer.h"
#include "aimbot.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

bool g_Running = true;
int g_ScreenWidth = 1920;
int g_ScreenHeight = 1080;

void Init() {
    Natives::Init();
    Hooks::InitDX11Hook();
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        while (g_Running) {
            RunSilentAim();
            Sleep(1);
        }
        return 0;
    }, nullptr, 0, nullptr);
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