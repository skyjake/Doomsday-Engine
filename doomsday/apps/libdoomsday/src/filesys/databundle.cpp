/** @file databundle.cpp  Classic data files: PK3, WAD, LMP, DED, DEH.
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

#include "doomsday/filesys/databundle.h"
#include "doomsday/filesys/datafolder.h"
#include "doomsday/filesys/datafile.h"

#include <de/ArchiveFeed>

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

DENG2_PIMPL(DataBundle)
{
    SafePtr<File> source;
    Format format;

    Instance(Public *i, Format fmt) : Base(i), format(fmt)
    {}
};

DataBundle::DataBundle(Format format, File &source)
    : d(new Instance(this, format))
{
    d->source.reset(&source);
}

DataBundle::~DataBundle()
{}

String DataBundle::description() const
{
    if(!d->source)
    {
        return "invalid data bundle";
    }
    return QString("%1 \"%2\"")
            .arg(internal::formatDescriptions[d->format])
            .arg(d->source->name().fileNameWithoutExtension());
}

IByteArray::Size DataBundle::size() const
{
    if(d->source)
    {
        return d->source->size();
    }
    return 0;
}

void DataBundle::get(Offset at, Byte *values, Size count) const
{
    if(!d->source)
    {
        throw File::InputError("DataBundle::get", "Source file has been destroyed");
    }
    d->source->as<ByteArrayFile>().get(at, values, count);
}

void DataBundle::set(Offset, Byte const *, Size)
{
    throw File::OutputError("DataBundle::set", "Classic data formats are read-only");
}

File *DataBundle::Interpreter::interpretFile(File *sourceData) const
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

            switch(fmt.format)
            {
            case Pk3:
                return new DataFolder(fmt.format, *sourceData);

            default:
                return new DataFile(fmt.format, *sourceData);
            }
        }
    }
    // Was not interpreted.
    return nullptr;
}
