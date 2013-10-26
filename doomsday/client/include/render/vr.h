#ifndef CLIENT_RENDER_VR_H
#define CLIENT_RENDER_VR_H

namespace VR {

// The order shown here determines the integer value in the console.
// TODO - are symbolic values possible in the console?
enum Stereo3DMode {
    MODE_MONO = 0,
    MODE_GREEN_MAGENTA,
    MODE_RED_CYAN,
    MODE_LEFT,
    MODE_RIGHT,
    MODE_TOP_BOTTOM, // 5
    MODE_SIDE_BY_SIDE,
    MODE_PARALLEL,
    MODE_CROSSEYE,
    MODE_OCULUS_RIFT,
    MODE_ROW_INTERLEAVED, // 10
    MODE_COLUMN_INTERLEAVED,
    MODE_CHECKERBOARD,
    MODE_QUAD_BUFFERED,
    //
    MODE_MAX_3D_MODE_PLUS_ONE
};

// Console variables

// Console variables
extern int  mode;
// Interpupillary distance in meters
extern float ipd;
extern float playerHeight;
extern float dominantEye;
extern byte  swapEyes;

// Variables below are global, but not user visible //

// Unlike most 3D modes, Oculus Rift typically uses no frustum shift.
// (or if we did, it would be different and complicated)
extern bool applyFrustumShift;

// local viewpoint relative eye position in map units,
// vr::eyeshift is ordinarily set from vr::setEyeView()
extern float eyeShift;

extern float hudDistance; // in map units
extern float weaponDistance; // in map units

// -1 means left eye, +1 means right eye
float getEyeShift(float eye);
// Register console variables
void consoleRegister();

} // namespace VR

#endif // CLIENT_RENDER_VR_H
