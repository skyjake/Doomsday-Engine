/** @file archivefolder.cpp  Folder whose contents represent an archive.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ArchiveFolder"
#include "de/ArchiveFeed"

namespace de {

ArchiveFolder::ArchiveFolder(File &sourceArchiveFile, String const &name) : Folder(name)
{
    // Create the feed.
    attach(new ArchiveFeed(sourceArchiveFile));
}

ArchiveFolder::~ArchiveFolder()
{
    DE_FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

void ArchiveFolder::flush()
{
    Folder::flush();
    primaryFeed()->as<ArchiveFeed>().rewriteFile();
}

String ArchiveFolder::describe() const
{
    DE_GUARD(this);

    String desc = Stringf("archive \"%s\"", name().c_str());

    String const feedDesc = describeFeeds();
    if (!feedDesc.isEmpty())
    {
        desc += " (";
        desc += feedDesc;
        desc += ")";
    }

    return desc;
}

Archive &ArchiveFolder::archive()
{
    DE_ASSERT(!feeds().empty());
    return primaryFeed()->as<ArchiveFeed>().archive();
}

Archive const &ArchiveFolder::archive() const
{
    return const_cast<ArchiveFolder *>(this)->archive();
}

} // namespace de
