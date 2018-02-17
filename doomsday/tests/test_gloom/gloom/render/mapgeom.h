/** @file mapgeom.h
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

#ifndef GLOOM_MAPGEOM_H
#define GLOOM_MAPGEOM_H

#include <de/AtlasTexture>
#include "../world/map.h"

namespace gloom {

class ICamera;

class MapRender
{
public:
    MapRender();

    void setAtlas(de::AtlasTexture &atlas);
    void setMap(const Map &map);

    void glInit();
    void glDeinit();
    void rebuild();
    void advanceTime(const de::TimeSpan &elapsed);
    void render(const ICamera &camera);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPGEOM_H
