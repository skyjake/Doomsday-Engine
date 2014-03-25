/** @file packagefolder.cpp Folder that hosts a data package archive.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/PackageFolder"
#include "de/ArchiveFeed"

namespace de {

PackageFolder::PackageFolder(File &sourceArchiveFile, String const &name) : Folder(name)
{
    // Create the feed.
    attach(new ArchiveFeed(sourceArchiveFile));
}

PackageFolder::~PackageFolder()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String PackageFolder::describe() const
{
    DENG2_GUARD(this);

    String desc = String("package \"%1\"").arg(name());

    String const feedDesc = describeFeeds();
    if(!feedDesc.isEmpty())
    {
        desc += String(" (%1)").arg(feedDesc);
    }

    return desc;
}

Archive &PackageFolder::archive()
{
    DENG2_ASSERT(!feeds().empty());
    return feeds().front()->as<ArchiveFeed>().archive();
}

Archive const &PackageFolder::archive() const
{
    return const_cast<PackageFolder *>(this)->archive();
}

} // namespace de
