/** @file render/vr.h  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_RENDER_VR_H
#define CLIENT_RENDER_VR_H

#include "de/Vector"

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


class RiftState {
public:
    RiftState();

    // Use screen size instead of resolution in case non-square pixels?
    float aspect() const {return 0.5f * hScreenSize() / vScreenSize();}
    const de::Vector4f& chromAbParam() const {return m_chromAbParam;}
    float distortionScale() const;
    float fovX() const; // in degrees
    float fovY() const; // in degrees
    const de::Vector4f& hmdWarpParam() const {return m_hmdWarpParam;}
    float hScreenSize() const {return m_screenSize[0];}
    float lensSeparationDistance() const {return m_lensSeparationDistance;}
    bool loadRiftParameters();
    const de::Vector2f& screenSize() const {return m_screenSize;}
    float vScreenSize() const {return m_screenSize[1];}

private:
    de::Vector2f m_screenSize;
    float m_lensSeparationDistance;
    de::Vector4f m_hmdWarpParam;
    de::Vector4f m_chromAbParam;
    float m_eyeToScreenDistance;
};

extern RiftState riftState;

// Console variables
Stereo3DMode mode(); ///< Currently active Stereo3DMode index
bool modeNeedsStereoGLFormat(Stereo3DMode mode);
float riftFovX(); ///< Horizontal field of view in Oculus Rift in degrees
float riftLatency(); ///< Estimated head-motion->photons latency, in seconds

extern float ipd; ///< Interpupillary distance in meters
extern float playerHeight; ///< Human player's real world height in meters
extern float dominantEye; ///< Kludge for aim-down-weapon-sight modes
extern byte  swapEyes; ///< When true, inverts stereoscopic effect

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

// Called to allow head orientation to change again.
void allowHeadOrientationUpdate();

void updateHeadOrientation();

// Returns current pitch, roll, yaw angles, in radians. If no head tracking is available,
// the returned values are not valid.
de::Vector3f getHeadOrientation();

// To release memory and resources when done, for tidiness.
void deleteOculusTracker();

void setRiftLatency(float latency);

// Load Oculus Rift parameters via Rift SDK
bool loadRiftParameters();

} // namespace VR

#endif // CLIENT_RENDER_VR_H
