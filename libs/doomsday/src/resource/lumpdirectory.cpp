/** @file lumpdirectory.cpp
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

#include "doomsday/res/lumpdirectory.h"

#include <de/legacy/mathutil.h>
#include <de/metadatabank.h>
#include <de/reader.h>
#include <de/regexp.h>

using namespace de;

namespace res {

const dsize LumpDirectory::InvalidPos = std::numeric_limits<dsize>::max();

DE_STATIC_STRING(CACHE_CATEGORY, "LumpDirectory");

static const RegExp regExMy ("^E[1-9]M[1-9]$");
static const RegExp regMAPxx("^MAP[0-9][0-9]$");
static const Block  flatMarkers[4] = {"FF_START", "FF_END", "F_START", "F_END"};

DE_PIMPL_NOREF(LumpDirectory), public ISerializable
{
    Type             type    = Invalid;
    MapType          mapType = None;
    duint32          crc     = 0;
    List<Entry>      entries;
    Hash<Block, int> index; // points to entries

    void read(const IByteArray &source)
    {
        // First check the metadata cache if we already have this directory.
        if (const File *file = dynamic_cast<const File *>(&source))
        {
            if (readFromCache(file->metaId())) return;
        }

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

        crc = crc32_Block(data);

        // Read all the entries.
        Reader lumpReader(data);
        for (duint32 i = 0; i < count; ++i)
        {
            Entry entry;
            lumpReader >> entry.offset >> entry.size;
            lumpReader.readBytes(8, entry.name);
            entry.name.resize(strlen(entry.name.c_str())); // Block is zero-terminated
            entries.append(entry);
        }

        // Make an index of all the lumps.
        index.clear();
        for (int i = 0; i < entries.sizei(); ++i)
        {
            index.insert(entries.at(i).name, i);

            // If there are a map lumps, check which kind it is.
            if (mapType == None)
            {
                const String lumpName = String::fromLatin1(entries.at(i).name);
                if (regExMy.hasMatch(lumpName))
                {
                    mapType = ExMy;
                }
                else if (regMAPxx.hasMatch(lumpName))
                {
                    mapType = MAPxx;
                }
            }
        }

        if (const File *file = dynamic_cast<const File *>(&source))
        {
            updateCache(file->metaId());
        }
    }

    bool readFromCache(const Block &id)
    {
        try
        {
            if (const Block data = MetadataBank::get().check(CACHE_CATEGORY(), id))
            {
                // We're in luck.
                Reader reader(data);
                reader.withHeader() >> *this;

                // Update the name lookup.
                for (int i = 0; i < entries.sizei(); ++i)
                {
                    index.insert(entries.at(i).name, i);
                }
                return true;
            }
        }
        catch (const Error &er)
        {
            LOGDEV_RES_WARNING("Corrupt cached metadata: %s") << er.asText();
        }
        return false;
    }

    void updateCache(const Block &id)
    {
        Block data;
        Writer writer(data);
        writer.withHeader() << *this;
        MetadataBank::get().setMetadata(CACHE_CATEGORY(), id, data);
    }

    void operator >> (Writer &to) const override
    {
        to << duint8(type) << duint8(mapType) << crc;
        to.writeElements(entries);
    }

    void operator << (Reader &from) override
    {
        entries.clear();
        from.readAs<duint8>(type).readAs<duint8>(mapType) >> crc;
        from.readElements(entries);
    }
};

LumpDirectory::LumpDirectory(const IByteArray &wadData)
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

const LumpDirectory::Entry &LumpDirectory::entry(Pos pos) const
{
    if (pos >= count())
    {
        throw OffsetError("LumpDirectory::entry",
                          stringf("Invalid position %zu (lump count: %zu)",
                          pos, count()));
    }
    return d->entries.at(int(pos));
}

duint32 LumpDirectory::crc32() const
{
    return d->crc;
}

duint32 LumpDirectory::lumpSize(const Block &lumpName) const
{
    auto found = d->index.find(lumpName);
    if (found != d->index.end())
    {
        return d->entries.at(found->second).size;
    }
    return 0;
}

bool LumpDirectory::has(const Block &lumpName) const
{
    return d->index.contains(lumpName);
}

LumpDirectory::Pos LumpDirectory::find(const Block &lumpName) const
{
    auto found = d->index.find(lumpName);
    if (found != d->index.end())
    {
        return Pos(found->second);
    }
    return InvalidPos;
}

List<LumpDirectory::Pos> LumpDirectory::findAll(const Block &lumpName) const
{
    List<LumpDirectory::Pos> found;
    for (int i = d->entries.sizei() - 1; i >= 0; --i)
    {
        if (d->entries.at(i).name == lumpName)
        {
            found.push_back(i);
        }
    }
    return found;
}

List<LumpDirectory::Range> LumpDirectory::findRanges(RangeType rangeType) const
{
    List<Range> ranges;
    if (rangeType == Flats)
    {
        auto start = find(flatMarkers[0]);
        auto end   = find(flatMarkers[1]);
        if (start != InvalidPos && end != InvalidPos)
        {
            ranges << Range{start + 1, end};
        }

        start = find(flatMarkers[2]);
        end   = find(flatMarkers[3]);
        if (start != InvalidPos && end != InvalidPos)
        {
            ranges << Range{start + 1, end};
        }
    }
    return ranges;
}

List<LumpDirectory::Pos> res::LumpDirectory::findMaps() const
{
    List<LumpDirectory::Pos> maps;
    if (d->mapType != None)
    {
        for (auto i = d->index.begin(); i != d->index.end(); ++i)
        {
            const String &name = i->first;
            if (regMAPxx.hasMatch(name) || regExMy.hasMatch(name))
            {
                maps << Pos(i->second);
            }
        }
    }
    return maps;
}

StringList LumpDirectory::findMapLumpNames() const
{
    StringList maps;
    for (const auto &pos : findMaps())
    {
        maps << String::fromLatin1(entry(pos).name);
    }
    maps.sort();
    return maps;
}

StringList LumpDirectory::mapsInContiguousRangesAsText() const
{
    StringList mapRanges;
    const auto maps = findMapLumpNames();
    static const StringList prefixes(
        {"MAP", "E1M", "E2M", "E3M", "E4M", "E5M", "E6M", "E7M", "E8M", "E9M"});

    for (const String &prefix : prefixes)
    {
        List<int> numbers;
        for (const String &map : maps)
        {
            if (map.beginsWith(prefix))
            {
                int number = map.substr(BytePos(3)).toInt(nullptr, 10);
                numbers.append(number);
            }
        }

        auto mapNumberText = [] (int num, int fieldWidth) {
            return Stringf(fieldWidth == 2? "%02i" : "%i", num);
        };
        const int numFieldWidth = (prefix == DE_STR("MAP")? 2 : 0);

        for (const Rangei &range : Rangei::findContiguousRanges(numbers))
        {
            String mr = prefix + mapNumberText(range.start, numFieldWidth);
            if (range.size() > 1)
            {
                mr += "-";
                mr += mapNumberText(range.end - 1, numFieldWidth);
            }
            mapRanges << mr;
        }
    }

    return mapRanges;
}

//---------------------------------------------------------------------------------------

void LumpDirectory::Entry::operator >> (Writer &to) const
{
    to << name << offset << size;
}

void LumpDirectory::Entry::operator << (Reader &from)
{
    from >> name >> offset >> size;
}

} // namespace res
