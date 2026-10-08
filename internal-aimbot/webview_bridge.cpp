#include "webview_bridge.h"
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <string>
#include <iostream>

#pragma comment(lib, "WebView2LoaderStatic.lib")

using namespace Microsoft::WRL;

namespace WebViewBridge {
    UIState g_UIState;
    
    HWND g_hWnd = nullptr;
    ComPtr<ICoreWebView2Controller> g_webviewController;
    ComPtr<ICoreWebView2> g_webview;

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_SIZE) {
            if (g_webviewController) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                g_webviewController->put_Bounds(bounds);
            }
            return 0;
        }
        if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void ParseAndUpdateUIState(const std::wstring& json) {
        if (json.find(L"fov") != std::wstring::npos) {
            size_t pos = json.find(L"value");
            if (pos != std::wstring::npos) {
                g_UIState.fov = std::stof(json.substr(pos + 8));
            }
        }
        if (json.find(L"silentAim") != std::wstring::npos) {
            g_UIState.silentAim = json.find(L"true") != std::wstring::npos;
        }
        if (json.find(L"colorInner") != std::wstring::npos) {
            size_t pos = json.find(L"#");
            if (pos != std::wstring::npos) {
                std::wstring hex = json.substr(pos, 7);
                if (hex.length() == 7) {
                    int r = std::stoi(hex.substr(1, 2), nullptr, 16);
                    int g = std::stoi(hex.substr(3, 2), nullptr, 16);
                    int b = std::stoi(hex.substr(5, 2), nullptr, 16);
                    g_UIState.fovColorInner[0] = (float)r / 255.0f;
                    g_UIState.fovColorInner[1] = (float)g / 255.0f;
                    g_UIState.fovColorInner[2] = (float)b / 255.0f;
                }
            }
        }
    }

    bool Init(HINSTANCE hInstance, int nCmdShow) {
        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.lpszClassName = L"AltVModMenuOverlay";
        RegisterClassEx(&wcex);

        g_hWnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
            L"AltVModMenuOverlay", L"Mod Menu",
            WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            nullptr, nullptr, hInstance, nullptr
        );
        
        SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);

        CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    env->CreateCoreWebView2Controller(g_hWnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (controller) {
                                    g_webviewController = controller;
                                    g_webviewController->get_CoreWebView2(&g_webview);
                                    
                                    ICoreWebView2Settings* settings;
                                    g_webview->get_Settings(&settings);
                                    settings->put_IsScriptEnabled(TRUE);
                                    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                    settings->put_IsWebMessageEnabled(TRUE);
                                    
                                    g_webview->AddWebMessageReceived(
                                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                            [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                                LPWSTR message;
                                                args->get_WebMessageAsString(&message);
                                                ParseAndUpdateUIState(std::wstring(message));
                                                CoTaskMemFree(message);
                                                return S_OK;
                                            }).Get(), nullptr);
                                            
                                    g_webview->Navigate(L"menu.html"); 
                                }
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
                
        return true;
    }

    void RunMessageLoop() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void Destroy() {
        if (g_hWnd) DestroyWindow(g_hWnd);
    }
}
