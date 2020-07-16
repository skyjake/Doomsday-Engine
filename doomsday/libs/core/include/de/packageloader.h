/** @file packageloader.h  Loads and unloads packages.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_PACKAGELOADER_H
#define LIBCORE_PACKAGELOADER_H

#include "de/string.h"
#include "de/package.h"
//#include "de/archivefolder.h"
#include "de/filesystem.h"
#include "de/hash.h"

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
class DE_PUBLIC PackageLoader
{
public:
    /// Notified when any package is loaded or unloaded.
    DE_AUDIENCE(Activity, void setOfLoadedPackagesChanged())

    /// Notified when a package is loaded.
    DE_AUDIENCE(Load, void packageLoaded(const String &packageId))

    /// Notified when a package is unloaded.
    DE_AUDIENCE(Unload, void aboutToUnloadPackage(const String &packageId))

    /// Requested package was not found. @ingroup errors
    DE_ERROR(NotFoundError);

    /// Package is already loaded. @ingroup errors
    DE_ERROR(AlreadyLoadedError);

    /// Errors during reactions to loading a package. @ingroup errors
    DE_ERROR(PostLoadError);

    typedef Hash<String, Package *> LoadedPackages;

    /**
     * Utility for dealing with space-separated lists of identifiers.
     */
    struct DE_PUBLIC IdentifierList
    {
        StringList ids;

        IdentifierList(const String &spaceSeparatedIds);
    };

    static PackageLoader &get();

public:
    PackageLoader();

    /**
     * Checks if a specific package is available. There may be multiple
     * versions of the package.
     *
     * @param packageId  Package identifier(s), with optional versions.
     *
     * @return @c true, if the package is availalbe. Loading the package
     * should be successful.
     */
    bool isAvailable(const String &packageId) const;

    /**
     * Finds the file that would be loaded when loading with @a packageId.
     *
     * @param packageId  Package identifier(s), with optional versions.
     *
     * @return File representing the package, or @c nullptr if none found.
     */
    const File *select(const String &packageId) const;

    const Package &load(const String &packageId);

    void unload(const String &packageId);

    void unloadAll();

    /**
     * Repopulate the /packs folder synchronously. The loaded packages are present as links
     * under /packs, as are all the assets provided by the loaded packages.
     *
     * The /packs folder is not automatically refreshed after packages are loaded/unloaded.
     */
    void refresh();

    bool isLoaded(const String &packageId) const;

    bool isLoaded(const File &file) const;

    const Package *tryFindLoaded(const File &file) const;

    /**
     * Returns the set of all loaded packages.
     */
    const LoadedPackages &loadedPackages() const;

    List<Package *> loadedPackagesInOrder() const;

    FileSystem::FoundFiles loadedPackagesAsFilesInPackageOrder() const;

    enum IdentifierType { NonVersioned, Versioned };

    /**
     * Returns a list of the currently loaded package IDs. The identifiers include
     * version suffixes so that the packages can be unambigously located.
     *
     * @return Versioned package IDs.
     */
    StringList loadedPackageIdsInOrder(IdentifierType type = Versioned) const;

    /**
     * Retrieves a specific loaded package. The package must already be loaded
     * using load().
     *
     * @param packageId
     *
     * @return Package.
     */
    const Package &package(const String &packageId) const;

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
     * Lists all the packages specified on the command line (using the @c -pkg option)
     * so they can be loaded. The order matches the order of the command line parameters.
     */
    StringList loadedFromCommandLine() const;

    /**
     * Looks up all the packages in the file system index.
     */
    StringList findAllPackages() const;

    /**
     * Takes a list of package identifiers and checks if they have any dependent packages
     * (required, recommended, or extra). Those are then expanded using the same logic as
     * when loading packages (everything precedes the main package).
     *
     * @param packageIdentifiers  Package identifiers.
     *
     * @return Expanded list of identifiers.
     */
    StringList expandDependencies(const StringList &packageIdentifiers) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_PACKAGELOADER_H
