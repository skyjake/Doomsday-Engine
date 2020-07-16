/** @file mapbuilder.cpp  Backend for constructing a map (MPE API).
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

#include "doomsday/world/mapbuilder.h"
#include "doomsday/world/factory.h"
#include "doomsday/world/materials.h"
#include "doomsday/res/resources.h"

namespace world {

using namespace de;

MapBuilder editMap; // public

void MapBuilder::clear()
{
    _map.reset();
    clearMaterialDict();
}

void MapBuilder::begin()
{
    if (!_map)
    {
        _map.reset(Factory::newMap());
    }
}

void MapBuilder::end()
{
    // Log warnings about any issues we encountered during conversion of
    // the basic map data elements.
    printMissingMaterialsInDict();
    clearMaterialDict();
}

void MapBuilder::clearMaterialDict()
{
    if (_materialDict)
    {
        _materialDict->clear();
        _materialDict.reset();
    }
}

void MapBuilder::printMissingMaterialsInDict() const
{
    if (!_materialDict) return;

    _materialDict->forAll([this](StringPool::Id id)
    {
        // A valid id?
        if (_materialDict->string(id))
        {
            // An unresolved reference?
            if (!_materialDict->userPointer(id))
            {
                LOG_RES_WARNING("Found %4i x unknown material \"%s\"")
                    << _materialDict->userValue(id)
                    << _materialDict->string(id);
            }
        }
        return LoopContinue;
    });
}

Material *MapBuilder::findMaterialInDict(const String &materialUriStr)
{
    if (materialUriStr.isEmpty()) return 0;

    // Time to create the dictionary?
    if (!_materialDict)
    {
        _materialDict.reset(new StringPool);
    }

    res::Uri materialUri(materialUriStr, RC_NULL);

    // Intern this reference.
    StringPool::Id internId = _materialDict->intern(materialUri.compose());
    world::Material *material = 0;

    // Have we previously encountered this?.
    uint refCount = _materialDict->userValue(internId);
    if (refCount)
    {
        // Yes, if resolved the user pointer holds the found material.
        material = reinterpret_cast<world::Material *>(_materialDict->userPointer(internId));
    }
    else
    {
        // No, attempt to resolve this URI and update the dictionary.
        // First try the preferred scheme, then any.
        try
        {
            material = &Materials::get().material(materialUri);
        }
        catch (const Resources::MissingResourceManifestError &)
        {
            // Try any scheme.
            try
            {
                materialUri.setScheme("");
                material = &Materials::get().material(materialUri);
            }
            catch (const Resources::MissingResourceManifestError &)
            {}
        }

        // Insert the possibly resolved material into the dictionary.
        _materialDict->setUserPointer(internId, material);
    }

    // There is now one more reference.
    refCount++;
    _materialDict->setUserValue(internId, refCount);

    return material;
}

Map *MapBuilder::take()
{
    return _map.release();
}

} // namespace world
