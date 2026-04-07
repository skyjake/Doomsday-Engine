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

#include "de/packagefeed.h"
#include "de/linkfile.h"
//#include "de/archivefolder.h"
#include "de/packageloader.h"
#include "de/filesystem.h"

namespace de {

static String const VAR_LINK_PACKAGE_ID("link.package");

DE_PIMPL(PackageFeed)
{
    PackageLoader &loader;
    LinkMode linkMode;
    Filter filter;

    Impl(Public *i, PackageLoader &ldr, LinkMode lm)
        : Base(i), loader(ldr), linkMode(lm)
    {}

    File *linkToPackage(Package &pkg, const String &linkName, const Folder &folder)
    {
        /// @todo Resolve conflicts: replace, ignore, or fail. -jk

        if (folder.has(linkName)) return nullptr; // Already there, keep the existing link.

        // Packages can be optionally filtered from the feed.
        if (filter && !filter(pkg)) return nullptr;

        // Create a link to the loaded package's file.
        String name;
        if (linkMode == LinkIdentifier)
        {
            name = linkName;
        }
        else
        {
            name = Package::versionedIdentifierForFile(pkg.file());
        }
        LinkFile *link = LinkFile::newLinkToFile(pkg.file(), name);

        // We will decide on pruning this.
        link->setOriginFeed(thisPublic);

        // Identifier also in metadata.
        link->objectNamespace().addText(VAR_LINK_PACKAGE_ID, pkg.identifier());

        return link;
    }

    PopulatedFiles populate(const Folder &folder)
    {
        PopulatedFiles populated;
        for (auto &i : loader.loadedPackages())
        {
            Package *pkg = i.second;
            populated << linkToPackage(*pkg, i.first, folder);

            // Also link it under its possible alias identifier (for variants).
            if (pkg->objectNamespace().has(Package::VAR_PACKAGE_ALIAS))
            {
                populated << linkToPackage(*pkg, pkg->objectNamespace()
                                           .gets(Package::VAR_PACKAGE_ALIAS), folder);
            }

            // Link each contained asset, too.
            for (const String &ident : pkg->assets())
            {
                populated << linkToPackage(*pkg, DE_STR("asset.") + ident, folder);
            }
        }
        return populated;
    }
};

PackageFeed::PackageFeed(PackageLoader &loader, LinkMode linkMode)
    : d(new Impl(this, loader, linkMode))
{}

void PackageFeed::setFilter(const Filter &filter)
{
    d->filter = filter;
}

PackageLoader &PackageFeed::loader()
{
    return d->loader;
}

String PackageFeed::description() const
{
    return "loaded packages";
}

Feed::PopulatedFiles PackageFeed::populate(const Folder &folder)
{
    return d->populate(folder);
}

bool PackageFeed::prune(File &file) const
{
    if (const LinkFile *link = maybeAs<LinkFile>(file))
    {
        // Links to unloaded packages should be pruned.
        if (!d->loader.isLoaded(link->objectNamespace().gets(VAR_LINK_PACKAGE_ID)))
            return true;

        //if (const Folder *pkg = maybeAs<Folder>(link->target()))
//        {
            // Links to unloaded packages should be pruned.
            //if (!d->loader.isLoaded(*pkg)) return true;

//            qDebug() << "Link:" << link->description() << link->status().modifiedAt.asText();
//            qDebug() << " tgt:" << link->target().description() << link->target().status().modifiedAt.asText();

        // Package has been modified, should be pruned.
        if (link->status() != link->target().status())
            return true;
//        }
    }
    return false; // Don't prune.
}

} // namespace de
