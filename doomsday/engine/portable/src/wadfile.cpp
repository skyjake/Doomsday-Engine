/**
 * @file wadfile.cpp
 * WAD archives. @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include "de_base.h"
#include "de_filesys.h"
#include "lumpcache.h"
#include "lumpindex.h"
#include "pathdirectory.h"

#include "wadfile.h"

#include <vector>

#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>
#include <de/memoryzone.h>

using namespace de;
using de::DFile;
using de::PathDirectoryNode;

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

struct WadLumpRecord
{
    uint crc;
    LumpInfo info_;

    explicit WadLumpRecord(LumpInfo const& _info) : crc(0)
    {
        F_CopyLumpInfo(&info_, &_info);
    }

    ~WadLumpRecord()
    {
        F_DestroyLumpInfo(&info_);
    }

    LumpInfo const& info() const
    {
        return info_;
    }
};

struct WadFile::Instance
{
    WadFile* self;

    /// Number of lump records in the archived wad.
    int arcRecordsCount;

    /// Offset to the lump record table in the archived wad.
    size_t arcRecordsOffset;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    typedef std::vector<PathDirectoryNode*> LumpNodeLut;
    LumpNodeLut* lumpNodeLut;

    /// Lump data cache.
    LumpCache* lumpCache;

    Instance(WadFile* d, DFile& file, const char* path)
        : self(d),
          arcRecordsCount(0),
          arcRecordsOffset(0),
          lumpDirectory(0),
          lumpNodeLut(0),
          lumpCache(0)
    {
        // Seek to the start of the header.
        file.seek(0, SeekSet);

        wadheader_t hdr;
        if(!readArchiveHeader(file, hdr))
            throw WadFile::FormatError("WadFile::WadFile", QString("File %1 does not appear to be a known WAD format").arg(path));

        arcRecordsCount  = hdr.lumpRecordsCount;
        arcRecordsOffset = hdr.lumpRecordsOffset;
    }

    ~Instance()
    {
        if(lumpDirectory)
        {
            PathDirectory_Iterate(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                                  NULL, PATHDIRECTORY_NOHASH, clearLumpRecordWorker);
            delete lumpDirectory;
        }

        if(lumpNodeLut) delete lumpNodeLut;
        if(lumpCache) delete lumpCache;
    }

    WadLumpRecord* lumpRecord(int lumpIdx)
    {
        if(!self->isValidIndex(lumpIdx)) return NULL;
        buildLumpNodeLut();
        return reinterpret_cast<WadLumpRecord*>((*lumpNodeLut)[lumpIdx]->userData());
    }

    static int clearLumpRecordWorker(pathdirectorynode_s* _node, void* /*parameters*/)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        WadLumpRecord* rec = reinterpret_cast<WadLumpRecord*>(node->userData());
        if(rec)
        {
            // Detach our user data from this node.
            node->setUserData(0);
            delete rec;
        }
        return 0; // Continue iteration.
    }

    /// @pre @a file is positioned at the start of the header.
    static bool readArchiveHeader(DFile& file, wadheader_t& hdr)
    {
        size_t readBytes = file.read((uint8_t*)&hdr, sizeof(wadheader_t));
        if(!(readBytes < sizeof(wadheader_t)))
        {
            hdr.lumpRecordsCount  = littleEndianByteOrder.toNative(hdr.lumpRecordsCount);
            hdr.lumpRecordsOffset = littleEndianByteOrder.toNative(hdr.lumpRecordsOffset);
            return true;
        }
        return false;
    }

    /// @return Length of the archived lump name in characters.
    static int nameLength(wadlumprecord_t const& lrec)
    {
        int nameLen = 0;
        while(nameLen < LUMPNAME_T_LASTINDEX && lrec.name[nameLen])
        { nameLen++; }
        return nameLen;
    }

    /// Perform all translations and encodings to the archived lump name and write
    /// the result to @a normName.
    static void normalizeName(wadlumprecord_t const& lrec, ddstring_t* normName)
    {
        LOG_AS("WadFile");
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

        // All lumps are ordained with the .lmp extension if they don't have one.
        char const* ext = F_FindFileExtension(Str_Text(normName));
        if(!(ext && Str_Length(normName) > ext - Str_Text(normName) + 1))
            Str_Append(normName, ".lmp");
    }

    void readLumpDirectory()
    {
        LOG_AS("WadFile");
        if(arcRecordsCount <= 0) return;

        // We'll load the lump directory using one continous read into a temporary
        // local buffer before we process it into our runtime representation.
        wadlumprecord_t* arcRecords = new wadlumprecord_t[arcRecordsCount];
        self->file->seek(arcRecordsOffset, SeekSet);
        self->file->read((uint8_t*)arcRecords, arcRecordsCount * sizeof(*arcRecords));

        // Reserve a small work buffer for processing archived lump names.
        ddstring_t absPath;
        Str_Reserve(Str_Init(&absPath), LUMPNAME_T_LASTINDEX + 4/*.lmp*/);

        // Intialize the directory.
        lumpDirectory = new PathDirectory(PDF_ALLOW_DUPLICATE_LEAF);

        // Build our runtime representation from the archived lump directory.
        wadlumprecord_t const* arcRecord = arcRecords;
        for(int i = 0; i < arcRecordsCount; ++i, arcRecord++)
        {
            LumpInfo info; F_InitLumpInfo(&info);
            info.baseOffset     = littleEndianByteOrder.toNative(arcRecord->filePos);
            info.size           = littleEndianByteOrder.toNative(arcRecord->size);
            info.compressedSize = info.size;
            info.container      = reinterpret_cast<abstractfile_s*>(self);
            // The modification date is inherited from the file (note recursion).
            info.lastModified   = self->lastModified();
            info.lumpIdx        = i;

            // Determine the name for this lump in the VFS.
            normalizeName(*arcRecord, &absPath);
            F_PrependBasePath(&absPath, &absPath); // Make it absolute.

            WadLumpRecord* record = new WadLumpRecord(info);
            PathDirectoryNode* node = lumpDirectory->insert(Str_Text(&absPath));
            node->setUserData(record);

            F_DestroyLumpInfo(&info);

            // Calcuate a simple CRC checksum for the lump.
            /// @note If we intend to use the CRC for anything meaningful this algorithm
            ///       should be replaced and execution deferred until the CRC is needed.
            record->crc = uint(record->info().size);
            int nameLen = nameLength(*arcRecord);
            for(int k = 0; k < nameLen; ++k)
            {
                record->crc += arcRecord->name[k];
            }
        }

        Str_Free(&absPath);

        // We are finished with the temporary lump directory records.
        delete[] arcRecords;
    }

    static int buildLumpNodeLutWorker(pathdirectorynode_s* _node, void* parameters)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        Instance* wadInst = (Instance*)parameters;
        WadLumpRecord* lumpRecord = reinterpret_cast<WadLumpRecord*>(node->userData());
        DENG2_ASSERT(lumpRecord && wadInst->self->isValidIndex(lumpRecord->info().lumpIdx)); // Sanity check.
        (*wadInst->lumpNodeLut)[lumpRecord->info().lumpIdx] = node;
        return 0; // Continue iteration.
    }

    void buildLumpNodeLut()
    {
        LOG_AS("WadFile");
        // Been here already?
        if(lumpNodeLut) return;

        lumpNodeLut = new LumpNodeLut(self->lumpCount());
        PathDirectory_Iterate2(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                               NULL, PATHDIRECTORY_NOHASH, buildLumpNodeLutWorker, (void*)this);
    }
};

