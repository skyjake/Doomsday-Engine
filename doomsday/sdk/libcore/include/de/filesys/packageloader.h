/** @file packageloader.h  Loads and unloads packages.
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

#ifndef LIBDENG2_PACKAGELOADER_H
#define LIBDENG2_PACKAGELOADER_H

#include "../String"
#include "../Package"
#include "../ArchiveFolder"
#include "../FileSystem"

#include <QMap>

namespace de {

/**
 * Package loader/unloader.
 *
 * PackageLoader's responsibilities include knowing which packages are loaded, the
 * priority order for loaded packages, and providing means to locate specific sets of
 * files from the loaded packages.
 *
 * PackageLoader assumes that the file system has already indexed all the available
 * packages as ArchiveFolder instances.
 *
 * @todo Observe FS index to see when packages become available at runtime. -jk
 *
 * @ingroup fs
 */
class DENG2_PUBLIC PackageLoader
{
public:
    /// Notified when a package is loaded or unloaded.
    DENG2_DEFINE_AUDIENCE2(Activity, void setOfLoadedPackagesChanged())

    /// Requested package was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// Package is already loaded. @ingroup errors
    DENG2_ERROR(AlreadyLoadedError);

    typedef QMap<String, Package *> LoadedPackages;

public:
    PackageLoader();

    Package const &load(String const &packageId);

    void unload(String const &packageId);

    bool isLoaded(String const &packageId) const;

    bool isLoaded(File const &file) const;

    /**
     * Returns the set of all loaded packages.
     */
    LoadedPackages const &loadedPackages() const;

    /**
     * Retrieves a specific loaded package. The package must already be loaded
     * using load().
     *
     * @param packageId
     *
     * @return Package.
     */
    Package const &package(String const &packageId) const;

    /**
     * Sorts the files in the provided list in package order: files from earlier-loaded
     * packages are sorted before files from later-loaded packages.
     *
     * If a file is not containing inside a package, it will appear before all files that
     * are in packages.
     *
     * @param filesToSort  File list.
     */
    void sortInPackageOrder(FileSystem::FoundFiles &filesToSort) const;

    /**
     * Loads all the packages specified on the command line (using the @c -pkg option).
     */
    void loadFromCommandLine();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_PACKAGELOADER_H
