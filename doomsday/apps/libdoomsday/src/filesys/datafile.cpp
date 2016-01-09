/** @file datafile.cpp  FS2 File for classic data files: PK3, WAD, LMP, DED, DEH.
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

using namespace de;

namespace internal
{
    static char const *formatDescriptions[] =
    {
        "PK3 archive",
        "WAD file",
        "data lump",
        "DED definitions",
        "DeHackEd patch"
    };
}

DENG2_PIMPL(DataFile)
{
    Format format;

    Instance(Public *i, Format fmt) : Base(i), format(fmt)
    {}
};

DataFile::DataFile(Format format, String const &name)
    : ByteArrayFile(name)
    , d(new Instance(this, format))
{}

DataFile::~DataFile()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
}

String DataFile::describe() const
{
    return QString("%1 \"%2\"")
            .arg(internal::formatDescriptions[d->format])
            .arg(name().fileNameWithoutExtension());
}

IByteArray::Size DataFile::size() const
{
    DENG2_ASSERT(source());
    return source()->size();
}

void DataFile::get(Offset at, Byte *values, Size count) const
{
    DENG2_ASSERT(source());
    source()->as<ByteArrayFile>().get(at, values, count);
}

void DataFile::set(Offset at, Byte const *values, Size count)
{
    DENG2_ASSERT(source());
    source()->as<ByteArrayFile>().set(at, values, count);
}

File *DataFile::Interpreter::interpretFile(File *sourceData) const
{
    // Naive check using the file extension.
    static struct { String str; Format format; } formats[] = {
        { ".pk3", Pk3 },
        { ".wad", Wad },
        { ".lmp", Lump },
        { ".ded", Ded },
        { ".deh", Dehacked }
    };
    String const ext = sourceData->name().fileNameExtension();
    for(auto const &fmt : formats)
    {
        if(ext == fmt.str)
        {
            LOG_RES_VERBOSE("Interpreted ") << sourceData->description()
                                            << " as "
                                            << internal::formatDescriptions[fmt.format];

            auto *data = new DataFile(fmt.format, sourceData->name());
            data->setSource(sourceData);
            return data;
        }
    }

    // Was not interpreted.
    return nullptr;
}
