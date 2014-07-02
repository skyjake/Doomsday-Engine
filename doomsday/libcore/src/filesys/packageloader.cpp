/** @file packageloader.cpp  Loads and unloads packages.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/PackageLoader"
#include "de/FS"
#include "de/App"
#include "de/Version"

#include <QMap>

namespace de {

DENG2_PIMPL(PackageLoader)
{
    LoadedPackages loaded;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        // We own all loaded packages.
        qDeleteAll(loaded.values());
    }

    /**
     * Determines if a specific package is currently loaded.
     * @param pkg  Package.
     * @return @c true, if loaded.
     */
    bool isLoaded(File const &file) const
    {
        LoadedPackages::const_iterator found = loaded.constFind(Package::identifierForFile(file));
        if(found != loaded.constEnd())
        {
            return &found.value()->file() == &file;
        }
        return false;
    }

    static int ascendingPackagesByLatest(File const *a, File const *b)
    {
        // The version must be specified using a format understood by Version.
        Version const aVer(a->info().gets("version"));
        Version const bVer(b->info().gets("version"));

        if(aVer == bVer)
        {
            // Identifical versions are prioritized by modification time.
            return de::cmp(a->status().modifiedAt, b->status().modifiedAt);
        }
        return aVer < bVer? -1 : 1;
    }

    /**
     * Given a package identifier, pick one of the available versions of the package
     * based on predefined criteria.
     *
     * @param packageId  Package identifier.
     *
     * @return Selected package, or @c NULL if a version could not be selected.
     */
    File const *selectPackage(String const &packageId) const
    {
        LOG_AS("selectPackage");

        FS::FoundFiles found;
        if(!App::fileSystem().findAllOfType(DENG2_TYPE_NAME(ArchiveFolder), packageId + ".pack", found))
        {
            // None found.
            return 0;
        }

        // Each must have a version specified.
        DENG2_FOR_EACH_CONST(FS::FoundFiles, i, found)
        {
            Package::validateMetadata((*i)->info());
        }

        found.sort(ascendingPackagesByLatest);

        return found.back();
    }

    void load(String const &packageId, File const &source)
    {
        if(loaded.contains(packageId))
        {
            throw AlreadyLoadedError("PackageLoader::load",
                                     "Package '" + packageId + "' is already loaded from \"" +
                                     loaded[packageId]->info().gets("path") + "\"");
        }

        Package *pkg = new Package(source);
        loaded.insert(packageId, pkg);
        pkg->didLoad();
    }

    bool unload(String const &identifier)
    {
        LoadedPackages::iterator found = loaded.find(identifier);
        if(found == loaded.end()) return false;

        Package *pkg = found.value();
        pkg->aboutToUnload();
        delete pkg;

        loaded.remove(identifier);
        return true;
    }

    DENG2_PIMPL_AUDIENCE(Activity)
};

DENG2_AUDIENCE_METHOD(PackageLoader, Activity)

PackageLoader::PackageLoader() : d(new Instance(this))
{}

void PackageLoader::load(String const &packageId)
{
    LOG_AS("PackageLoader");

    File const *pack = d->selectPackage(packageId);
    if(!pack)
    {
        throw NotFoundError("PackageLoader::load",
                            "Package \"" + packageId + "\" is not available");
    }

    d->load(packageId, *pack);

    DENG2_FOR_AUDIENCE2(Activity, i)
    {
        i->setOfLoadedPackagesChanged();
    }
}

void PackageLoader::unload(String const &packageId)
{
    if(d->unload(packageId))
    {
        DENG2_FOR_AUDIENCE2(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
}

bool PackageLoader::isLoaded(String const &packageId) const
{
    return d->loaded.contains(packageId);
}

bool PackageLoader::isLoaded(File const &file) const
{
    return d->isLoaded(file);
}

PackageLoader::LoadedPackages const &PackageLoader::loadedPackages() const
{
    return d->loaded;
}

} // namespace de
