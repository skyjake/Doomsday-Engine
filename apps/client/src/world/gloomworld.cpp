/** @file gloomworld.cpp  World integrated with libgloom for rendering.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "world/gloomworld.h"
#include "render/rendersystem.h"
#include "render/gloomworldrenderer.h"
#include "clientapp.h"

#include <doomsday/res/lumpcatalog.h>
#include <doomsday/world/map.h>
#include <doomsday/world/convexsubspace.h>
#include <doomsday/world/subsector.h>
#include <doomsday/world/plane.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/material.h>
#include <doomsday/world/mobjthinkerdata.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/sky.h>
#include <doomsday/world/surface.h>
#include <gloom/world/mapimport.h>
#include <de/packageloader.h>

using namespace de;

DE_PIMPL(GloomWorld)
{
    String exportedPath;

    Impl(Public *i) : Base(i)
    {}
};

GloomWorld::GloomWorld()
    : d(new Impl(this))
{
    useDefaultConstructors();
}

String GloomWorld::mapPackageId() const
{
    return Package::identifierForFile(FS::locate<const File>(d->exportedPath));
}

void GloomWorld::aboutToChangeMap()
{
    ClientApp::render().world().unloadMap();
    PackageLoader::get().unload(mapPackageId());
    d->exportedPath.clear();
}

void GloomWorld::mapFinalized()
{
    res::LumpCatalog lumps;
    lumps.setPackages(PackageLoader::get().loadedPackageIdsInOrder());

    const String   mapId    = map().uri().path().toString().upper();
    const auto     pos      = lumps.find(mapId);
    const uint32_t checksum = crc32(Block(*pos.first /* DataBundle */));

    DE_ASSERT(!mapPackageId() || !PackageLoader::get().isLoaded(mapPackageId()));

    d->exportedPath = Stringf("/home/cache/maps/net.dengine.exported.%s.%08x.pack/%s.pack",
                              String(pos.first->asFile().name().fileNameWithoutExtension()).c_str(),
                              checksum,
                              mapId.c_str())
                          .lower();

    // If this already exists, no need to re-export.
    if (!FS::exists(d->exportedPath))
    {
        gloom::MapImport importer(lumps);
        if (importer.importMap(mapId))
        {
            // Successfully imported. Now write it to a package.
            importer.exportPackage(d->exportedPath);
        }
    }

    PackageLoader::get().load(mapPackageId());
    
    // We are likely in a busy thread now, so we shouldn't do GL operations.
    // Load the map in the main thread instead.
    Loop::mainCall([mapId]() {
        ClientApp::render().world().loadMap(mapId.lower());
    });
}
