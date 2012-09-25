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

#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>

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
};

struct wadfile_s
{
    /// Base file instance.
    abstractfile_t base;

    /// Number of lumprecords in the archived wad.
    int lumpRecordsCount;

    /// Offset to the lumprecord table in the archived wad.
    size_t lumpRecordsOffset;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    PathDirectoryNode** lumpDirectoryMap;

    /// Vector of lump records.
    LumpRecord* lumpRecords;

    /// Lump cache data pointers.
    void** lumpCache;

    wadfile_s(DFile& file, const char* path, LumpInfo const& info)
        : lumpRecordsCount(0),
          lumpRecordsOffset(0),
          lumpDirectory(0),
          lumpDirectoryMap(0),
          lumpRecords(0),
          lumpCache(0)
    {
        AbstractFile_Init(&base, FT_WADFILE, path, &file, &info);

        wadheader_t hdr;
        if(!readArchiveHeader(file, hdr))
            throw de::Error("wadfile_s::wadfile_s", QString("File %1 does not appear to be a known WAD format").arg(path));

        lumpRecordsCount  = hdr.lumpRecordsCount;
        lumpRecordsOffset = hdr.lumpRecordsOffset;
    }

    ~wadfile_s()
    {
        F_ReleaseFile(reinterpret_cast<abstractfile_t*>(this));
        clearLumpCache();

        if(lumpDirectory)
        {
            if(PathDirectory_Size(lumpDirectory) > 1 && lumpCache)
            {
                free(lumpCache);
            }

            PathDirectory_Iterate(lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearLumpRecordWorker);
            PathDirectory_Delete(lumpDirectory);
        }

        if(lumpDirectoryMap) free(lumpDirectoryMap);
        if(lumpRecords) free(lumpRecords);

        AbstractFile_Destroy(reinterpret_cast<abstractfile_t*>(this));
    }

