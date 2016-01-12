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

#include "doomsday/resource/databundle.h"
#include "doomsday/filesys/datafolder.h"
#include "doomsday/filesys/datafile.h"
#include "doomsday/resource/bundles.h"
#include "doomsday/resource/resources.h"
#include "doomsday/resource/lumpdirectory.h"
#include "doomsday/doomsdayapp.h"

#include <de/ArchiveFeed>
//#include <de/File>
#include <de/Info>
#include <de/Package>
#include <QRegExp>

using namespace de;

namespace internal
{
    static char const *formatDescriptions[] =
    {
        "unknown",
        "PK3 archive",
        "WAD file",
        "IWAD file",
        "PWAD file",
        "data lump",
        "Doomsday Engine definitions",
        "DeHackEd patch"
    };
}

DENG2_PIMPL(DataBundle)
{
    SafePtr<File> source;
    Format format;
    String packageId; // linked under /sys/bundles/
    std::unique_ptr<res::LumpDirectory> lumpDir;

    Instance(Public *i, Format fmt) : Base(i), format(fmt)
    {}

    void identify()
    {
        using Info = de::Info;

        DENG2_ASSERT(packageId.isEmpty()); // should be only called once
        if(!packageId.isEmpty()) return;

        // Load the lump directory of WAD files.
        if(format == Wad || format == Pwad || format == Iwad)
        {
            qDebug() << "[DataBundle] Checking" << source->description();

            lumpDir.reset(new res::LumpDirectory(source->as<ByteArrayFile>()));
            if(!lumpDir->isValid())
            {
                throw FormatError("DataBundle::identify",
                                  dynamic_cast<File *>(thisPublic)->description() +
                                  ": WAD file lump directory not found");
            }

            format = (lumpDir->type() == res::LumpDirectory::Pwad? Pwad : Iwad);

            qDebug() << "format:" << (lumpDir->type()==res::LumpDirectory::Pwad? "PWAD" : "IWAD")
                     << "\nfileName:" << source->name()
                     << "\nfileSize:" << source->size()
                     << "\nlumpDirCRC32:" << QString::number(lumpDir->crc32(), 16).toLatin1();
        }

        Info::BlockElement const *bestMatch = nullptr;
        String packageId;
        Version packageVersion;
        int bestScore = 0;

        // Find the best match from the registry.
        for(auto const *def : DoomsdayApp::bundles().formatEntries(format))
        {
            int score = 0;

            // Match the file name.
            if(auto const *fileName = def->find(QStringLiteral("fileName")))
            {
                if(fileName->isKey() &&
                   fileName->as<Info::KeyElement>().value()
                        .text.compareWithoutCase(source->name()) == 0)
                {
                    ++score;
                }
                else if(fileName->isList())
                {
                    // Any of the provided alternatives will be accepted.
                    for(auto const &cand : fileName->as<Info::ListElement>().values())
                    {
                        if(!cand.text.compareWithoutCase(source->name()))
                        {
                            ++score;
                            break;
                        }
                    }
                }
            }

            // Match the file size.
            String fileSize = def->keyValue(QStringLiteral("fileSize")).text;
            if(!fileSize.isEmpty() && fileSize.toUInt() == source->size())
            {
                ++score;
            }

            bool crcMismatch = false;

            // Additional criteria for recognizing WADs.
            if(format == Iwad || format == Pwad)
            {
                String lumpDirCRC32 = def->keyValue(QStringLiteral("lumpDirCRC32")).text;
                if(!lumpDirCRC32.isEmpty())
                {
                    if(lumpDirCRC32.toUInt(nullptr, 16) == lumpDir->crc32())
                    {
                        // Low probability of a false negative => more significant.
                        score += 2;
                    }
                    else
                    {
                        crcMismatch = true;
                    }
                }

                if(auto const *lumps = def->find(QStringLiteral("lumps"))->maybeAs<Info::ListElement>())
                {
                    ++score; // will be subtracted if not matched

                    for(auto const &val : lumps->values())
                    {
                        QRegExp const sizeCondition("(.*)==([0-9]+)");
                        Block lumpName;
                        int requiredSize = 0;

                        if(sizeCondition.exactMatch(val.text))
                        {
                            lumpName     = sizeCondition.cap(1).toUtf8();
                            requiredSize = sizeCondition.cap(2).toInt();
                        }
                        else
                        {
                            lumpName     = val.text.toUtf8();
                            requiredSize = -1;
                        }

                        if(!lumpDir->has(lumpName))
                        {
                            --score;
                            break;
                        }

                        if(requiredSize >= 0 && lumpDir->lumpSize(lumpName) != duint32(requiredSize))
                        {
                            --score;
                            break;
                        }
                    }
                }
            }

            if(score > 0 && score >= bestScore)
            {
                bestMatch = def;
                bestScore = score;

                auto idVer = Package::split(def->name());
                packageId = idVer.first;
                // If the specified CRC32 doesn't match, we can't be certain of
                // which version this actually is.
                packageVersion = (!crcMismatch? idVer.second : Version(""));
            }
        }

        qDebug() << "\n" << dynamic_cast<File *>(thisPublic)->path();

        if(bestMatch)
        {
            qDebug() << "  " << packageId
                     << packageVersion.asText()
                     << internal::formatDescriptions[format]
                     << "score:" << bestScore;
        }
        else
        {
            qDebug() << "  Not matched!";
        }
        qDebug() << "\n";
    }
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

void DataBundle::setFormat(Format format)
{
    d->format = format;
}

void DataBundle::identifyPackages() const
{
    d->identify();
}

bool DataBundle::isNested() const
{
    return containerBundle() != nullptr;
}

DataBundle const *DataBundle::containerBundle() const
{
    auto const *file = dynamic_cast<File const *>(this);
    DENG2_ASSERT(file != nullptr);

    for(Folder const *folder = file->parent(); folder; folder = folder->parent())
    {
        if(auto const *data = folder->maybeAs<DataFolder>())
            return data;
    }
    return nullptr;
}

res::LumpDirectory const *DataBundle::lumpDirectory() const
{
    return d->lumpDir.get();
}

File *DataBundle::Interpreter::interpretFile(File *sourceData) const
{
    // Naive check using the file extension.
    static struct { String str; Format format; } formats[] = {
        { ".pk3", Pk3 },
        { ".wad", Wad /* I/P checked later */ },
        { ".lmp", Lump },
        { ".ded", Ded },
        { ".deh", Dehacked }
    };
    String const ext = sourceData->name().fileNameExtension();
    for(auto const &fmt : formats)
    {
        if(!ext.compareWithoutCase(fmt.str))
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
