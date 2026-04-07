/** @file wad.cpp  WAD Archive (file).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/filesys/wad.h"

#include "doomsday/doomsdayapp.h"
#include "doomsday/filesys/lumpcache.h"
#include <de/byteorder.h>
#include <de/nativepath.h>
#include <de/logbuffer.h>
#include <de/legacy/memoryzone.h>
#include <cstring> // memcpy

namespace res {
namespace internal {

struct FileHeader
{
    DE_ERROR(ReadError);

    Block identification; // 4 bytes
    dint32 lumpRecordsCount;
    dint32 lumpRecordsOffset;

    void operator << (FileHandle &from)
    {
        uint8_t buf[12];
        dsize readBytes = from.read(buf, 12);
        if (readBytes != 12) throw ReadError("FileHeader::operator << (FileHandle &)", "Source file is truncated");

        identification    = Block(buf, 4);
        lumpRecordsCount  = littleEndianByteOrder.toHost(*reinterpret_cast<dint32 *>(buf + 4));
        lumpRecordsOffset = littleEndianByteOrder.toHost(*reinterpret_cast<dint32 *>(buf + 8));
    }
};

struct IndexEntry
{
    DE_ERROR(ReadError);

    dint32 offset;
    dint32 size;
    Block name; // 8 bytes

    void operator << (FileHandle &from)
    {
        uint8_t buf[16];
        dsize readBytes = from.read(buf, 16);
        if (readBytes != 16) throw ReadError("IndexEntry::operator << (FileHandle &)", "Source file is truncated");

        name   = Block(buf + 8, 8);
        const dint32 *off = reinterpret_cast<const dint32 *>(buf);
        offset = littleEndianByteOrder.toHost(*off);
        size   = littleEndianByteOrder.toHost(*reinterpret_cast<const dint32 *>(buf + 4));
    }

    /// Perform all translations and encodings to the actual lump name.
    String nameNormalized() const
    {
        String normName;

        // Determine the actual length of the name.
        int nameLen = 0;
        while (nameLen < 8 && name[nameLen]) { nameLen++; }

        for (int i = 0; i < nameLen; ++i)
        {
            /// The Hexen demo on Mac uses the 0x80 on some lumps, maybe has significance?
            /// @todo Ensure that this doesn't break other IWADs. The 0x80-0xff
            ///       range isn't normally used in lump names, right??
            char ch = name[i] & 0x7f;
            normName += ch;
        }

        // WAD format allows characters not normally permitted in native paths.
        // To achieve uniformity we apply a percent encoding to the "raw" names.
        if (!normName.isEmpty())
        {
            normName = normName.toPercentEncoding();
        }
        else
        {
            /// Zero-length names are not considered valid - replace with *something*.
            /// @todo fixme: Handle this more elegantly...
            normName = "________";
        }

        // All lumps are ordained with an extension if they don't have one.
        if (normName.fileNameExtension().isEmpty())
        {
            normName += !normName.compareWithoutCase("DEHACKED")? ".deh" : ".lmp";
        }

        return normName;
    }
};

static String Wad_invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    String msg = Stringf("Invalid lump index %i ", invalidIdx);
    if (lastValidIdx < 0) msg += "(file is empty)";
    else                  msg += Stringf("(valid range: [0..%i])", lastValidIdx);
    return msg;
}

} // namespace internal

using namespace internal;

Wad::LumpFile::LumpFile(Entry &entry, FileHandle *hndl, String path,
    const FileInfo &info, File1 *container)
    : File1(hndl, path, info, container)
    , entry(entry)
{}

const String &Wad::LumpFile::name() const
{
    return directoryNode().name();
}

Uri Wad::LumpFile::composeUri(Char delimiter) const
{
    return directoryNode().path(delimiter);
}

PathTree::Node &Wad::LumpFile::directoryNode() const
{
    return entry;
}

size_t Wad::LumpFile::read(uint8_t *buffer, bool tryCache)
{
    return wad().readLump(info_.lumpIdx, buffer, tryCache);
}

size_t Wad::LumpFile::read(uint8_t *buffer, size_t startOffset, size_t length, bool tryCache)
{
    return wad().readLump(info_.lumpIdx, buffer, startOffset, length, tryCache);
}

const uint8_t *Wad::LumpFile::cache()
{
    return wad().cacheLump(info_.lumpIdx);
}

Wad::LumpFile &Wad::LumpFile::unlock()
{
    wad().unlockLump(info_.lumpIdx);
    return *this;
}

Wad &Wad::LumpFile::wad() const
{
    return container().as<Wad>();
}

DE_PIMPL_NOREF(Wad)
{
    LumpTree entries;                     ///< Directory structure and entry records for all lumps.
    std::unique_ptr<LumpCache> dataCache;  ///< Data payload cache.

    Impl() : entries(PathTree::MultiLeaf) {}
};

Wad::Wad(FileHandle &hndl, String path, const FileInfo &info, File1 *container)
    : File1(&hndl, path, info, container)
    , LumpIndex()
    , d(new Impl)
{
    LOG_AS("Wad");

    // Seek to the start of the header.
    handle_->seek(0, SeekSet);
    FileHeader hdr;
    hdr << *handle_;

    // Read the lump entries:
    if (hdr.lumpRecordsCount <= 0) return;

    // Seek to the start of the lump index.
    handle_->seek(hdr.lumpRecordsOffset, SeekSet);
    for (int i = 0; i < hdr.lumpRecordsCount; ++i)
    {
        IndexEntry lump;
        lump << *handle_;

        // Determine the name for this lump in the VFS.
        String absPath = String(DoomsdayApp::app().doomsdayBasePath()) / lump.nameNormalized();

        // Make an index entry for this lump.
        Entry &entry = d->entries.insert(absPath);

        entry.offset = lump.offset;
        entry.size   = lump.size;

        LumpFile *lumpFile = new LumpFile(entry, nullptr, entry.path(),
                                          FileInfo(lastModified(), // Inherited from the container (note recursion).
                                                   i, entry.offset, entry.size, entry.size),
                                          this);
        entry.lumpFile.reset(lumpFile); // takes ownership

        catalogLump(*lumpFile);
    }
}

Wad::~Wad()
{}

void Wad::clearCachedLump(int lumpIndex, bool *retCleared)
{
    LOG_AS("Wad::clearCachedLump");

    if (retCleared) *retCleared = false;

    if (hasLump(lumpIndex))
    {
        if (d->dataCache)
        {
            d->dataCache->remove(lumpIndex, retCleared);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(Wad_invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

void Wad::clearLumpCache()
{
    LOG_AS("Wad::clearLumpCache");
    if (d->dataCache)
    {
        d->dataCache->clear();
    }
}

const uint8_t *Wad::cacheLump(int lumpIndex)
{
    LOG_AS("Wad::cacheLump");

    const LumpFile &lumpFile = static_cast<LumpFile &>(lump(lumpIndex));
    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s)",
               NativePath(composePath()).pretty()
            << NativePath(lumpFile.composePath()).pretty()
            << lumpFile.info().size
            << (lumpFile.info().isCompressed()? ", compressed" : ""));

    // Time to create the cache?
    if (!d->dataCache)
    {
        d->dataCache.reset(new LumpCache(LumpIndex::size()));
    }

    const uint8_t *data = d->dataCache->data(lumpIndex);
    if (data) return data;

    uint8_t *region = (uint8_t *) Z_Malloc(lumpFile.info().size, PU_APPSTATIC, 0);
    if (!region)
        throw Error("Wad::cacheLump",
                    stringf("Failed on allocation of %zu bytes for cache copy of lump #%i",
                            lumpFile.info().size,
                            lumpIndex));

    readLump(lumpIndex, region, false);
    d->dataCache->insert(lumpIndex, region);

    return region;
}

void Wad::unlockLump(int lumpIndex)
{
    LOG_AS("Wad::unlockLump");
    LOGDEV_RES_XVERBOSE("\"%s:%s\"",
                        NativePath(composePath()).pretty() <<
                        NativePath(lump(lumpIndex).composePath()).pretty());

    if (hasLump(lumpIndex))
    {
        if (d->dataCache)
        {
            d->dataCache->unlock(lumpIndex);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(Wad_invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

size_t Wad::readLump(int lumpIndex, uint8_t *buffer, bool tryCache)
{
    LOG_AS("Wad::readLump");
    return readLump(lumpIndex, buffer, 0, lump(lumpIndex).size(), tryCache);
}

size_t Wad::readLump(int lumpIndex, uint8_t *buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("Wad::readLump");

    const LumpFile &lumpFile = static_cast<LumpFile &>(lump(lumpIndex));
    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s) [%u +%u]",
                        NativePath(composePath()).pretty()
                        << NativePath(lumpFile.composePath()).pretty()
                        << lumpFile.size()
                        << (lumpFile.isCompressed()? ", compressed" : "")
                        << startOffset
                        << length);

    // Try to avoid a file system read by checking for a cached copy.
    if (tryCache)
    {
        const uint8_t *data = (d->dataCache ? d->dataCache->data(lumpIndex) : 0);
        LOGDEV_RES_XVERBOSE("Cache %s on #%i", (data? "hit" : "miss") << lumpIndex);
        if (data)
        {
            size_t readBytes = de::min(size_t(lumpFile.size()), length);
            std::memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    handle_->seek(lumpFile.info().baseOffset + startOffset, SeekSet);
    size_t readBytes = handle_->read(buffer, length);

    /// @todo Do not check the read length here.
    if (readBytes < length)
        throw Error(
            "Wad::readLumpSection",
            stringf("Only read %zu of %zu bytes of lump #%i", readBytes, length, lumpIndex));

    return readBytes;
}

uint Wad::calculateCRC()
{
    uint crc = 0;
    for (File1 *file : allLumps())
    {
        Entry &entry = static_cast<Entry &>(file->as<LumpFile>().directoryNode());
        entry.update();
        crc += entry.crc;
    }
    return crc;
}

bool Wad::recognise(FileHandle &file)
{
    // Seek to the start of the header.
    size_t initPos = file.tell();
    file.seek(0, SeekSet);

    // Attempt to read the header.
    bool readOk = false;
    FileHeader hdr;
    try
    {
        hdr << file;
        readOk = true;
    }
    catch (const FileHeader::ReadError &)
    {} // Ignore

    // Return the stream to its original position.
    file.seek(initPos, SeekSet);

    if (!readOk) return false;

    return (hdr.identification == "IWAD" || hdr.identification == "PWAD");
}

const Wad::LumpTree &Wad::lumpTree() const
{
    return d->entries;
}

Wad::LumpFile &Wad::Entry::file() const
{
    DE_ASSERT(lumpFile);
    return *lumpFile;
}

void Wad::Entry::update()
{
    crc = uint(file().size());
    const String lumpName = Node::name();
    for (Char ch : lumpName)
    {
        crc += uint(ch);
    }
}

} // namespace res
