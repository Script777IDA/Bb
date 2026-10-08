#pragma once
#include <Windows.h>

struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };
typedef int Entity;
typedef int Ped;

namespace Natives { void Init(); }
namespace PLAYER { int PLAYER_PED_ID(); }
namespace ENTITY { bool DOES_ENTITY_EXIST(Entity); bool IS_ENTITY_DEAD(Entity, BOOL); Vector3 GET_ENTITY_COORDS(Entity, BOOL); }
namespace PED { bool IS_PED_IN_ANY_VEHICLE(Ped, BOOL); int GET_PED_NEARBY_PEDS(Ped, int*, int); Vector3 GET_PED_BONE_COORDS(Ped, int, float, float, float); }
namespace CAM { Vector3 GET_GAMEPLAY_CAM_ROT(int); Vector3 GET_GAMEPLAY_CAM_COORD(); void SET_GAMEPLAY_CAM_ROT_OVERRIDE(float, float, float, int); }
namespace GRAPHICS { bool GET_SCREEN_COORD_FROM_WORLD_COORD(float, float, float, float*, float*); }

void RunSilentAim();