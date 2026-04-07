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

#pragma once

#include "gloom/identity.h"
#include "gloom/render/icamera.h"
#include "gloom/world/user.h"
#include "gloom/world/iworld.h"

#include <de/glshaderbank.h>
#include <de/imagebank.h>

namespace gloom {

using namespace de;

class Map;
class MapRender;

class LIBGLOOM_PUBLIC World : public IWorld
{
public:
    World(GLShaderBank &shaders, ImageBank &images);

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
    void setCurrentTime(double time);

    void glInit();
    void glDeinit();
    void update(TimeSpan elapsed);
    void render(const ICamera &camera);

    Map &      map();
    const Map &map() const;
    MapRender &mapRender();
    User *     localUser() const;
    POI        initialViewPosition() const;
    List<POI>  pointsOfInterest() const;

    /**
     * Determines the height of the ground at a given world coordinates.
     */
    double groundSurfaceHeight(const Vec3d &posMeters) const;

    double ceilingHeight(const Vec3d &posMeters) const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom
