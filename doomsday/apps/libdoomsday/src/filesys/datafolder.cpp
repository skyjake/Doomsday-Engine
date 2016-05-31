/** @file datafolder.cpp  Classic data files: PK3.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/filesys/datafolder.h"

#include <de/ArchiveFeed>
#include <de/ZipArchive>

using namespace de;

DataFolder::DataFolder(Format format, File &sourceFile)
    : Folder(sourceFile.name())
    , DataBundle(format, sourceFile)
{
    setSource(&sourceFile);

    // Contents of ZIP archives appear inside the folder automatically.
    if (ZipArchive::recognize(sourceFile))
    {
        attach(new ArchiveFeed(sourceFile));
    }

    /*else if (sourceFile.originFeed())
    {
        attach(sourceFile.originFeed()->newSubFeed(sourceFile.name()));
    }
    else
    {
        DENG2_ASSERT(!"DataFolder doesn't know how to access the source file");
    }*/
}

DataFolder::~DataFolder()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String DataFolder::describe() const
{
    String desc = DataBundle::description();

    // The folder contents (if any) are produced by feeds.
    String const feedDesc = describeFeeds();
    if (!feedDesc.isEmpty())
    {
        desc += String(" (%1)").arg(feedDesc);
    }

    return desc;
}
