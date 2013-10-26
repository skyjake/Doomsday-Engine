#include "de_console.h"
#include "render/vr.h"

int vr_mode = 0;
// Interpupillary distance in meters
float vr_ipd = 0.0622f;
float vr_player_height = 1.70f;
float vr_dominant_eye = 0.0f;
bool vr_apply_frustum_shift = true;
float vr_eyeshift = 0;
byte vr_swap_eyes = 0;
float vr_viewheight = 41.0; // That's what it is in zdoom...
float vr_hud_distance = 30.0f;
float vr_weapon_distance = 10.0f;

// eye: -1 means left eye, +1 means right eye
// Returns viewpoint eye shift in map units
float VR_GetEyeShift(float eye) {
    // 0.95 because eyes are not at top of head
    float map_units_per_meter = vr_viewheight / ((0.95) * vr_player_height);
    float result = map_units_per_meter * (eye - vr_dominant_eye) * 0.5 * vr_ipd;
    if (vr_swap_eyes != 0)
        result *= -1;
    return result;
}

void VR_Register()
{
    C_VAR_FLOAT ("vr_ipd",              &vr_ipd,           0, 0.02f, 0.2f);
    C_VAR_FLOAT ("vr_player_height",    &vr_player_height, 0, 1.0f, 3.0f);
    C_VAR_FLOAT ("vr_dominant_eye",     &vr_dominant_eye,  0, -1.0f, 1.0f);
    C_VAR_BYTE  ("vr_swap_eyes",        &vr_swap_eyes,     0, 0, 1);
    C_VAR_INT   ("vr_mode",             &vr_mode,          0, 0, (int)(MODE3D_MAX_3D_MODE - 1));
}
