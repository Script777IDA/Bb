#include "aimbot.h"
#include <math.h>

struct Settings {
    bool silentAim = true;
    float fov = 90.0f;
    float smoothness = 0.0f;
    int targetBone = 0x796E; // SKEL_Head
    float fovColorInner[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    float fovColorOuter[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
Settings g_Settings;

extern int g_ScreenWidth;
extern int g_ScreenHeight;

void RunSilentAim() {
    if (!g_Settings.silentAim) return;
    Ped localPlayer = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(localPlayer) || ENTITY::IS_ENTITY_DEAD(localPlayer, FALSE)) return;
    Vector3 camRot = CAM::GET_GAMEPLAY_CAM_ROT(2);
    Vector3 camPos = CAM::GET_GAMEPLAY_CAM_COORD();
    Vector2 screenCenter = { g_ScreenWidth / 2.0f, g_ScreenHeight / 2.0f };
    float fovPixels = g_Settings.fov;
    Entity bestTarget = 0;
    float bestDist = fovPixels * fovPixels;
    const int arrSize = 256;
    int peds[arrSize];
    int count = PED::GET_PED_NEARBY_PEDS(localPlayer, (int*)peds, -1);
    for (int i = 0; i < count; i++) {
        Entity ped = peds[i * 2 + 2];
        if (!ENTITY::DOES_ENTITY_EXIST(ped) || ENTITY::IS_ENTITY_DEAD(ped, FALSE)) continue;
        if (PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE)) continue;
        Vector3 headPos = PED::GET_PED_BONE_COORDS(ped, g_Settings.targetBone, 0, 0, 0);
        Vector2 screenPos;
        if (GRAPHICS::GET_SCREEN_COORD_FROM_WORLD_COORD(headPos.x, headPos.y, headPos.z, &screenPos.x, &screenPos.y)) {
            screenPos.x *= g_ScreenWidth; screenPos.y *= g_ScreenHeight;
            float dx = screenPos.x - screenCenter.x;
            float dy = screenPos.y - screenCenter.y;
            float dist = dx*dx + dy*dy;
            if (dist < bestDist) { bestDist = dist; bestTarget = ped; }
        }
    }
    if (bestTarget != 0) {
        Vector3 targetPos = PED::GET_PED_BONE_COORDS(bestTarget, g_Settings.targetBone, 0, 0, 0);
        Vector3 dir = { targetPos.x - camPos.x, targetPos.y - camPos.y, targetPos.z - camPos.z };
        float pitch = -atan2f(dir.z, sqrtf(dir.x*dir.x + dir.y*dir.y)) * (180.0f / 3.14159f);
        float yaw = atan2f(dir.y, dir.x) * (180.0f / 3.14159f) - 90.0f;
        float smooth = g_Settings.smoothness == 0 ? 1.0f : (10.0f - g_Settings.smoothness) / 10.0f;
        camRot.x += (pitch - camRot.x) * smooth;
        camRot.z += (yaw - camRot.z) * smooth;
        CAM::SET_GAMEPLAY_CAM_ROT_OVERRIDE(camRot.x, camRot.y, camRot.z, 2);
    }
}