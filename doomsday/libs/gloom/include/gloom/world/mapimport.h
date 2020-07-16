/** @file mapimport.h  Importer for id-formatted map data.
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

#ifndef GLOOM_MAPIMPORT_H
#define GLOOM_MAPIMPORT_H

#include "gloom/world/map.h"
#include "../libgloom.h"

#include <de/image.h>
#include <de/string.h>
#include <doomsday/res/lumpcatalog.h>

namespace gloom {

using namespace de;

/**
 * Imports an id-formatted map, converting it to the Gloom format.
 */
class LIBGLOOM_PUBLIC MapImport
{
public:
    explicit MapImport(const res::LumpCatalog &lumps);

    bool importMap(const String &mapId);

    Map &      map();
    String     mapId() const;
    StringList materials() const;
    Image      materialImage(const String &name) const;

    /**
     * Exports the map and its associated resources (textures) as a .pack.
     * @param packageRootPath
     */
    void exportPackage(const String &packageRootPath) const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAPIMPORT_H
