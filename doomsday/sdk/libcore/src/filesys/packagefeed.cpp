/** @file packagefeed.cpp  Links to loaded packages.
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

#include "de/PackageFeed"
#include "de/LinkFile"
#include "de/ArchiveFolder"
#include "de/PackageLoader"
#include "de/FS"

namespace de {

DENG2_PIMPL(PackageFeed)
{
    PackageLoader &loader;

    Impl(Public *i, PackageLoader &ldr)
        : Base(i), loader(ldr)
    {}

    File *linkToPackage(Package &pkg, String const &linkName, Folder const &folder)
    {
        /// @todo Resolve conflicts: replace, ignore, or fail. -jk

        if (folder.has(linkName)) return nullptr; // Already there, keep the existing link.

        // Create a link to the loaded package's file.
        LinkFile *link = LinkFile::newLinkToFile(pkg.file(), linkName);

        // We will decide on pruning this.
        link->setOriginFeed(thisPublic);

        return link;
    }

    PopulatedFiles populate(Folder const &folder)
    {
        PopulatedFiles populated;
        DENG2_FOR_EACH_CONST(PackageLoader::LoadedPackages, i, loader.loadedPackages())
        {
            Package *pkg = i.value();
            populated << linkToPackage(*pkg, i.key(), folder);

            // Also link it under its possible alias identifier (for variants).
            if (pkg->objectNamespace().has(Package::VAR_PACKAGE_ALIAS))
            {
                populated << linkToPackage(*pkg, pkg->objectNamespace()
                                           .gets(Package::VAR_PACKAGE_ALIAS), folder);
            }

            // Link each contained asset, too.
            foreach (String ident, pkg->assets())
            {
                populated << linkToPackage(*pkg, "asset." + ident, folder);
            }
        }
        return populated;
    }
};

PackageFeed::PackageFeed(PackageLoader &loader) : d(new Impl(this, loader))
{}

PackageLoader &PackageFeed::loader()
{
    return d->loader;
}

String PackageFeed::description() const
{
    return "loaded packages";
}

Feed::PopulatedFiles PackageFeed::populate(Folder const &folder)
{
    return d->populate(folder);
}

bool PackageFeed::prune(File &file) const
{
    if (LinkFile const *link = maybeAs<LinkFile>(file))
    {
        if (Folder const *pkg = maybeAs<Folder>(link->target()))
        {
            // Links to unloaded packages should be pruned.
            if (!d->loader.isLoaded(*pkg)) return true;

            // Package has been modified, should be pruned.
            if (link->status() != pkg->status()) return true;
        }
    }
    return false; // Don't prune.
}

} // namespace de
