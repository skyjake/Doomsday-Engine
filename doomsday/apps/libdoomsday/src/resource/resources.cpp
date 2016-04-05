/** @file resource/resources.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/resources.h"
#include "doomsday/filesys/fs_main.h"

#include <de/App>
#include <de/CommandLine>
#include <de/Config>

using namespace de;

static Resources *theResources = nullptr;

DENG2_PIMPL(Resources)
{
    typedef QList<ResourceClass *> ResourceClasses;
    ResourceClasses resClasses;
    NullResourceClass nullResourceClass;
    NativePath nativeSavePath;
    MapManifests mapManifests;

    Instance(Public *i)
        : Base(i)
        , nativeSavePath(App::app().nativeHomePath() / "savegames") // default
    {
        theResources = thisPublic;

        resClasses << new ResourceClass("RC_PACKAGE",    "Packages")
                   << new ResourceClass("RC_DEFINITION", "Defs")
                   << new ResourceClass("RC_GRAPHIC",    "Graphics")
                   << new ResourceClass("RC_MODEL",      "Models")
                   << new ResourceClass("RC_SOUND",      "Sfx")
                   << new ResourceClass("RC_MUSIC",      "Music")
                   << new ResourceClass("RC_FONT",       "Fonts");

        // Determine the root directory of the saved session repository.
        if(auto arg = App::commandLine().check("-savedir", 1))
        {
            // Using a custom root save directory.
            App::commandLine().makeAbsolutePath(arg.pos + 1);
            nativeSavePath = App::commandLine().at(arg.pos + 1);
        }
    }

    ~Instance()
    {
        self.clearMapManifests();
        qDeleteAll(resClasses);

        theResources = nullptr;
    }
};

Resources::Resources() : d(new Instance(this))
{}

void Resources::timeChanged(Clock const &)
{
    // Nothing to do.
}

void Resources::clear()
{}

Resources &Resources::get()
{
    DENG2_ASSERT(theResources);
    return *theResources;
}

ResourceClass &Resources::resClass(String name)
{
    if(!name.isEmpty())
    {
        foreach(ResourceClass *resClass, d->resClasses)
        {
            if(!resClass->name().compareWithoutCase(name))
                return *resClass;
        }
    }
    return d->nullResourceClass; // Not found.
}

ResourceClass &Resources::resClass(resourceclassid_t id)
{
    if(id == RC_NULL) return d->nullResourceClass;
    if(VALID_RESOURCECLASSID(id))
    {
        return *d->resClasses.at(uint(id));
    }
    /// @throw UnknownResourceClass Attempted with an unknown id.
    throw UnknownResourceClassError("ResourceResources::toClass", QString("Invalid id '%1'").arg(int(id)));
}

void Resources::updateOverrideIWADPathFromConfig()
{
    String path = App::config().gets("resource.iwadFolder", "");
    if(!path.isEmpty())
    {
        LOG_RES_NOTE("Using user-selected primary IWAD folder: \"%s\"") << path;

        FS1::Scheme &ps = App_FileSystem().scheme(App_ResourceClass("RC_PACKAGE").defaultScheme());
        ps.clearSearchPathGroup(FS1::OverridePaths);
        ps.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path, RC_PACKAGE),
                                    SearchPath::NoDescend), FS1::OverridePaths);
    }
}

NativePath Resources::nativeSavePath() const
{
    return d->nativeSavePath;
}

res::MapManifest &Resources::findMapManifest(de::Uri const &mapUri) const
{
    // Only one resource scheme is known for maps.
    if(!mapUri.scheme().compareWithoutCase("Maps"))
    {
       if(res::MapManifest *found = d->mapManifests.tryFind(
                   mapUri.path(), PathTree::MatchFull | PathTree::NoBranch))
           return *found;
    }
    /// @throw MissingResourceManifestError  An unknown map URI was specified.
    throw MissingResourceManifestError("Resources::findMapManifest", "Failed to locate a manifest for \"" + mapUri.asText() + "\"");
}

res::MapManifest *Resources::tryFindMapManifest(de::Uri const &mapUri) const
{
    // Only one resource scheme is known for maps.
    if(mapUri.scheme().compareWithoutCase("Maps")) return nullptr;
    return d->mapManifests.tryFind(mapUri.path(), PathTree::MatchFull | PathTree::NoBranch);
}

dint Resources::mapManifestCount() const
{
    return d->mapManifests.count();
}

void Resources::initMapManifests()
{
    clearMapManifests();

    // Locate all the maps using the central lump index:
    /// @todo Locate new maps each time a package is loaded rather than rely on
    /// the central lump index.
    LumpIndex const &lumpIndex = App_FileSystem().nameIndex();
    lumpnum_t lastLump = -1;
    while(lastLump < lumpIndex.size())
    {
        std::unique_ptr<Id1MapRecognizer> recognizer(new Id1MapRecognizer(lumpIndex, lastLump));
        lastLump = recognizer->lastLump();
        if(recognizer->format() != Id1MapRecognizer::UnknownFormat)
        {
            File1 *sourceFile  = recognizer->sourceFile();
            String const mapId = recognizer->id();

            res::MapManifest &manifest = d->mapManifests.insert(mapId);
            manifest.set("id", mapId);
            manifest.setSourceFile(sourceFile)
                    .setRecognizer(recognizer.release());
        }
    }
}

void Resources::clearMapManifests()
{
    d->mapManifests.clear();
}

Resources::MapManifests const &Resources::allMapManifests() const
{
    return d->mapManifests;
}

ResourceClass &App_ResourceClass(String className)
{
    return Resources::get().resClass(className);
}

ResourceClass &App_ResourceClass(resourceclassid_t classId)
{
    return Resources::get().resClass(classId);
}

void Resources::clearAllMaterialSchemes()
{}
