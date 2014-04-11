/** @file oculusrift.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../libappfw.h"
#include <de/Vector>
#include <de/Matrix>

namespace de {

/**
 * Oculus Rift configuration and head tracking.
 */
class LIBAPPFW_PUBLIC OculusRift
{
public:
    OculusRift();

    bool init();

    void deinit();

    /**
     * Checks if Oculus Rift is enabled and can report head orientation.
     */
    bool isReady() const;

    void setPredictionLatency(float latency);

    /**
     * Applies an offset to the yaw angle returned from Oculus Rift. This can be used to
     * calibrate where the zero angle's physical direction is.
     *
     * @param yawRadians  Offset to apply to the yaw angle (radians).
     */
    void setYawOffset(float yawRadians);

    /**
     * Sets a yaw offset that makes the current actual Oculus Rift yaw come out as zero.
     */
    void resetYaw();

    // Called to allow head orientation to change again.
    void allowUpdate();

    void update();

    /**
     * Returns the current head orientation as a vector containing the pitch, roll and
     * yaw angles, in radians. If no head tracking is available, the returned values are
     * not valid.
     */
    Vector3f headOrientation() const;      

    float yawOffset() const;

    /**
     * Returns a model-view matrix that applies the head's orientation.
     */
    Matrix4f headModelViewMatrix() const;

    float predictionLatency() const;

    /**
     * Returns the IPD configured in the Oculus Rift preferences.
     */
    float interpupillaryDistance() const;

    // Use screen size instead of resolution in case non-square pixels?
    float aspect() const;

    Vector2f screenSize() const;
    Vector4f chromAbParam() const;
    float distortionScale() const;
    float fovX() const; // in degrees
    float fovY() const; // in degrees
    Vector4f hmdWarpParam() const;
    float lensSeparationDistance() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_OCULUSRIFT_H
