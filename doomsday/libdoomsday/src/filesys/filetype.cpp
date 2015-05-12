/** @file
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/filesys/filetype.h"

using namespace de;

static NullFileType nullFileType;

/// A symbolic name => file type map.
static FileTypes fileTypeMap;

void DD_AddFileType(FileType const &ftype)
{
    fileTypeMap.insert(ftype.name().toLower(), &ftype);
}

FileType const &DD_FileTypeByName(String name)
{
    if(!name.isEmpty())
    {
        FileTypes::const_iterator found = fileTypeMap.constFind(name.toLower());
        if(found != fileTypeMap.constEnd()) return **found;
    }
    return nullFileType; // Not found.
}

FileType const &DD_GuessFileTypeFromFileName(String path)
{
    if(!path.isEmpty())
    {
        DENG2_FOR_EACH_CONST(FileTypes, i, fileTypeMap)
        {
            FileType const &ftype = **i;
            if(ftype.fileNameIsKnown(path))
                return ftype;
        }
    }
    return nullFileType;
}

FileTypes &DD_FileTypes()
{
    return fileTypeMap;
}
