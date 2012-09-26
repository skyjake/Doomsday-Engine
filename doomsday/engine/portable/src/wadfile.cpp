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

#include "lumpdirectory.h"
#include "pathdirectory.h"
#include "wadfile.h"

#include <vector>

#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>

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

struct LumpRecord
{
    size_t baseOffset;
    uint crc;
    LumpInfo info;

    LumpRecord() : baseOffset(0), crc(0)
    {
        F_InitLumpInfo(&info);
    }
};

struct de::WadFile::Instance
{
    de::WadFile* self;

    /// Number of lump records in the archived wad.
    int arcRecordsCount;

    /// Offset to the lump record table in the archived wad.
    size_t arcRecordsOffset;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    typedef std::vector<PathDirectoryNode*> LumpNodeLut;
    LumpNodeLut* lumpNodeLut;

    /// Lump cache data pointers.
    void** lumpCache;

    Instance(de::WadFile* d, DFile& file, const char* path)
        : self(d),
          arcRecordsCount(0),
          arcRecordsOffset(0),
          lumpDirectory(0),
          lumpNodeLut(0),
          lumpCache(0)
    {
        // Seek to the start of the header.
        DFile_Seek(&file, 0, SEEK_SET);

        wadheader_t hdr;
        if(!readArchiveHeader(file, hdr))
            throw de::Error("WadFile::WadFile", QString("File %1 does not appear to be a known WAD format").arg(path));

        arcRecordsCount  = hdr.lumpRecordsCount;
        arcRecordsOffset = hdr.lumpRecordsOffset;
    }

    ~Instance()
    {
        if(self->lumpCount() > 1 && lumpCache)
        {
            M_Free(lumpCache);
        }

        if(lumpDirectory)
        {
            PathDirectory_Iterate(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                                  NULL, PATHDIRECTORY_NOHASH, clearLumpRecordWorker);
            delete lumpDirectory;
        }

        if(lumpNodeLut) delete lumpNodeLut;
    }

    LumpRecord* lumpRecord(int lumpIdx)
    {
        if(!self->isValidIndex(lumpIdx)) return NULL;
        buildLumpNodeLut();
        return reinterpret_cast<LumpRecord*>((*lumpNodeLut)[lumpIdx]->userData());
    }

