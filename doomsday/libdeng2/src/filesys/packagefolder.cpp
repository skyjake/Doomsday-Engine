/** @file packagefolder.cpp Folder that hosts a data package archive.
 * @ingroup fs
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
    DENG2_FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    deindex();
}

Archive &PackageFolder::archive()
{
    DENG2_ASSERT(!feeds().empty());
    DENG2_ASSERT(dynamic_cast<ArchiveFeed *>(feeds().front()) != 0);

    return static_cast<ArchiveFeed *>(feeds().front())->archive();
}

Archive const &PackageFolder::archive() const
{
    return const_cast<PackageFolder *>(this)->archive();
}

} // namespace de
