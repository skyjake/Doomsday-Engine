/** @file icamera.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef ICAMERA_H
#define ICAMERA_H

#include <de/matrix.h>
#include "../libgloom.h"

namespace gloom {

class LIBGLOOM_PUBLIC ICamera
{
public:
    virtual ~ICamera() = default;

    /**
     * Returns the position of the camera in world space.
     */
    virtual de::Vec3f cameraPosition() const = 0;

    virtual de::Vec3f cameraFront() const = 0;

    virtual de::Vec3f cameraUp() const = 0;

    virtual de::Mat4f cameraProjection() const = 0;

    virtual de::Mat4f cameraModelView() const = 0;

    virtual de::Mat4f cameraModelViewProjection() const {
        return cameraProjection() * cameraModelView();
    }
};

} // namespace gloom

#endif // ICAMERA_H
