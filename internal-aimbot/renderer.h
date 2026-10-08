#pragma once
#include <d3d11.h>
extern int g_ScreenWidth;
extern int g_ScreenHeight;
void RenderFovCircle(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps, float fov, float* innerColor, float* outerColor);