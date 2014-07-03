/** @file packagefeed.cpp  Links to loaded packages.
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

#include "de/PackageFeed"
#include "de/LinkFile"
#include "de/ArchiveFolder"
#include "de/PackageLoader"
#include "de/FS"

namespace de {
DENG2_PIMPL(PackageFeed)
{
    PackageLoader &loader;

    Instance(Public *i, PackageLoader &ldr)
        : Base(i), loader(ldr)
    {}

    void linkToPackage(Package &pkg, String const &linkName, Folder &folder)
    {
        if(folder.has(linkName)) return; // Already there.

        // Create a link to the loaded package's file.
        LinkFile &link = folder.add(LinkFile::newLinkToFile(pkg.file(), linkName));

        // We will decide on pruning this.
        link.setOriginFeed(thisPublic);

        // Include new files in the main index.
        folder.fileSystem().index(link);
    }

    void populate(Folder &folder)
    {
        DENG2_FOR_EACH_CONST(PackageLoader::LoadedPackages, i, loader.loadedPackages())
        {
            Package *pkg = i.value();
            linkToPackage(*pkg, i.key(), folder);
            // Also link it under its possible alias identifier (for variants).
            if(pkg->info().has("alias"))
            {
                linkToPackage(*pkg, pkg->info().gets("alias"), folder);
            }
        }
    }
};
} // namespace de

namespace de {

PackageFeed::PackageFeed(PackageLoader &loader) : d(new Instance(this, loader))
{}

PackageLoader &PackageFeed::loader()
{
    return d->loader;
}

String PackageFeed::description() const
{
    return "loaded packages";
}

void PackageFeed::populate(Folder &folder)
{
    d->populate(folder);
}

bool PackageFeed::prune(File &file) const
{
    if(LinkFile const *link = file.maybeAs<LinkFile>())
    {
        if(ArchiveFolder const *pkg = link->target().maybeAs<ArchiveFolder>())
        {
            // Links to unloaded packages should be pruned.
            if(!d->loader.isLoaded(*pkg)) return true;

            // Package has been modified, should be pruned.
            if(link->status() != pkg->status()) return true;
        }
    }
    return false; // Don't prune.
}

} // namespace de
