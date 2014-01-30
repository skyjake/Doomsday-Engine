/** @file oculusrift.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_OCULUSRIFT_H
#define LIBAPPFW_OCULUSRIFT_H

#include "../libappfw.h"
#include <de/Vector>

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

    // True if Oculus Rift is enabled and can report head orientation.
    bool isReady() const;

    void setRiftLatency(float latency);

    // Called to allow head orientation to change again.
    void allowUpdate();

    void update();

    // Returns current pitch, roll, yaw angles, in radians. If no head tracking is available,
    // the returned values are not valid.
    Vector3f headOrientation() const;

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

public:
    float riftLatency; /// @todo refactor away
};

} // namespace de

#endif // LIBAPPFW_OCULUSRIFT_H