WadFile::WadFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_WADFILE, path, file, info)
{
    d = new Instance(this, file, path);
}

WadFile::~WadFile()
{
    clearLumpCache();
    delete d;
}

bool WadFile::isValidIndex(int lumpIdx)
{
    return lumpIdx >= 0 && lumpIdx < lumpCount();
}

int WadFile::lastIndex()
{
    return lumpCount() - 1;
}

int WadFile::lumpCount()
{
    return d->lumpDirectory? d->lumpDirectory->size() : 0;
}

bool WadFile::empty()
{
    return !lumpCount();
}

static QString invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    QString msg = QString("Invalid lump index %1 ").arg(invalidIdx);
    if(lastValidIdx < 0) msg += "(file is empty)";
    else                 msg += QString("(valid range: [0..%2])").arg(lastValidIdx);
    return msg;
}

PathDirectoryNode& WadFile::lumpDirectoryNode(int lumpIdx)
{
    if(!isValidIndex(lumpIdx)) throw NotFoundError("WadFile::lumpDirectoryNode", invalidIndexMessage(lumpIdx, lastIndex()));
    d->buildLumpNodeLut();
    return *((*d->lumpNodeLut)[lumpIdx]);
}

LumpInfo const& WadFile::lumpInfo(int lumpIdx)
{
    LOG_AS("WadFile");
    WadLumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw NotFoundError("WadFile::lumpInfo", invalidIndexMessage(lumpIdx, lastIndex()));
    return lrec->info();
}

size_t WadFile::lumpSize(int lumpIdx)
{
    LOG_AS("WadFile");
    WadLumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw NotFoundError("WadFile::lumpSize", invalidIndexMessage(lumpIdx, lastIndex()));
    return lrec->info().size;
}

AutoStr* WadFile::composeLumpPath(int lumpIdx, char delimiter)
{
    if(!isValidIndex(lumpIdx)) return AutoStr_NewStd();

    PathDirectoryNode& node = lumpDirectoryNode(lumpIdx);
    return node.composePath(AutoStr_NewStd(), NULL, delimiter);
}

