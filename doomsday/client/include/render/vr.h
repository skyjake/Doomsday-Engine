/** @file render/vr.h  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dd_types.h"

#include <de/Vector>

namespace de {

/**
 * Virtual reality configuration settings.
 */
class VRConfig
{
public:
    /**
     * Stereoscopic 3D rendering mode. This enumeration determines the integer value in
     * the console variable.
     */
    enum StereoMode
    {
        ModeMono, // 0
        ModeGreenMagenta,
        ModeRedCyan,
        ModeLeftOnly,
        ModeRightOnly,
        ModeTopBottom, // 5
        ModeSideBySide,
        ModeParallel,
        ModeCrossEye,
        ModeOculusRift,
        ModeRowInterleaved, // 10   // NOT IMPLEMENTED YET
        ModeColumnInterleaved,      // NOT IMPLEMENTED YET
        ModeCheckerboard,           // NOT IMPLEMENTED YET
        ModeQuadBuffered,
        NUM_STEREO_MODES
    };

    /**
     * State parameters for Oculus Rift.
     */
    class OculusRift {
    public:
        OculusRift();

        bool init();

        void deinit();

        void setRiftLatency(float latency);

        // True if Oculus Rift is enabled and can report head orientation.
        bool isReady() const;

        void update();

        // Called to allow head orientation to change again.
        void allowUpdate();

        // Returns current pitch, roll, yaw angles, in radians. If no head tracking is available,
        // the returned values are not valid.
        Vector3f headOrientation() const;

        float interpupillaryDistance() const;

        // Use screen size instead of resolution in case non-square pixels?
        float aspect() const { return 0.5f * _screenSize.x / _screenSize.y; }
        Vector2f const &screenSize() const { return _screenSize; }
        Vector4f const &chromAbParam() const { return _chromAbParam; }
        float distortionScale() const;
        float fovX() const; // in degrees
        float fovY() const; // in degrees
        Vector4f const &hmdWarpParam() const { return _hmdWarpParam; }
        float lensSeparationDistance() const { return _lensSeparationDistance; }

    private:
        DENG2_PRIVATE(d)

        Vector2f _screenSize;
        float _lensSeparationDistance;
        Vector4f _hmdWarpParam;
        Vector4f _chromAbParam;
        float _eyeToScreenDistance;

    public:
        float riftLatency;
    };

public:
    VRConfig();

    OculusRift &ovr();
    OculusRift const &ovr() const;

    /// Currently active stereo rendering mode.
    StereoMode mode() const;

    static bool modeNeedsStereoGLFormat(StereoMode mode);

    /// @param eye: -1 means left eye, +1 means right eye
    /// @return viewpoint eye shift in map units
    float getEyeShift(float eye) const;

    void setEyeHeightInMapUnits(float eyeHeightInMapUnits);

private:
    DENG2_PRIVATE(d)

public:
    int vrMode;
    float ipd; ///< Interpupillary distance in meters
    float playerHeight; ///< Human player's real world height in meters
    float dominantEye; ///< Kludge for aim-down-weapon-sight modes
    bool swapEyes; ///< When true, inverts stereoscopic effect

    // Unlike most 3D modes, Oculus Rift typically uses no frustum shift.
    // (or if we did, it would be different and complicated)
    bool applyFrustumShift;

    // local viewpoint relative eye position in map units,
    // VR::eyeShift is ordinarily set from VR::getEyeShift()
    float eyeShift;

    float hudDistance; // Distance from player character to screen, in map units (not used in Rift mode, because it's used by frustum shift)
    float weaponDistance; // (UNUSED) Distance from player character to weapon sprite, in map units

    int riftFramebufferSamples; // Multisampling used in unwarped Rift framebuffer
};

} // namespace de

extern de::VRConfig vrCfg;

namespace VR {

// Register console variables
void consoleRegister();

float riftFovX(); ///< Horizontal field of view in Oculus Rift in degrees

// To release memory and resources when done, for tidiness.
//void deleteOculusTracker();

// Load Oculus Rift parameters via Rift SDK
bool loadRiftParameters();

} // namespace VR

#endif // CLIENT_RENDER_VR_H
