/** @file lumpdirectory.cpp
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

#include "doomsday/resource/lumpdirectory.h"

#include <de/Reader>
#include <de/mathutil.h>
#include <QList>
#include <QRegularExpression>

using namespace de;

namespace res {

dsize const LumpDirectory::InvalidPos = dsize(-1);

static QRegularExpression const regExMy ("^E[1-9]M[1-9]$");
static QRegularExpression const regMAPxx("^MAP[0-9][0-9]$");

DENG2_PIMPL_NOREF(LumpDirectory)
{
    Type type = Invalid;
    MapType mapType = None;
    duint32 crc = 0;
    QList<Entry> entries;
    QHash<QByteArray, int> index; // points to entries

    void read(IByteArray const &source)
    {
        Reader reader(source);

        // Verify the magic.
        Block typeId;
        reader.readBytes(4, typeId);
        type = (typeId == "PWAD"? Pwad : typeId == "IWAD"? Iwad : Invalid);
        if (type == Invalid) return;

        duint32 count = 0;
        duint32 dirOffset = 0;
        reader >> count >> dirOffset;

        // Read the directory into memory.
        Block data;
        reader.setOffset(dirOffset);
        reader.readBytes(16 * count, data);

        crc = M_CRC32(data.dataConst(), uint(data.size()));

        // Read all the entries.
        Reader lumpReader(data);
        for (duint32 i = 0; i < count; ++i)
        {
            Entry entry;
            lumpReader >> entry.offset >> entry.size;
            lumpReader.readBytes(8, entry.name);
            entry.name.resize(qstrlen(entry.name)); // QByteArray is zero-terminated
            entries.append(entry);
        }

        // Make an index of all the lumps.
        index.clear();
        for (int i = 0; i < entries.size(); ++i)
        {
            index.insert(entries.at(i).name, i);

            // If there are a map lumps, check which kind it is.
            if (mapType == None)
            {
                String const lumpName = String::fromLatin1(entries.at(i).name);
                if (regExMy.match(lumpName).hasMatch())
                {
                    mapType = ExMy;
                }
                else if (regMAPxx.match(lumpName).hasMatch())
                {
                    mapType = MAPxx;
                }
            }
        }
    }
};

LumpDirectory::LumpDirectory(IByteArray const &wadData)
    : d(new Impl)
{
    d->read(wadData);
}

bool LumpDirectory::isValid() const
{
    return d->type != Invalid;
}

LumpDirectory::Type LumpDirectory::type() const
{
    return d->type;
}

LumpDirectory::MapType LumpDirectory::mapType() const
{
    return d->mapType;
}

LumpDirectory::Pos LumpDirectory::count() const
{
    return Pos(d->entries.size());
}

LumpDirectory::Entry const &LumpDirectory::entry(Pos pos) const
{
    if (pos >= count())
    {
        throw OffsetError("LumpDirectory::entry",
                          QString("Invalid position %1 (lump count: %2)")
                          .arg(pos).arg(count()));
    }
    return d->entries.at(int(pos));
}

duint32 LumpDirectory::crc32() const
{
    return d->crc;
}

duint32 LumpDirectory::lumpSize(Block const &lumpName) const
{
    auto found = d->index.constFind(lumpName);
    if (found != d->index.constEnd())
    {
        return d->entries.at(found.value()).size;
    }
    return 0;
}

bool LumpDirectory::has(Block const &lumpName) const
{
    return d->index.contains(lumpName);
}

LumpDirectory::Pos LumpDirectory::find(Block const &lumpName) const
{
    auto found = d->index.constFind(lumpName);
    if (found != d->index.constEnd())
    {
        return Pos(found.value());
    }
    return InvalidPos;
}

QList<LumpDirectory::Pos> res::LumpDirectory::findMaps() const
{
    QList<LumpDirectory::Pos> maps;
    if (d->mapType != None)
    {
        for (auto i = d->index.constBegin(); i != d->index.constEnd(); ++i)
        {
            String const name = QString::fromLatin1(i.key());
            if (regMAPxx.match(name).hasMatch() ||
                regExMy .match(name).hasMatch())
            {
                maps << Pos(i.value());
            }
        }
    }
    return maps;
}

StringList LumpDirectory::findMapLumpNames() const
{
    StringList maps;
    foreach (auto pos, findMaps())
    {
        maps << String::fromLatin1(entry(pos).name);
    }
    qSort(maps);
    return maps;
}

StringList LumpDirectory::mapsInContiguousRangesAsText() const
{
    StringList mapRanges;
    auto const maps = findMapLumpNames();
    static StringList const prefixes({ "MAP", "E1M", "E2M", "E3M", "E4M", "E5M", "E6M",
                                       "E7M", "E8M", "E9M" });

    foreach (String const &prefix, prefixes)
    {
        QList<int> numbers;
        foreach (String const &map, maps)
        {
            if (map.startsWith(prefix))
            {
                int number = map.substr(3).toInt(nullptr, 10);
                numbers.append(number);
            }
        }

        auto mapNumberText = [] (int num, int fieldWidth) {
            return QString("%1").arg(num, fieldWidth, 10, QChar('0'));
        };
        int const numFieldWidth = (prefix == QStringLiteral("MAP")? 2 : 0);

        foreach (Rangei range, Rangei::findContiguousRanges(numbers))
        {
            String mr = prefix + mapNumberText(range.start, numFieldWidth);
            if (range.size() > 1)
            {
                mr += QString("-%1").arg(mapNumberText(range.end - 1, numFieldWidth));
            }
            mapRanges << mr;
        }
    }

    return mapRanges;
}

} // namespace res
