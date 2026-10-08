#include "renderer.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string.h>
const char* g_FovPixelShaderSrc = R"(
cbuffer Constants : register(b0) {
    float2 center;
    float radius;
    float4 colorInner;
    float4 colorOuter;
};
float4 main(float4 pos : SV_POSITION) : SV_TARGET {
    float dist = distance(pos.xy, center);
    if (dist > radius) discard;
    float t = dist / radius;
    return lerp(colorInner, colorOuter, t);
}
)";
void RenderFovCircle(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps, float fov, float* innerColor, float* outerColor) {
    struct Constants { float center[2]; float radius; float padding; float colorInner[4]; float colorOuter[4]; } cb;
    cb.center[0] = g_ScreenWidth / 2.0f;
    cb.center[1] = g_ScreenHeight / 2.0f;
    cb.radius = fov;
    memcpy(cb.colorInner, innerColor, sizeof(float) * 4);
    memcpy(cb.colorOuter, outerColor, sizeof(float) * 4);
}