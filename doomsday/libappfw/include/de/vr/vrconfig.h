/** @file vrconfig.h  Virtual reality configuration.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VRCONFIG_H
#define LIBAPPFW_VRCONFIG_H

#include <de/OculusRift>

namespace de {

/**
 * Virtual reality configuration settings.
 */
class LIBAPPFW_PUBLIC VRConfig
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

public:
    VRConfig();

    void setMode(StereoMode newMode);

    void setScreenDistance(float distance);

    void setEyeHeightInMapUnits(float eyeHeightInMapUnits);

    /**
     * Sets the currently used IPD.
     *
     * @param ipd  IPD in mm.
     */
    void setInterpupillaryDistance(float ipd);

    void setPhysicalPlayerHeight(float heightInMeters);

    enum Eye {
        NeitherEye,
        LeftEye,
        RightEye
    };

    void setCurrentEye(Eye eye);

    void enableFrustumShift(bool enable = true);

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

    bool needsStereoGLFormat() const;

    float interpupillaryDistance() const;

    float physicalPlayerHeight() const;

    /// Local viewpoint relative eye position in map units.
    float eyeShift() const;

    /**
     * Determines if frustum shift is enabled.
     */
    bool frustumShift() const;

    bool swapEyes() const;

    float dominantEye() const;

    /// Multisampling used in unwarped Rift framebuffer.
    int riftFramebufferSampleCount() const;

    de::OculusRift &oculusRift();
    de::OculusRift const &oculusRift() const;

public:
    static bool modeNeedsStereoGLFormat(StereoMode mode);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VRCONFIG_H
