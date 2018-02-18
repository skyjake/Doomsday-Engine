/** @file entityrender.h
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

#ifndef GLOOM_ENTITIES_H
#define GLOOM_ENTITIES_H

#include "../world/entitymap.h"

#include <de/AtlasTexture>

namespace gloom {

class ICamera;
class Map;

class EntityRender
{
public:
    EntityRender();

    EntityMap &entityMap();

    void setMap(const Map *map);
    void setAtlas(de::AtlasTexture *atlas);
    void glInit();
    void glDeinit();
    void render(const ICamera &camera);

    void createEntities();

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_ENTITIES_H
