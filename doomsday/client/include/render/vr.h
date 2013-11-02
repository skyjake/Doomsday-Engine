#ifndef CLIENT_RENDER_VR_H
#define CLIENT_RENDER_VR_H

namespace VR {

/// Menu of stereoscopic 3D modes available. Oculus Rift is the star player, but there
/// are many other options.
/// The order shown here determines the integer value in the console.
/// @todo - Add an additional console command to support symbolic versions of these mode settings
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
    MODE_ROW_INTERLEAVED, // 10 // NOT IMPLEMENTED YET
    MODE_COLUMN_INTERLEAVED, // NOT IMPLEMENTED YET
    MODE_CHECKERBOARD, // NOT IMPLEMENTED YET
    MODE_QUAD_BUFFERED,
    //
    MODE_MAX_3D_MODE_PLUS_ONE
};

// Console variables
Stereo3DMode mode(); /// Currently active Stereo3DMode index
float riftAspect(); /// Aspect ratio of OculusRift
extern float ipd; /// Interpupillary distance in meters
extern float playerHeight; /// Human player's real world height in meters
extern float dominantEye; /// Kludge for aim-down-weapon-sight modes
extern byte  swapEyes; /// When true, inverts stereoscopic effect

// Variables below are global, but not user visible //

// Unlike most 3D modes, Oculus Rift typically uses no frustum shift.
// (or if we did, it would be different and complicated)
extern bool applyFrustumShift;

// local viewpoint relative eye position in map units,
// VR::eyeShift is ordinarily set from VR::getEyeShift()
extern float eyeShift;

extern float hudDistance; // Distance from player character to screen, in map units (not used in Rift mode, because it's used by frustum shift)
extern float weaponDistance; // (UNUSED) Distance from player character to weapon sprite, in map units

/// @param eye: -1 means left eye, +1 means right eye
/// @return viewpoint eye shift in map units
float getEyeShift(float eye);

// Register console variables
void consoleRegister();

// Head tracking API

// True if Oculus Rift is enabled and can report head orientation.
bool hasHeadOrientation();

// Returns current pitch, roll, yaw angles, in radians, or empty vector if head tracking is not available.
std::vector<float> getHeadOrientation();

// To release memory and resources when done, for tidiness.
void deleteOculusTracker();

} // namespace VR

#endif // CLIENT_RENDER_VR_H
