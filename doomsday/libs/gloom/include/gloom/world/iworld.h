/** @file iworld.h
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

#pragma once

#include <de/atlastexture.h>
#include <de/matrix.h>
#include <de/list.h>
#include "gloom/render/icamera.h"

namespace gloom {

using namespace de;

class User;

class LIBGLOOM_PUBLIC IWorld : public de::Deletable
{
public:
    IWorld();
    virtual ~IWorld();

    virtual void setLocalUser(User *) = 0;

    virtual void glInit();
    virtual void glDeinit();
    virtual void update(TimeSpan elapsed);
    virtual void render(const ICamera &camera);

    struct POI {
        Vec3f position;
        float yaw;

        POI(const Vec3f &pos, float yawAngle = 0)
            : position(pos)
            , yaw(yawAngle)
        {}
    };

    virtual User *    localUser() const = 0;
    virtual POI       initialViewPosition() const;
    virtual List<POI> pointsOfInterest() const;

    virtual double groundSurfaceHeight(const Vec3d &posMeters) const;
    virtual double ceilingHeight(const Vec3d &posMeters) const;

    DE_CAST_METHODS()

public:
    DE_DEFINE_AUDIENCE(Ready, void worldReady(IWorld &))
};

} // namespace gloom