    static int clearLumpRecordWorker(pathdirectorynode_s* _node, void* /*parameters*/)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        LumpRecord* rec = reinterpret_cast<LumpRecord*>(node->userData());
        if(rec)
        {
            // Detach our user data from this node.
            node->setUserData(0);
            F_DestroyLumpInfo(&rec->info);
            delete rec;
        }
        return 0; // Continue iteration.
    }

    /// @pre @a file is positioned at the start of the header.
    static bool readArchiveHeader(DFile& file, wadheader_t& hdr)
    {
        size_t readBytes = DFile_Read(&file, (uint8_t*)&hdr, sizeof(wadheader_t));
        if(!(readBytes < sizeof(wadheader_t)))
        {
            hdr.lumpRecordsCount  = de::littleEndianByteOrder.toNative(hdr.lumpRecordsCount);
            hdr.lumpRecordsOffset = de::littleEndianByteOrder.toNative(hdr.lumpRecordsOffset);
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
        Str_StripRight(normName);

        // Lump names allow characters the file system does not. Therefore they
        // will be percent-encoded here and later decoded if/when necessary.
        Str_PercentEncode(normName);

        /// We do not consider zero-length names to be valid, so replace with
        /// with _something_.
        /// @todo Handle this more elegantly...
        if(Str_IsEmpty(normName))
            Str_Set(normName, "________");

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
        DFile_Seek(self->base._file, arcRecordsOffset, SEEK_SET);
        DFile_Read(self->base._file, (uint8_t*)arcRecords, arcRecordsCount * sizeof(*arcRecords));

        // Reserve a small work buffer for processing archived lump names.
        ddstring_t absPath;
        Str_Reserve(Str_Init(&absPath), LUMPNAME_T_LASTINDEX + 4/*.lmp*/);

        // Intialize the directory.
        lumpDirectory = new PathDirectory(PDF_ALLOW_DUPLICATE_LEAF);

        // Build our runtime representation from the archived lump directory.
        wadlumprecord_t const* arcRecord = arcRecords;
        for(int i = 0; i < arcRecordsCount; ++i, arcRecord++)
        {
            LumpRecord* record = new LumpRecord();

            record->baseOffset     = de::littleEndianByteOrder.toNative(arcRecord->filePos);
            record->info.size      = de::littleEndianByteOrder.toNative(arcRecord->size);
            record->info.compressedSize = record->info.size;
            record->info.container = reinterpret_cast<abstractfile_t*>(&self->base);
            // The modification date is inherited from the file (note recursion).
            record->info.lastModified = AbstractFile_LastModified(reinterpret_cast<abstractfile_t*>(&self->base));
            record->info.lumpIdx   = i;

            // Determine the name for this lump in the VFS.
            normalizeName(*arcRecord, &absPath);
            F_PrependBasePath(&absPath, &absPath); // Make it absolute.

            // Insert this lump record into the directory.
            PathDirectoryNode* node = lumpDirectory->insert(Str_Text(&absPath));
            node->setUserData(record);

            // Calcuate a simple CRC checksum for the lump.
            /// @note If we intend to use the CRC for anything meaningful this algorithm
            ///       should be replaced and execution deferred until the CRC is needed.
            record->crc = uint(record->info.size);
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

    static int buildLumpDirectoryMapWorker(pathdirectorynode_s* _node, void* parameters)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        Instance* wadInst = (Instance*)parameters;
        LumpRecord* lumpRecord = reinterpret_cast<LumpRecord*>(node->userData());
        DENG2_ASSERT(lumpRecord && wadInst->self->isValidIndex(lumpRecord->info.lumpIdx));
        (*wadInst->lumpNodeLut)[lumpRecord->info.lumpIdx] = node;
        return 0; // Continue iteration.
    }

    void buildLumpNodeLut()
    {
        LOG_AS("WadFile");
        // Been here already?
        if(lumpNodeLut) return;

        lumpNodeLut = new LumpNodeLut(self->lumpCount());
        PathDirectory_Iterate2(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                               NULL, PATHDIRECTORY_NOHASH, buildLumpDirectoryMapWorker, (void*)this);
    }

    void** lumpCacheAddress(int lumpIdx)
    {
        if(!lumpCache) return 0;
        if(!self->isValidIndex(lumpIdx)) return 0;
        if(self->lumpCount() > 1)
        {
            const uint cacheIdx = lumpIdx;
            return &lumpCache[cacheIdx];
        }
        else
        {
            return (void**)&lumpCache;
        }
    }
};

de::WadFile::WadFile(DFile& file, char const* path, LumpInfo const& info)
{
    AbstractFile_Init(reinterpret_cast<abstractfile_t*>(this), FT_WADFILE, path, &file, &info);
    d = new Instance(this, file, path);
}

de::WadFile::~WadFile()
{
    F_ReleaseFile(reinterpret_cast<abstractfile_t*>(this));
    clearLumpCache();
    delete d;
    AbstractFile_Destroy(reinterpret_cast<abstractfile_t*>(this));
}

bool de::WadFile::isValidIndex(int lumpIdx)
{
    return lumpIdx >= 0 && lumpIdx < lumpCount();
}

int de::WadFile::lumpCount()
{
    return d->lumpDirectory? d->lumpDirectory->size() : 0;
}

de::PathDirectoryNode* de::WadFile::lumpDirectoryNode(int lumpIdx)
{
    if(!isValidIndex(lumpIdx)) return NULL;
    d->buildLumpNodeLut();
    return (*d->lumpNodeLut)[lumpIdx];
}

LumpInfo const* de::WadFile::lumpInfo(int lumpIdx)
{
    LOG_AS("WadFile");
    LumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw de::Error("WadFile::lumpInfo", QString("Invalid lump index %1 (valid range: [0..%2])").arg(lumpIdx).arg(lumpCount()));
    return &lrec->info;
}

size_t de::WadFile::lumpSize(int lumpIdx)
{
    LOG_AS("WadFile");
    LumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw de::Error("WadFile::lumpSize", QString("Invalid lump index %1 (valid range: [0..%2])").arg(lumpIdx).arg(lumpCount()));
    return lrec->info.size;
}

AutoStr* de::WadFile::composeLumpPath(int lumpIdx, char delimiter)
{
    de::PathDirectoryNode* node = lumpDirectoryNode(lumpIdx);
    if(node)
    {
        return node->composePath(AutoStr_NewStd(), NULL, delimiter);
    }
    return AutoStr_NewStd();
}

int de::WadFile::publishLumpsToDirectory(LumpDirectory* directory)
{
    LOG_AS("WadFile");
    int numPublished = 0;
    if(directory)
    {
        d->readLumpDirectory();
        const int numLumps = lumpCount();
        if(numLumps)
        {
            // Insert the lumps into their rightful places in the directory.
            LumpDirectory_CatalogLumps(directory, (abstractfile_t*)this, 0, numLumps);
            numPublished += numLumps;
        }
    }
    return numPublished;
}

de::WadFile& de::WadFile::clearCachedLump(int lumpIdx, bool* retCleared)
{
    LOG_AS("WadFile");
    void** cacheAdr = d->lumpCacheAddress(lumpIdx);
    bool isCached = (cacheAdr && *cacheAdr);
    if(isCached)
    {
        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(*cacheAdr) < PU_MAP)
        {
            Z_ChangeTag2(*cacheAdr, PU_MAP);
        }

        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(*cacheAdr, (void*) 0x2);
    }

    if(retCleared) *retCleared = isCached;
    return *this;
}

