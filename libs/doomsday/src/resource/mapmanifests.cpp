/** @file mapmanifests.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/mapmanifests.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/res/resources.h" // MissingResourceManifestError

using namespace de;

namespace res {

DE_PIMPL_NOREF(MapManifests)
{
    Tree manifests;

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        manifests.clear();
    }
};

MapManifests::MapManifests() : d(new Impl)
{}

res::MapManifest &MapManifests::findMapManifest(const res::Uri &mapUri) const
{
    // Only one resource scheme is known for maps.
    if (!mapUri.scheme().compareWithoutCase("Maps"))
    {
       if (res::MapManifest *found = d->manifests.tryFind(
                   mapUri.path(), PathTree::MatchFull | PathTree::NoBranch))
           return *found;
    }
    /// @throw MissingResourceManifestError  An unknown map URI was specified.
    throw Resources::MissingResourceManifestError("MapManifests::findMapManifest", "Failed to locate a manifest for \"" + mapUri.asText() + "\"");
}

res::MapManifest *MapManifests::tryFindMapManifest(const res::Uri &mapUri) const
{
    // Only one resource scheme is known for maps.
    if (mapUri.scheme().compareWithoutCase("Maps")) return nullptr;
    return d->manifests.tryFind(mapUri.path(), PathTree::MatchFull | PathTree::NoBranch);
}

dint MapManifests::mapManifestCount() const
{
    return d->manifests.count();
}

void MapManifests::initMapManifests()
{
    d->clear();

    // Locate all the maps using the central lump index:
    /// @todo Locate new maps each time a package is loaded rather than rely on
    /// the central lump index.
    const LumpIndex &lumpIndex = App_FileSystem().nameIndex();
    lumpnum_t lastLump = -1;
    while (lastLump < lumpIndex.size())
    {
        std::unique_ptr<Id1MapRecognizer> recognizer(new Id1MapRecognizer(lumpIndex, lastLump));
        lastLump = recognizer->lastLump();
        if (recognizer->format() != Id1MapRecognizer::UnknownFormat)
        {
            File1 *sourceFile  = recognizer->sourceFile();
            const String mapId = recognizer->id();

            res::MapManifest &manifest = d->manifests.insert(mapId);
            manifest.set("id", mapId);
            manifest.setSourceFile(sourceFile)
                    .setRecognizer(recognizer.release());
        }
    }
}

const MapManifests::Tree &MapManifests::allMapManifests() const
{
    return d->manifests;
}

} // namespace res
