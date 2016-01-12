/** @file datafile.cpp  Classic data files: WAD, LMP, DED, DEH.
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

#include "doomsday/filesys/datafile.h"

#include <de/Reader>

using namespace de;

DataFile::DataFile(Format format, File &sourceFile)
    : ByteArrayFile(sourceFile.name())
    , DataBundle(format, sourceFile)
{
    setSource(&sourceFile);

    if(format == Iwad || format == Pwad)
    {
        // Automatically set the correct format.
        Block fileType;
        fileType.resize(4);
        get(0, fileType.data(), fileType.size());
        setFormat(fileType == "IWAD"? Iwad : Pwad);
    }
}

DataFile::~DataFile()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String DataFile::describe() const
{
    return DataBundle::description();
}

void DataFile::get(Offset at, Byte *values, Size count) const
{
    DataBundle::get(at, values, count);
}

void DataFile::set(Offset at, Byte const *values, Size count)
{
    DataBundle::set(at, values, count);
}