de::WadFile& de::WadFile::clearLumpCache()
{
    LOG_AS("WadFile");
    const int numLumps = lumpCount();
    for(int i = 0; i < numLumps; ++i)
    {
        clearCachedLump(i);
    }
    return *this;
}

uint8_t const* de::WadFile::cacheLump(int lumpIdx, int tag)
{
    LOG_AS("WadFile::cacheLump");
    const LumpInfo* info = lumpInfo(lumpIdx);

    LOG_TRACE("\"%s:%s\" (%lu bytes%s)")
        << F_PrettyPath(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this))))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) info->size
        << (info->compressedSize != info->size? ", compressed" : "");

    // Time to create the lump cache?
    if(!d->lumpCache)
    {
        if(lumpCount() > 1)
        {
            d->lumpCache = (void**) M_Calloc(lumpCount() * sizeof(*d->lumpCache));
        }
    }

    void** cacheAdr = d->lumpCacheAddress(lumpIdx);
    bool isCached = cacheAdr && *cacheAdr;
    if(!isCached)
    {
        uint8_t* buffer = (uint8_t*) Z_Malloc(info->size, tag, cacheAdr);
        if(!buffer) throw de::Error("WadFile::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(info->size).arg(lumpIdx));

        readLump(lumpIdx, buffer, false);
    }
    else
    {
        Z_ChangeTag2(*cacheAdr, tag);
    }

    return (uint8_t*)(*cacheAdr);
}

de::WadFile& de::WadFile::changeLumpCacheTag(int lumpIdx, int tag)
{
    LOG_AS("WadFile::changeLumpCacheTag");
    LOG_TRACE("\"%s:%s\" tag=%i")
        << F_PrettyPath(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this))))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << tag;

    void** cacheAdr = d->lumpCacheAddress(lumpIdx);
    bool isCached = cacheAdr && *cacheAdr;
    if(isCached)
    {
        Z_ChangeTag2(*cacheAdr, tag);
    }
    return *this;
}

size_t de::WadFile::readLumpSection(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("WadFile::readLumpSection");
    LumpRecord const* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) return 0;

    LOG_TRACE("\"%s:%s\" (%lu bytes%s) [%lu +%lu]")
        << F_PrettyPath(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this))))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) lrec->info.size
        << (lrec->info.compressedSize != lrec->info.size? ", compressed" : "")
        << (unsigned long) startOffset
        << (unsigned long)length;

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache)
    {
        void** cacheAdr = d->lumpCacheAddress(lumpIdx);
        bool isCached = cacheAdr && *cacheAdr;
        LOG_DEBUG("Cache %s on #%i") << (isCached? "hit" : "miss") << lumpIdx;
        if(isCached)
        {
            size_t readBytes = MIN_OF(lrec->info.size, length);
            memcpy(buffer, (char*)*cacheAdr + startOffset, readBytes);
            return readBytes;
        }
    }

    DFile_Seek(base._file, lrec->baseOffset + startOffset, SEEK_SET);
    size_t readBytes = DFile_Read(base._file, buffer, length);

    /// @todo Do not check the read length here.
    if(readBytes < length)
        throw de::Error("WadFile::readLumpSection", QString("Only read %1 of %2 bytes of lump #%3").arg(readBytes).arg(length).arg(lumpIdx));

    return readBytes;
}

