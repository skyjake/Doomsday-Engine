#include "de_console.h"
#include "render/vr.h"


// Console variables
int  VR::mode = 0;
// Interpupillary distance in meters
float VR::ipd = 0.0622f;
float VR::playerHeight = 1.70f;
float VR::dominantEye = 0.0f;
byte  VR::swapEyes = 0;

// Global variables
bool  VR::applyFrustumShift = true;
float  VR::eyeShift = 0;
float  VR::hudDistance = 30.0f;
float  VR::weaponDistance = 10.0f;

// eye: -1 means left eye, +1 means right eye
// Returns viewpoint eye shift in map units
float VR::getEyeShift(float eye)
{
    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = Con_GetInteger("player-eyeheight") / ((0.95) *  VR::playerHeight);
    float result = mapUnitsPerMeter * (eye -  VR::dominantEye) * 0.5 *  VR::ipd;
    if ( VR::swapEyes != 0)
        result *= -1;
    return result;
}

static void vrModeChanged()
{
    if(ClientWindow::hasMain())
    {
        // The logical UI size may need to be changed.
        ClientWindow::main().updateRootSize();
    }
}

void VR::consoleRegister()
{
    C_VAR_FLOAT ("rend-vr-ipd",              & VR::ipd,           0, 0.02f, 0.2f);
    C_VAR_FLOAT ("rend-vr-player-height",    & VR::playerHeight,  0, 1.0f, 3.0f);
    C_VAR_FLOAT ("rend-vr-dominant-eye",     & VR::dominantEye,   0, -1.0f, 1.0f);
    C_VAR_BYTE  ("rend-vr-swap-eyes",        & VR::swapEyes,      0, 0, 1);
    C_VAR_INT2  ("rend-vr-mode",             & VR::mode,          0, 0, (int)(VR::MODE_MAX_3D_MODE - 1), vrModeChanged);
}
