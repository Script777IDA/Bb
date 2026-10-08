#pragma once
#include <Windows.h>
#include <string>

namespace WebViewBridge {
    bool Init(HINSTANCE hInstance, int nCmdShow);
    void RunMessageLoop();
    void Destroy();
    
    struct UIState {
        bool silentAim = true;
        float fov = 90.0f;
        float smoothness = 0.0f;
        int targetBone = 0x796E;
        float fovColorInner[4] = {1.0f, 1.0f, 1.0f, 0.5f};
        float fovColorOuter[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };
    
    extern UIState g_UIState;
}