size_t de::WadFile::readLump(int lumpIdx, uint8_t* buffer, bool tryCache)
{
    LOG_AS("WadFile::readLump");
    LumpInfo const* info = lumpInfo(lumpIdx);
    if(!info) return 0;
    return readLumpSection(lumpIdx, buffer, 0, info->size, tryCache);
}

uint de::WadFile::calculateCRC()
{
    uint crc = 0;
    const int numLumps = lumpCount();
    for(int i = 0; i < numLumps; ++i)
    {
        LumpRecord const* lrec = d->lumpRecord(i);
        crc += lrec->crc;
    }
    return crc;
}

bool de::WadFile::recognise(DFile& file)
{
    wadheader_t hdr;

    // Seek to the start of the header.
    size_t initPos = DFile_Tell(&file);
    DFile_Seek(&file, 0, SEEK_SET);

    bool readHeaderOk = de::WadFile::Instance::readArchiveHeader(file, hdr);

    // Return the stream to its original position.
    DFile_Seek(&file, initPos, SEEK_SET);

    if(!readHeaderOk) return false;
    if(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)) return false;
    return true;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::WadFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::WadFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::WadFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::WadFile const* self = TOINTERNAL_CONST(inst)

WadFile* WadFile_New(DFile* file, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("WadFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<WadFile*>(new de::WadFile(*file, path, *info));
    }
    catch(de::Error& er)
    {
        QString msg = QString("WadFile_New: Failed to instantiate new WadFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void WadFile_Delete(WadFile* wad)
{
    if(wad)
    {
        SELF(wad);
        delete self;
    }
}

int WadFile_PublishLumpsToDirectory(WadFile* wad, LumpDirectory* directory)
{
    SELF(wad);
    return self->publishLumpsToDirectory(directory);
}

PathDirectoryNode* WadFile_LumpDirectoryNode(WadFile* wad, int lumpIdx)
{
    SELF(wad);
    return reinterpret_cast<PathDirectoryNode*>( self->lumpDirectoryNode(lumpIdx) );
}

AutoStr* WadFile_ComposeLumpPath(WadFile* wad, int lumpIdx, char delimiter)
{
    SELF(wad);
    return self->composeLumpPath(lumpIdx, delimiter);
}

const LumpInfo* WadFile_LumpInfo(WadFile* wad, int lumpIdx)
{
    SELF(wad);
    return self->lumpInfo(lumpIdx);
}

void WadFile_ClearLumpCache(WadFile* wad)
{
    SELF(wad);
    self->clearLumpCache();
}

uint WadFile_CalculateCRC(WadFile* wad)
{
    SELF(wad);
    return self->calculateCRC();
}

size_t WadFile_ReadLumpSection2(WadFile* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    SELF(wad);
    return self->readLumpSection(lumpIdx, buffer, startOffset, length, tryCache);
}

size_t WadFile_ReadLumpSection(WadFile* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    SELF(wad);
    return self->readLumpSection(lumpIdx, buffer, startOffset, length);
}

size_t WadFile_ReadLump2(WadFile* wad, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    SELF(wad);
    return self->readLump(lumpIdx, buffer, tryCache);
}

size_t WadFile_ReadLump(WadFile* wad, int lumpIdx, uint8_t* buffer)
{
    SELF(wad);
    return self->readLump(lumpIdx, buffer);
}

const uint8_t* WadFile_CacheLump(WadFile* wad, int lumpIdx, int tag)
{
    SELF(wad);
    return self->cacheLump(lumpIdx, tag);
}

void WadFile_ChangeLumpCacheTag(WadFile* wad, int lumpIdx, int tag)
{
    SELF(wad);
    self->changeLumpCacheTag(lumpIdx, tag);
}

int WadFile_LumpCount(WadFile* wad)
{
    SELF(wad);
    return self->lumpCount();
}

boolean WadFile_Recognise(DFile* file)
{
    if(!file) return false;
    return de::WadFile::recognise(*file);
}
