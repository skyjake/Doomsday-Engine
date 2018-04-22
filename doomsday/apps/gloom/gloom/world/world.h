/** @file world.h
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

#ifndef WORLD_H
#define WORLD_H

#include <de/AtlasTexture>
#include <de/Matrix>
#include "gloom/render/icamera.h"

#include <QList>

namespace gloom {

class User;

class World : public de::Deletable
{
public:
    World();
    virtual ~World();

    virtual void setLocalUser(User *) = 0;

    virtual void glInit();
    virtual void glDeinit();
    virtual void update(de::TimeSpan const &elapsed);
    virtual void render(ICamera const &camera);

    struct POI {
        de::Vec3f position;
        float        yaw;

        POI(de::Vec3f const &pos, float yawAngle = 0)
            : position(pos)
            , yaw(yawAngle)
        {}
    };

    virtual User *     localUser() const = 0;
    virtual POI        initialViewPosition() const;
    virtual QList<POI> pointsOfInterest() const;

    virtual float groundSurfaceHeight(de::Vec3f const &pos) const;
    virtual float ceilingHeight(de::Vec3f const &pos) const;

    DENG2_CAST_METHODS()

public:
    DENG2_DEFINE_AUDIENCE(Ready, void worldReady(World &))
};

} // namespace gloom

#endif // WORLD_H
