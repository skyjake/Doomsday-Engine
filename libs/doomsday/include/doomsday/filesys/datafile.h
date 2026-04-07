/** @file datafile.h  Classic data files: WAD, LMP, DED, DEH.
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

#ifndef LIBDOOMSDAY_DATAFILE_H
#define LIBDOOMSDAY_DATAFILE_H

#include "../res/databundle.h"
#include <de/bytearrayfile.h>

/**
 * FS2 File for for classic data files: LMP, DED, DEH.
 */
class LIBDOOMSDAY_PUBLIC DataFile : public de::ByteArrayFile, public DataBundle
{
public:
    DataFile(Format format, File &sourceFile);
    ~DataFile();

    de::String describe() const;

    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, const Byte *values, Size count);
};

#endif // LIBDOOMSDAY_DATAFILE_H
