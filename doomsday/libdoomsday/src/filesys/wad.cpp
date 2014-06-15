/** @file wad.cpp  WAD Archive (file).
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "doomsday/filesys/lumpcache.h"
#include "doomsday/filesys/fs_util.h"

#include <vector>
#include <cstring> // memcpy
#include <de/ByteOrder>
#include <de/Error>
#include <de/NativePath>
#include <de/Log>
#include <de/memoryzone.h>

static QString invalidIndexMessage(int invalidIdx, int lastValidIdx);

namespace de {
namespace internal {

/// The following structures are used to read data directly from WAD files.
#pragma pack(1)
typedef struct {
    char identification[4];
    int32_t lumpRecordsCount;
    int32_t lumpRecordsOffset;
} wadheader_t;

typedef struct {
    int32_t filePos;
    int32_t size;
    char name[8];
} wadlumprecord_t;
#pragma pack()

static QString invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    QString msg = QString("Invalid lump index %1 ").arg(invalidIdx);
    if(lastValidIdx < 0) msg += "(file is empty)";
    else                 msg += QString("(valid range: [0..%2])").arg(lastValidIdx);
    return msg;
}

/// @return Length of the archived lump name in characters.
static int nameLength(wadlumprecord_t const &lrec)
{
    int nameLen = 0;
    while(nameLen < LUMPNAME_T_LASTINDEX && lrec.name[nameLen])
    { nameLen++; }
    return nameLen;
}

/// Perform all translations and encodings to the archived lump name and write
/// the result to @a normName.
static void normalizeLumpName(wadlumprecord_t const &lrec, ddstring_t *normName)
{
    if(!normName) return;

    Str_Clear(normName);
    int nameLen = nameLength(lrec);
    for(int i = 0; i < nameLen; ++i)
    {
        /// The Hexen demo on Mac uses the 0x80 on some lumps, maybe has significance?
        /// @todo Ensure that this doesn't break other IWADs. The 0x80-0xff
        ///       range isn't normally used in lump names, right??
        char ch = lrec.name[i] & 0x7f;
        Str_AppendChar(normName, ch);
    }

    // Lump names allow characters the file system does not. Therefore they
    // will be percent-encoded here and later decoded if/when necessary.
    if(!Str_IsEmpty(normName))
    {
        Str_PercentEncode(normName);
    }
    else
    {
        /// We do not consider zero-length names to be valid, so replace with
        /// with _something_.
        /// @todo fixme: Handle this more elegantly...
        Str_Set(normName, "________");
    }

    // All lumps are ordained with an extension if they don't have one.
    char const *ext = F_FindFileExtension(Str_Text(normName));
    if(!(ext && Str_Length(normName) > ext - Str_Text(normName) + 1))
    {
        Str_Append(normName, !Str_CompareIgnoreCase(normName, "DEHACKED")? ".deh" : ".lmp");
    }
}

} // namespace internal

using namespace internal;

Wad::LumpFile::LumpFile(FileHandle &hndl, String path, FileInfo const &info, File1 *container)
    : File1(hndl, path, info, container)
{}

String const &Wad::LumpFile::name() const
{
    return directoryNode().name();
}

Uri Wad::LumpFile::composeUri(QChar delimiter) const
{
    return directoryNode().path(delimiter);
}

PathTree::Node const &Wad::LumpFile::directoryNode() const
{
    return wad().lumpEntry(info_.lumpIdx);
}

size_t Wad::LumpFile::read(uint8_t *buffer, bool tryCache)
{
    return wad().readLump(info_.lumpIdx, buffer, tryCache);
}

size_t Wad::LumpFile::read(uint8_t *buffer, size_t startOffset, size_t length, bool tryCache)
{
    return wad().readLump(info_.lumpIdx, buffer, startOffset, length, tryCache);
}

uint8_t const *Wad::LumpFile::cache()
{
    return wad().cacheLump(info_.lumpIdx);
}

Wad::LumpFile &Wad::LumpFile::unlock()
{
    wad().unlockLump(info_.lumpIdx);
    return *this;
}

Wad &Wad::LumpFile::wad() const {
    return container().as<Wad>();
}

DENG2_PIMPL(Wad)
{
    int arcRecordsCount;                  ///< Number of lump records in the archived wad.
    size_t arcRecordsOffset;              ///< Offset to the lump record table in the archived wad.
    QScopedPointer<LumpTree> entries;     ///< Directory structure and entry records for all lumps.

    /// LUT which maps logical lump indices to Entry structures.
    typedef std::vector<Entry *> LumpEntryLut;
    QScopedPointer<LumpEntryLut> lumpEntryLut;

    QScopedPointer<LumpCache> lumpCache;  ///< Data payload cache.

    Instance(Public *i, FileHandle &file, String path)
        : Base(i)
        , arcRecordsCount (0)
        , arcRecordsOffset(0)
    {
        // Seek to the start of the header.
        file.seek(0, SeekSet);

        wadheader_t hdr;
        if(!readArchiveHeader(file, hdr))
            throw Wad::FormatError("Wad::Wad", QString("File %1 does not appear to be a known WAD format").arg(path));

        arcRecordsCount  = hdr.lumpRecordsCount;
        arcRecordsOffset = hdr.lumpRecordsOffset;
    }

    /// @pre @a file is positioned at the start of the header.
    static bool readArchiveHeader(FileHandle &file, wadheader_t &hdr)
    {
        size_t readBytes = file.read((uint8_t *)&hdr, sizeof(wadheader_t));
        if(!(readBytes < sizeof(wadheader_t)))
        {
            hdr.lumpRecordsCount  = littleEndianByteOrder.toNative(hdr.lumpRecordsCount);
            hdr.lumpRecordsOffset = littleEndianByteOrder.toNative(hdr.lumpRecordsOffset);
            return true;
        }
        return false;
    }

    void readLumpEntries()
    {
        LOG_AS("Wad");

        if(arcRecordsCount <= 0) return;

        // Already been here?
        if(!entries.isNull()) return;

        // We'll load the lump directory using one continous read into a temporary
        // local buffer before we process it into our runtime representation.
        wadlumprecord_t *arcRecords = new wadlumprecord_t[arcRecordsCount];
        self.handle_->seek(arcRecordsOffset, SeekSet);
        self.handle_->read((uint8_t *)arcRecords, arcRecordsCount * sizeof(*arcRecords));

        // Reserve a small work buffer for processing archived lump names.
        ddstring_t absPath;
        Str_Reserve(Str_Init(&absPath), LUMPNAME_T_LASTINDEX + 4/*.lmp*/);

        // Intialize the directory.
        entries.reset(new LumpTree(PathTree::MultiLeaf));

        // Build our runtime representation from the archived lump directory.
        wadlumprecord_t const *arcRecord = arcRecords;
        for(int i = 0; i < arcRecordsCount; ++i, arcRecord++)
        {
            // Determine the name for this lump in the VFS.
            normalizeLumpName(*arcRecord, &absPath);

            // Make it absolute.
            F_PrependBasePath(&absPath, &absPath);

            String const path = Str_Text(&absPath);
            Entry &entry      = entries->insert(Path(path));

            FileHandle *dummy = 0; /// @todo Fixme!
            LumpFile *lumpFile = new LumpFile(*dummy, path,
                                              FileInfo(self.lastModified(), // Inherited from the container (note recursion).
                                                       i,
                                                       littleEndianByteOrder.toNative(arcRecord->filePos),
                                                       littleEndianByteOrder.toNative(arcRecord->size),
                                                       littleEndianByteOrder.toNative(arcRecord->size)),
                                              thisPublic);
            entry.lumpFile.reset(lumpFile); // takes ownership
        }

        Str_Free(&absPath);

        // We are finished with the temporary lump directory records.
        delete[] arcRecords;
    }

    void buildLumpEntryLutIfNeeded()
    {
        LOG_AS("Wad");
        // Been here already?
        if(!lumpEntryLut.isNull()) return;

        lumpEntryLut.reset(new LumpEntryLut(self.lumpCount()));
        if(entries.isNull()) return;

        for(PathTreeIterator<LumpTree> iter(entries->leafNodes()); iter.hasNext(); )
        {
            Entry &entry = iter.next();
            (*lumpEntryLut)[entry.file().info().lumpIdx] = &entry;
        }
    }
};

