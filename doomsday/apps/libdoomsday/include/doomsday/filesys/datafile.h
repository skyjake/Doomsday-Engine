/** @file datafile.h  FS2 File for classic data files: PK3, WAD, LMP, DED, DEH.
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

#ifndef LIBDOOMSDAY_DATAFILE_H
#define LIBDOOMSDAY_DATAFILE_H

#include "../libdoomsday.h"
#include <de/filesys/IInterpreter>
#include <de/ByteArrayFile>

/**
 * Interpreter (for libcore FS) for classic data files: PK3, WAD, LMP, DED, DEH.
 *
 * Generates Doomsday 2 compatible metadata for data files, allowing them to
 * be treated as packages at runtime.
 *
 * As packages, DataFile makes sure that the data gets loaded and unloaded
 * when games are loaded (and in the right order). The data is actually loaded
 * using the resource subsystem. To facilitate that, data file contents are
 * available as plain byte arrays.
 */
class LIBDOOMSDAY_PUBLIC DataFile : public de::ByteArrayFile
{
public:
    enum Format { Pk3, Wad, Lump, Ded, Dehacked };

public:
    ~DataFile();

    de::String describe() const;
    Format format() const;

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

public:
    struct LIBDOOMSDAY_PUBLIC Interpreter : public de::filesys::IInterpreter {
        de::File *interpretFile(de::File *sourceData) const override;
    };

protected:
    /**
     * Constructs a DataFile. The constructor is protected because DataFile is
     * used as an interpreter so it gets created via IInterpreter.
     *
     * @param format  Classic data file format.
     * @param name    Name of the file.
     */
    DataFile(Format format, de::String const &name = "");

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DATAFILE_H
