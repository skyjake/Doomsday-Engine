/** @file datafolder.h  Classic data files: PK3.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DATAFOLDER_H
#define LIBDOOMSDAY_DATAFOLDER_H

#include "../res/databundle.h"
#include <de/folder.h>

/**
 * FS2 file for classic container-like data files: PK3, Snowberry Box.
 *
 * Containers are represented as folders so that their contents can be
 * accessed via the file tree.
 *
 * @todo WAD files should use DataFolder, too.
 */
class LIBDOOMSDAY_PUBLIC DataFolder : public de::Folder, public DataBundle
{
public:
    DataFolder(Format format, de::File &sourceFile);
    ~DataFolder();

    de::String describe() const override;

    // Stream access.
    const IIStream &operator >> (IByteArray &bytes) const override;
};

#endif // LIBDOOMSDAY_DATABUNDLE_H
