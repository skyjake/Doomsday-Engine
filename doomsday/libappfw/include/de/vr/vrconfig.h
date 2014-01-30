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

    de::OculusRift &oculusRift();
    de::OculusRift const &oculusRift() const;

    /**
     * Currently active stereo rendering mode.
     */
    StereoMode mode() const;

    bool needsStereoGLFormat() const;

    void setEyeHeightInMapUnits(float eyeHeightInMapUnits);

    enum Eye {
        NeitherEye,
        LeftEye,
        RightEye
    };

    void setCurrentEye(Eye eye);

    /// Local viewpoint relative eye position in map units.
    float eyeShift() const;

    void enableFrustumShift(bool enable = true);

    /**
     * Determines if frustum shift is enabled.
     */
    bool frustumShift() const;

public:
    static bool modeNeedsStereoGLFormat(StereoMode mode);

private:
    DENG2_PRIVATE(d)

public:
    int vrMode;
    float ipd; ///< Interpupillary distance in meters
    float playerHeight; ///< Human player's real world height in meters
    float dominantEye; ///< Kludge for aim-down-weapon-sight modes
    bool swapEyes; ///< When true, inverts stereoscopic effect

    float hudDistance; // Distance from player character to screen, in map units (not used in Rift mode, because it's used by frustum shift)
    float weaponDistance; // (UNUSED) Distance from player character to weapon sprite, in map units

    int riftFramebufferSamples; // Multisampling used in unwarped Rift framebuffer
};

} // namespace de

#endif // LIBAPPFW_VRCONFIG_H