int WadFile::publishLumpsToIndex(LumpIndex& index)
{
    LOG_AS("WadFile");
    d->readLumpDirectory();
    if(empty()) return 0;

    // Insert the lumps into their rightful places in the index.
    int numPublished = lumpCount();
    index.catalogLumps(*this, 0, numPublished);
    return numPublished;
}

WadFile& WadFile::clearCachedLump(int lumpIdx, bool* retCleared)
{
    LOG_AS("WadFile::clearCachedLump");

    if(retCleared) *retCleared = false;

    if(isValidIndex(lumpIdx))
    {
        if(d->lumpCache)
        {
            d->lumpCache->remove(lumpIdx, retCleared);
        }
        else
        {
            LOG_DEBUG("LumpCache not in use, ignoring.");
        }
    }
    else
    {
        QString msg = invalidIndexMessage(lumpIdx, lastIndex());
        LOG_DEBUG(msg + ", ignoring.");
    }
    return *this;
}

WadFile& WadFile::clearLumpCache()
{
    LOG_AS("WadFile::clearLumpCache");
    if(d->lumpCache) d->lumpCache->clear();
    return *this;
}

uint8_t const* WadFile::cacheLump(int lumpIdx)
{
    LOG_AS("WadFile::cacheLump");

    if(!isValidIndex(lumpIdx)) throw NotFoundError("WadFile::cacheLump", invalidIndexMessage(lumpIdx, lastIndex()));

    LumpInfo const& info = lumpInfo(lumpIdx);
    LOG_TRACE("\"%s:%s\" (%lu bytes%s)")
        << F_PrettyPath(Str_Text(path()))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) info.size
        << (info.compressedSize != info.size? ", compressed" : "");

    // Time to create the cache?
    if(!d->lumpCache)
    {
        d->lumpCache = new LumpCache(lumpCount());
    }

    uint8_t const* data = d->lumpCache->data(lumpIdx);
    if(data) return data;

    uint8_t* region = (uint8_t*) Z_Malloc(info.size, PU_APPSTATIC, 0);
    if(!region) throw Error("WadFile::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(info.size).arg(lumpIdx));

    readLump(lumpIdx, region, false);
    d->lumpCache->insert(lumpIdx, region);

    return region;
}

WadFile& WadFile::unlockLump(int lumpIdx)
{
    LOG_AS("WadFile::unlockLump");
    LOG_TRACE("\"%s:%s\"")
        << F_PrettyPath(Str_Text(path())) << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')));

    if(isValidIndex(lumpIdx))
    {
        if(d->lumpCache)
        {
            d->lumpCache->unlock(lumpIdx);
        }
        else
        {
            LOG_DEBUG("LumpCache not in use, ignoring.");
        }
    }
    else
    {
        QString msg = invalidIndexMessage(lumpIdx, lastIndex());
        LOG_DEBUG(msg + ", ignoring.");
    }
    return *this;
}

size_t WadFile::readLump(int lumpIdx, uint8_t* buffer, bool tryCache)
{
    LOG_AS("WadFile::readLump");
    if(!isValidIndex(lumpIdx)) return 0;
    return readLump(lumpIdx, buffer, 0, lumpInfo(lumpIdx).size, tryCache);
}

size_t WadFile::readLump(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("WadFile::readLump");
    WadLumpRecord const* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) return 0;

    LOG_TRACE("\"%s:%s\" (%lu bytes%s) [%lu +%lu]")
        << F_PrettyPath(Str_Text(path()))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) lrec->info().size
        << (lrec->info().compressedSize != lrec->info().size? ", compressed" : "")
        << (unsigned long) startOffset
        << (unsigned long)length;

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache)
    {
        uint8_t const* data = d->lumpCache? d->lumpCache->data(lumpIdx) : 0;
        LOG_DEBUG("Cache %s on #%i") << (data? "hit" : "miss") << lumpIdx;
        if(data)
        {
            size_t readBytes = MIN_OF(lrec->info().size, length);
            memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    file->seek(lrec->info().baseOffset + startOffset, SeekSet);
    size_t readBytes = file->read(buffer, length);

    /// @todo Do not check the read length here.
    if(readBytes < length)
        throw Error("WadFile::readLumpSection", QString("Only read %1 of %2 bytes of lump #%3").arg(readBytes).arg(length).arg(lumpIdx));

    return readBytes;
}

uint WadFile::calculateCRC()
{
    uint crc = 0;
    const int numLumps = lumpCount();
    for(int i = 0; i < numLumps; ++i)
    {
        WadLumpRecord const* lrec = d->lumpRecord(i);
        crc += lrec->crc;
    }
    return crc;
}

bool WadFile::recognise(DFile& file)
{
    wadheader_t hdr;

    // Seek to the start of the header.
    size_t initPos = file.tell();
    file.seek(0, SeekSet);

    bool readHeaderOk = WadFile::Instance::readArchiveHeader(file, hdr);

    // Return the stream to its original position.
    file.seek(initPos, SeekSet);

    if(!readHeaderOk) return false;
    if(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)) return false;
    return true;
}
