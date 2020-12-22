/** @file oculusrift.h
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

#ifndef LIBAPPFW_OCULUSRIFT_H
#define LIBAPPFW_OCULUSRIFT_H

#include "libgui.h"
#include <de/vector.h>
#include <de/matrix.h>

namespace de {

/**
 * Oculus Rift configuration and head tracking.
 *
 * @ingroup vr
 */
class LIBGUI_PUBLIC OculusRift
{
public:
    enum Eye { LeftEye, RightEye };
    enum Screen { DefaultScreen, PreviousScreen, HMDScreen };

public:
    OculusRift();

    /**
     * Checks if the Oculus Rift functionality is available. This returns
     * @c true if LibOVR is enabled in the build.
     */
    bool isEnabled() const;

    void glPreInit();

    /**
     * Checks if a HMD is connected at the moment.
     */
    bool isHMDConnected() const;

    /**
     * Initialize Oculus Rift rendering. The main window must exist and be visible.
     * This should be called whenever the Oculus Rift rendering mode is enabled.
     */
    void init();

    void deinit();

    void beginFrame();

    void endFrame();

    /**
     * Sets the currently rendered eye. LibOVR will decide which eye is left and which
     * is right.
     *
     * @param index  Eye index (0 or 1).
     */
    void setCurrentEye(int index);

    Eye currentEye() const;

    /**
     * Checks if Oculus Rift is connected and can report head orientation.
     */
    bool isReady() const;

    /**
     * Applies an offset to the yaw angle returned from Oculus Rift. This can be used to
     * calibrate where the zero angle's physical direction is.
     *
     * @param yawRadians  Offset to apply to the yaw angle (radians).
     */
    void setYawOffset(float yawRadians);

    void resetTracking();

    /**
     * Sets a yaw offset that makes the current actual Oculus Rift yaw come out as zero.
     */
    void resetYaw();

    // Called to allow head orientation to change again.
    //void allowUpdate();

    //void update();

    /**
     * Returns the current head orientation as a vector containing the pitch, roll and
     * yaw angles, in radians. If no head tracking is available, the returned values are
     * not valid.
     */
    Vec3f headOrientation() const;

    /**
     * Returns the current real-world head position.
     */
    Vec3f headPosition() const;

    Vec3f eyeOffset() const;

    /**
     * Returns the eye pose for the current eye, including the head orientation and
     * position in relation to the tracking origin.
     *
     * @return Eye pose matrix.
     */
    Mat4f eyePose() const;

    Mat4f projection(float nearDist, float farDist) const;

    float yawOffset() const;

    /**
     * Returns a model-view matrix that applies the head's orientation.
     */
    //Mat4f headModelViewMatrix() const;

    //float predictionLatency() const;

    /**
     * Returns the IPD configured in the Oculus Rift preferences.
     */
    //float interpupillaryDistance() const;

    float aspect() const;

    Vec2ui resolution() const;

    /*Vec2f screenSize() const;
    Vec4f chromAbParam() const;
    float distortionScale() const;*/
    float fovX() const; // in degrees
    //float fovY() const; // in degrees
    //Vec4f hmdWarpParam() const;
    //float lensSeparationDistance() const;

    void moveWindowToScreen(Screen screen);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_OCULUSRIFT_H
