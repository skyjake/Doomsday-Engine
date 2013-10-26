#ifndef CLIENT_RENDER_VR_H
#define CLIENT_RENDER_VR_H

// TODO - move stereo stuff to its own class
enum Stereo3DMode {
    MODE3D_MONO = 0,
    MODE3D_GREEN_MAGENTA,
    MODE3D_SIDE_BY_SIDE,
    MODE3D_MAX_3D_MODE
};

// User configurable console variables //

// Which stereo 3D mode?
extern int vr_mode;

// Interpupillary distance in meters, e.g. 0.0622 for CMB
extern float vr_ipd;

// Your actual height, in meters, to help scale the world just so.
extern float vr_player_height;

// -1: left eye dominant; +1: right eye dominant; 0: middle
// So player can look down gun sight with one eye.
extern float vr_dominant_eye;

// To reverse the stereo effect
extern byte vr_swap_eyes;


// Variables below are global, but not user visible //

// Unlike most 3D modes, Oculus Rift typically uses no frustum shift.
// (or if we did, it would be different and complicated)
extern bool vr_apply_frustum_shift;

// local viewpoint relative eye position in map units,
// vr_eyeshift is ordinarily set from VR_SetEyeView()
extern float vr_eyeshift;

extern float vr_viewheight; // in map units

extern float vr_hud_distance; // in map units
extern float vr_weapon_distance; // in map units

// -1 means left eye, +1 means right eye
float VR_GetEyeShift(float eye);
// Register console variables
void VR_Register();

#endif // DDENG_VR_H