Wad::Wad(FileHandle &hndl, String path, FileInfo const &info, File1 *container)
    : File1(hndl, path, info, container)
    , d(new Instance(this, hndl, path))
{}

bool Wad::isValidIndex(int lumpIdx) const
{
    return lumpIdx >= 0 && lumpIdx < lumpCount();
}

int Wad::lastIndex() const
{
    return lumpCount() - 1;
}

int Wad::lumpCount() const
{
    d->readLumpEntries();
    return d->entries.isNull()? 0 : d->entries->size();
}

Wad::Entry &Wad::lumpEntry(int lumpIndex) const
{
    LOG_AS("Wad");
    if(isValidIndex(lumpIndex))
    {
        d->buildLumpEntryLutIfNeeded();
        return *(*d->lumpEntryLut)[lumpIndex];
    }
    /// @throw NotFoundEntry  The lump index given is out of range.
    throw NotFoundError("Wad::entry", invalidIndexMessage(lumpIndex, lastIndex()));
}

void Wad::clearCachedLump(int lumpIndex, bool *retCleared)
{
    LOG_AS("Wad::clearCachedLump");

    if(retCleared) *retCleared = false;

    if(isValidIndex(lumpIndex))
    {
        if(!d->lumpCache.isNull())
        {
            d->lumpCache->remove(lumpIndex, retCleared);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

void Wad::clearLumpCache()
{
    LOG_AS("Wad::clearLumpCache");
    if(!d->lumpCache.isNull())
    {
        d->lumpCache->clear();
    }
}

uint8_t const *Wad::cacheLump(int lumpIndex)
{
    LOG_AS("Wad::cacheLump");

    if(!isValidIndex(lumpIndex)) throw NotFoundError("Wad::cacheLump", invalidIndexMessage(lumpIndex, lastIndex()));

    LumpFile const &file = lumpEntry(lumpIndex).file();
    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s)")
            << NativePath(composePath()).pretty()
            << NativePath(file.composePath()).pretty()
            << (unsigned long) file.info().size
            << (file.info().isCompressed()? ", compressed" : "");

    // Time to create the cache?
    if(d->lumpCache.isNull())
    {
        d->lumpCache.reset(new LumpCache(lumpCount()));
    }

    uint8_t const *data = d->lumpCache->data(lumpIndex);
    if(data) return data;

    uint8_t *region = (uint8_t *) Z_Malloc(file.info().size, PU_APPSTATIC, 0);
    if(!region) throw Error("Wad::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(file.info().size).arg(lumpIndex));

    readLump(lumpIndex, region, false);
    d->lumpCache->insert(lumpIndex, region);

    return region;
}

void Wad::unlockLump(int lumpIndex)
{
    LOG_AS("Wad::unlockLump");
    LOGDEV_RES_XVERBOSE("\"%s:%s\"")
            << NativePath(composePath()).pretty()
            << NativePath(lumpEntry(lumpIndex).file().composePath()).pretty();

    if(isValidIndex(lumpIndex))
    {
        if(!d->lumpCache.isNull())
        {
            d->lumpCache->unlock(lumpIndex);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

size_t Wad::readLump(int lumpIndex, uint8_t *buffer, bool tryCache)
{
    LOG_AS("Wad::readLump");
    if(!isValidIndex(lumpIndex)) return 0;
    return readLump(lumpIndex, buffer, 0, lumpEntry(lumpIndex).file().size(), tryCache);
}

size_t Wad::readLump(int lumpIdx, uint8_t *buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("Wad::readLump");
    LumpFile const &file = static_cast<LumpFile &>(lump(lumpIdx));

    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s) [%u +%u]")
            << NativePath(composePath()).pretty()
            << NativePath(file.composePath()).pretty()
            << (unsigned long) file.size()
            << (file.isCompressed()? ", compressed" : "")
            << startOffset
            << length;

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache)
    {
        uint8_t const *data = (!d->lumpCache.isNull() ? d->lumpCache->data(lumpIdx) : 0);
        LOGDEV_RES_XVERBOSE("Cache %s on #%i") << (data? "hit" : "miss") << lumpIdx;
        if(data)
        {
            size_t readBytes = MIN_OF(file.size(), length);
            std::memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    handle_->seek(file.info().baseOffset + startOffset, SeekSet);
    size_t readBytes = handle_->read(buffer, length);

    /// @todo Do not check the read length here.
    if(readBytes < length)
        throw Error("Wad::readLumpSection", QString("Only read %1 of %2 bytes of lump #%3").arg(readBytes).arg(length).arg(lumpIdx));

    return readBytes;
}

uint Wad::calculateCRC()
{
    uint crc = 0;
    int const numLumps = lumpCount();
    for(int i = 0; i < numLumps; ++i)
    {
        Entry &entry = lumpEntry(i);

        entry.crc = uint(entry.file().size());
        String const lumpName = entry.name();
        int const nameLen = lumpName.length();
        for(int i = 0; i < nameLen; ++i) {
            entry.crc += lumpName.at(i).unicode();
        }

        crc += entry.crc;
    }
    return crc;
}

bool Wad::recognise(FileHandle &file)
{
    wadheader_t hdr;

    // Seek to the start of the header.
    size_t initPos = file.tell();
    file.seek(0, SeekSet);

    bool readHeaderOk = Wad::Instance::readArchiveHeader(file, hdr);

    // Return the stream to its original position.
    file.seek(initPos, SeekSet);

    if(!readHeaderOk) return false;
    if(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)) return false;
    return true;
}

Wad::LumpTree const &Wad::lumps() const
{
    d->readLumpEntries();
    return *d->entries;
}

} // namespace de
