/** @file vrconfig.h  Virtual reality configuration.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_VRCONFIG_H
#define LIBAPPFW_VRCONFIG_H

#include <de/oculusrift.h>
#include <de/matrix.h>

namespace de {

/**
 * Virtual reality configuration settings.
 *
 * @ingroup vr
 */
class LIBGUI_PUBLIC VRConfig
{
public:
    /**
     * Stereoscopic 3D rendering mode. This enumeration determines the integer value in
     * the console variable.
     */
    enum StereoMode
    {
        Mono, // 0
        GreenMagenta,
        RedCyan,
        LeftOnly,
        RightOnly,
        TopBottom, // 5
        SideBySide,
        Parallel,
        CrossEye,
        OculusRift,
        RowInterleaved, // 10   // NOT IMPLEMENTED YET
        ColumnInterleaved,      // NOT IMPLEMENTED YET
        Checkerboard,           // NOT IMPLEMENTED YET
        QuadBuffered,
        NUM_STEREO_MODES
    };

    enum Eye
    {
        NeitherEye,
        LeftEye,
        RightEye
    };

public:
    VRConfig();

    /**
     * Sets the current stereoscopic rendering mode.
     *
     * @param newMode  Rendering mode.
     */
    void setMode(StereoMode newMode);

    /**
     * Sets the distance from the eye to the screen onto which projection is being
     * done. This used when calculating a frustum-shifted projection matrix.
     * This is not used with Oculus Rift.
     *
     * @param distance  Distance.
     */
    void setScreenDistance(float distance);

    /**
     * Sets the height of the eye in map units. This is used to determine how big an
     * eye shift is needed.
     *
     * @param eyeHeightInMapUnits  Height of the eye in map units, measured from the
     *                             "ground".
     */
    void setEyeHeightInMapUnits(float eyeHeightInMapUnits);

    /**
     * Sets the currently used physical IPD. This is used to determine how big an
     * eye shift is needed.
     *
     * @param ipd  IPD in mm.
     */
    void setInterpupillaryDistance(float ipd);

    /**
     * Sets the height of the player in the real world. This is used as a scaling
     * factor to convert physical units to map units.
     *
     * @param heightInMeters  Height of the player in meters.
     */
    void setPhysicalPlayerHeight(float heightInMeters);

    /**
     * Sets the eye currently used for rendering a frame. In stereoscopic modes,
     * the frame is drawn twice; once for each eye.
     *
     * @param eye  Eye to render. In non-stereoscopic modes, NeitherEye is used.
     */
    void setCurrentEye(Eye eye);

    Eye currentEye() const;

    /**
     * Enables or disables projection frustum shifting.
     *
     * @param enable @c true to enable.
     */
    void enableFrustumShift(bool enable = true);

    /**
     * Sets the number of multisampling samples used in the offscreen framebuffer
     * where Oculus Rift frames are first drawn. This framebuffer is typically some
     * multiple of the Oculus Rift display resolution.
     *
     * @param samples  Number of samples to use for multisampling the Oculus Rift
     *                 framebuffer.
     */
    void setRiftFramebufferSampleCount(int samples);

    /**
     * Sets the eyes-swapped mode.
     *
     * @param swapped  @c true to swap left and right (default: false).
     */
    void setSwapEyes(bool swapped);

    void setDominantEye(float value);

    /**
     * Currently active stereo rendering mode.
     */
    StereoMode mode() const;

    float screenDistance() const;

    /**
     * Determines if the current stereoscopic rendering mode needs support from
     * the graphics hardware for quad-buffering (left/right back and front
     * buffers stored and drawn separately).
     */
    bool needsStereoGLFormat() const;

    float interpupillaryDistance() const;

    float eyeHeightInMapUnits() const;

    float mapUnitsPerMeter() const;

    float physicalPlayerHeight() const;

    /**
     * Local viewpoint relative eye position in map units.
     */
    float eyeShift() const;

    /**
     * Determines if frustum shift is enabled.
     */
    bool frustumShift() const;

    bool swapEyes() const;

    float dominantEye() const;

    /// Multisampling used in unwarped Rift framebuffer.
    int riftFramebufferSampleCount() const;

    float viewAspect(const Vec2f &viewPortSize) const;

    /**
     * Calculates a vertical field of view angle based on a horizontal FOV angle,
     * view port size, and the current VR configuration.
     *
     * @param horizFovDegrees  Field of view horizontally, in degrees.
     * @param viewPortSize     Size of the viewport in pixels.
     *
     * @return  Vertical field of view, in degrees.
     */
    float verticalFieldOfView(float horizFovDegrees, const Vec2f &viewPortSize) const;

    /**
     * Produces a projection matrix suitable for the current VR configuration.
     *
     * @param fovDegrees    Horizontal field of view angle.
     * @param viewPortSize  Size of the viewport in pixels.
     * @param nearClip      Distance of the near clipping plane.
     * @param farClip       Distance of the far clipping plane.
     *
     * @return Projection matrix.
     */
    Mat4f projectionMatrix(float fovDegrees,
                              const Vec2f &viewPortSize,
                              float nearClip, float farClip) const;

    de::OculusRift &oculusRift();
    const de::OculusRift &oculusRift() const;

public:
    /**
     * Determines if the VR mode will be applying a transformation to window contents
     * that displaces the content from its "actual" location.
     *
     * @param mode  Mode.
     *
     * @return @c true, if contents will change position on screen.
     */
    static bool modeAppliesDisplacement(StereoMode mode);

    static bool modeNeedsStereoGLFormat(StereoMode mode);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VRCONFIG_H