    static bool recognise(DFile& file)
    {
        wadheader_t hdr;
        if(!wadfile_s::readArchiveHeader(file, hdr)) return false;
        if(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)) return false;
        return true;
    }

    int lumpCount()
    {
        return lumpDirectory? PathDirectory_Size(lumpDirectory) : 0;
    }

    LumpRecord* lumpRecord(int lumpIdx)
    {
        if(lumpIdx < 0 || lumpIdx >= lumpCount()) return NULL;
        buildLumpDirectoryMap();
        return reinterpret_cast<LumpRecord*>(PathDirectoryNode_UserData(lumpDirectoryMap[lumpIdx]));
    }

    PathDirectoryNode* lumpDirectoryNode(int lumpIdx)
    {
        if(lumpIdx < 0 || lumpIdx >= lumpCount()) return NULL;
        buildLumpDirectoryMap();
        return lumpDirectoryMap[lumpIdx];
    }

    LumpInfo const* lumpInfo(int lumpIdx)
    {
        LOG_AS("WadFile");
        LumpRecord* lrec = lumpRecord(lumpIdx);
        if(!lrec) throw de::Error("WadFile::lumpInfo", QString("Invalid lump index %1 (valid range: [0..%2])").arg(lumpIdx).arg(lumpCount()));
        return &lrec->info;
    }

    AutoStr* composeLumpPath(int lumpIdx, char delimiter)
    {
        PathDirectoryNode* node = lumpDirectoryNode(lumpIdx);
        if(node)
        {
            return PathDirectoryNode_ComposePath2(node, AutoStr_NewStd(), NULL, delimiter);
        }
        return AutoStr_NewStd();
    }

    static int clearLumpRecordWorker(PathDirectoryNode* node, void* /*parameters*/)
    {
        LumpRecord* rec = reinterpret_cast<LumpRecord*>(PathDirectoryNode_UserData(node));
        if(rec)
        {
            // Detach our user data from this node.
            PathDirectoryNode_SetUserData(node, 0);
            F_DestroyLumpInfo(&rec->info);
            // The record itself is free'd later.
        }
        return 0; // Continue iteration.
    }

    static bool readArchiveHeader(DFile& file, wadheader_t& hdr)
    {
        size_t readBytes, initPos = DFile_Tell(&file);
        // Seek to the start of the header.
        DFile_Seek(&file, 0, SEEK_SET);
        readBytes = DFile_Read(&file, (uint8_t*)&hdr, sizeof(wadheader_t));
        // Return the stream to its original position.
        DFile_Seek(&file, initPos, SEEK_SET);
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
    static void normalizeName(wadlumprecord_t const& lrec, Str* normName)
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

    // We'll load the lump directory using one continous read into a temporary
    // local buffer before we process it into our runtime representation.
    void readLumpDirectory()
    {
        LOG_AS("WadFile");
        if(lumpRecordsCount <= 0) return;

        wadlumprecord_t* lumpDir;
        size_t lumpDirSize = lumpRecordsCount * sizeof(*lumpDir);
        lumpDir = (wadlumprecord_t*) malloc(lumpDirSize);
        if(!lumpDir) throw de::Error("WadFile::readLumpDirectory", QString("Failed on allocation of %1 bytes for temporary lump directory buffer").arg(lumpDirSize));

        // Buffer the archived lump directory.
        DFile_Seek(base._file, lumpRecordsOffset, SEEK_SET);
        DFile_Read(base._file, (uint8_t*)lumpDir, lumpDirSize);

        lumpRecords = (LumpRecord*) malloc(lumpRecordsCount * sizeof(*lumpRecords));
        if(!lumpRecords) throw de::Error("WadFile::readLumpDirectory", QString("Failed on allocation of %1 bytes for new lump record vector").arg(lumpRecordsCount * sizeof(*lumpRecords)));

        // Reserve a small work buffer for processing archived lump names.
        ddstring_t absPath;
        Str_Reserve(Str_Init(&absPath), LUMPNAME_T_LASTINDEX + 4/*.lmp*/);

        // Read the archived lump directory itself:
        wadlumprecord_t const* arcRecord = lumpDir;
        LumpRecord* record = lumpRecords;
        for(int i = 0; i < lumpRecordsCount; ++i, arcRecord++, record++)
        {
            record->baseOffset = de::littleEndianByteOrder.toNative(arcRecord->filePos);

            F_InitLumpInfo(&record->info);
            record->info.lumpIdx = i;

            normalizeName(*arcRecord, &absPath);

            // Make it absolute.
            F_PrependBasePath(&absPath, &absPath);

            record->info.size = record->info.compressedSize = de::littleEndianByteOrder.toNative(arcRecord->size);
            record->info.container = reinterpret_cast<abstractfile_t*>(this);

            // The modification date is inherited from the real file (note recursion).
            record->info.lastModified = AbstractFile_LastModified(reinterpret_cast<abstractfile_t*>(this));

            // Have we yet to intialize the directory?
            if(!lumpDirectory)
            {
                lumpDirectory = PathDirectory_NewWithFlags(PDF_ALLOW_DUPLICATE_LEAF);
            }

            PathDirectoryNode* node = PathDirectory_Insert2(lumpDirectory, Str_Text(&absPath), '/');
            PathDirectoryNode_SetUserData(node, record);

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
        free(lumpDir);
    }

    static int buildLumpDirectoryMapWorker(PathDirectoryNode* node, void* parameters)
    {
        WadFile* wad = (WadFile*)parameters;
        LumpRecord* lumpRecord = reinterpret_cast<LumpRecord*>(PathDirectoryNode_UserData(node));
        DENG2_ASSERT(lumpRecord && lumpRecord->info.lumpIdx >= 0 && lumpRecord->info.lumpIdx < WadFile_LumpCount(wad));
        wad->lumpDirectoryMap[lumpRecord->info.lumpIdx] = node;
        return 0; // Continue iteration.
    }

    void buildLumpDirectoryMap()
    {
        LOG_AS("WadFile");
        // Been here already?
        if(lumpDirectoryMap) return;

        lumpDirectoryMap = (PathDirectoryNode**) malloc(sizeof(*lumpDirectoryMap) * lumpCount());
        if(!lumpDirectoryMap) throw de::Error("WadFile::LumpRecord", QString("Failed on allocation of %1 bytes for the lumpdirectory map").arg(sizeof(*lumpDirectoryMap) * lumpCount()));

        PathDirectory_Iterate2(lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH,
                               buildLumpDirectoryMapWorker, (void*)this);
    }

    int publishLumpsToDirectory(LumpDirectory* directory)
    {
        LOG_AS("WadFile");
        int numPublished = 0;
        if(directory)
        {
            readLumpDirectory();
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

    void** lumpCacheAddress(int lumpIdx)
    {
        if(!lumpCache || lumpIdx < 0 || lumpIdx >= lumpCount()) return 0;
        if(lumpCount() > 1)
        {
            const uint cacheIdx = lumpIdx;
            return &lumpCache[cacheIdx];
        }
        else
        {
            return (void**)&lumpCache;
        }
    }

    wadfile_s& clearCachedLump(int lumpIdx, bool* retCleared = 0)
    {
        LOG_AS("WadFile");
        void** cacheAdr = lumpCacheAddress(lumpIdx);
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

    wadfile_s& clearLumpCache()
    {
        LOG_AS("WadFile");
        const int numLumps = lumpCount();
        for(int i = 0; i < numLumps; ++i)
        {
            clearCachedLump(i);
        }
        return *this;
    }

    const uint8_t* cacheLump(int lumpIdx, int tag)
    {
        LOG_AS("WadFile::cacheLump");
        const LumpInfo* info = lumpInfo(lumpIdx);

        LOG_TRACE("\"%s:%s\" (%lu bytes%s)")
            << F_PrettyPath(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this))))
            << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
            << (unsigned long) info->size
            << (info->compressedSize != info->size? ", compressed" : "");

        // Time to create the lump cache?
        if(!lumpCache)
        {
            if(lumpCount() > 1)
            {
                lumpCache = (void**)calloc(lumpCount(), sizeof(*lumpCache));
            }
        }

        void** cacheAdr = lumpCacheAddress(lumpIdx);
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

    wadfile_s& changeLumpCacheTag(int lumpIdx, int tag)
    {
        LOG_AS("WadFile::changeLumpCacheTag");
        LOG_TRACE("\"%s:%s\" tag=%i")
            << F_PrettyPath(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this))))
            << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
            << tag;

        void** cacheAdr = lumpCacheAddress(lumpIdx);
        bool isCached = cacheAdr && *cacheAdr;
        if(isCached)
        {
            Z_ChangeTag2(*cacheAdr, tag);
        }
        return *this;
    }

    size_t readLumpSection(int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length,
                           bool tryCache = true)
    {
        LOG_AS("WadFile::readLumpSection");
        LumpRecord const* lrec = lumpRecord(lumpIdx);
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
            void** cacheAdr = lumpCacheAddress(lumpIdx);
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

    size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true)
    {
        LOG_AS("WadFile::readLump");
        LumpInfo const* info = lumpInfo(lumpIdx);
        if(!info) return 0;
        return readLumpSection(lumpIdx, buffer, 0, info->size, tryCache);
    }

    uint calculateCRC()
    {
        uint crc = 0;
        const int numLumps = lumpCount();
        for(int i = 0; i < numLumps; ++i)
        {
            LumpRecord const* lrec = lumpRecord(i);
            crc += lrec->crc;
        }
        return crc;
    }
};

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<wadfile_s*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<wadfile_s const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    wadfile_s* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    wadfile_s const* self = TOINTERNAL_CONST(inst)

WadFile* WadFile_New(DFile* file, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("WadFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<WadFile*>(new wadfile_s(*file, path, *info));
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
    return self->lumpDirectoryNode(lumpIdx);
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
    return CPP_BOOL( wadfile_s::recognise(*file) );
}
