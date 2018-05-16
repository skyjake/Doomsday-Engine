/** @file gloomworld.h
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

#ifndef GLOOMWORLD_H
#define GLOOMWORLD_H

#include "gloom/render/icamera.h"
#include "gloom/world/user.h"
#include "gloom/world/world.h"

namespace gloom {

using namespace de;

class Map;

class GloomWorld : public World
{
public:
    GloomWorld();

    /**
     * Loads a map.
     *
     * The map must be defined in the "maps.dei" file of one of the loaded packages.
     *
     * @param mapId
     */
    void loadMap(const String &mapId);

    void setLocalUser(User *user);
    void setMap(const Map &map);
    void setDebugMode(int debugMode);

    void glInit();
    void glDeinit();
    void update(de::TimeSpan const &elapsed);
    void render(ICamera const &camera);

    User *     localUser() const;
    POI        initialViewPosition() const;
    QList<POI> pointsOfInterest() const;

    /**
     * Determines the height of the ground at a given world coordinates.
     */
    float groundSurfaceHeight(de::Vec3f const &pos) const;

    float ceilingHeight(de::Vec3f const &pos) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOMWORLD_H
