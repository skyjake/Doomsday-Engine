/** @file mapbuilder.h  Backend for constructing a map (MPE API).
 *
 * @authors Copyright © 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#pragma once

#include "../libdoomsday.h"
#include "map.h"
#include <de/string.h>
#include <de/stringpool.h>

namespace world {

class Material;

class LIBDOOMSDAY_PUBLIC MapBuilder
{
public:
    void begin();
    void end();

    void clear();    
    void clearMaterialDict();
    
    /**
     * Print any "missing" materials in the dictionary to the log.
     */
    void printMissingMaterialsInDict() const;
    
    /**
     * Attempt to locate a material by its URI. A dictionary of previously
     * searched-for URIs is maintained to avoid repeated searching and to record
     * "missing" materials.
     *
     * @param materialUriStr  URI of the material to search for.
     *
     * @return  Pointer to the found material; otherwise @c 0.
     */
    Material *findMaterialInDict(const de::String &materialUriStr);

    inline Material *findMaterialInDict(const char *materialUriStr)
    {
        if (!materialUriStr) return nullptr;
        return findMaterialInDict(de::String(materialUriStr));
    }
    
    Map *take();
    
    operator Map &()
    {
        DE_ASSERT(_map);
        return *_map.get();
    }

    operator Map *()
    {
        DE_ASSERT(_map);
        return _map.get();
    }
    
    explicit operator bool() const
    {
        return bool(_map);
    }
    
    Map &map() const
    {
        DE_ASSERT(_map);
        return *_map;
    }
    
    Map *operator->()
    {
        return &map();
    }
    
private:
    std::unique_ptr<Map> _map;
    
    /**
     * Material name references specified during map conversion are recorded in
     * this dictionary. A dictionary is used to avoid repeatedly resolving the same
     * URIs and to facilitate a log of missing materials encountered during the
     * process.
     *
     * The pointer user value holds a pointer to the resolved Material (if found).
     * The integer user value tracks the number of times a reference occurs.
     */
    std::unique_ptr<de::StringPool> _materialDict;
};

LIBDOOMSDAY_PUBLIC extern MapBuilder editMap;

} // namespace world
