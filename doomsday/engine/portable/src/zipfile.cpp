/**
 * @file zipfile.cpp
 * ZIP archives. @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <zlib.h>

#include "de_base.h"
#include "de_filesys.h"

#include "game.h"
#include "lumpdirectory.h"
#include "pathdirectory.h"
#include "zipfile.h"

#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>

#define SIG_LOCAL_FILE_HEADER   0x04034b50
#define SIG_CENTRAL_FILE_HEADER 0x02014b50
#define SIG_END_OF_CENTRAL_DIR  0x06054b50

// Maximum tolerated size of the comment.
#define MAXIMUM_COMMENT_SIZE    2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE        22

// File header flags.
#define ZFH_ENCRYPTED           0x1
#define ZFH_COMPRESSION_OPTS    0x6
#define ZFH_DESCRIPTOR          0x8
#define ZFH_COMPRESS_PATCHED    0x20    // Not supported.

// Compression methods.
enum {
    ZFC_NO_COMPRESSION = 0,     // Supported format.
    ZFC_SHRUNK,
    ZFC_REDUCED_1,
    ZFC_REDUCED_2,
    ZFC_REDUCED_3,
    ZFC_REDUCED_4,
    ZFC_IMPLODED,
    ZFC_DEFLATED = 8,           // The only supported compression (via zlib).
    ZFC_DEFLATED_64,
    ZFC_PKWARE_DCL_IMPLODED
};

/// The following structures are used to read data directly from ZIP files.
#pragma pack(1)
typedef struct localfileheader_s {
    uint32_t      signature;
    uint16_t      requiredVersion;
    uint16_t      flags;
    uint16_t      compression;
    uint16_t      lastModTime;
    uint16_t      lastModDate;
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
    uint16_t      fileNameSize;
    uint16_t      extraFieldSize;
} localfileheader_t;

typedef struct descriptor_s {
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
} descriptor_t;

typedef struct centralfileheader_s {
    uint32_t      signature;
    uint16_t      version;
    uint16_t      requiredVersion;
    uint16_t      flags;
    uint16_t      compression;
    uint16_t      lastModTime;
    uint16_t      lastModDate;
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
    uint16_t      fileNameSize;
    uint16_t      extraFieldSize;
    uint16_t      commentSize;
    uint16_t      diskStart;
    uint16_t      internalAttrib;
    uint32_t      externalAttrib;
    uint32_t      relOffset;

    /*
     * file name (variable size)
     * extra field (variable size)
     * file comment (variable size)
     */
} centralfileheader_t;

typedef struct centralend_s {
    uint16_t      disk;
    uint16_t      centralStartDisk;
    uint16_t      diskEntryCount;
    uint16_t      totalEntryCount;
    uint32_t      size;
    uint32_t      offset;
    uint16_t      commentSize;
} centralend_t;
#pragma pack()

static void ApplyPathMappings(ddstring_t* dest, const ddstring_t* src);

struct LumpRecord
{
    size_t baseOffset;
    LumpInfo info;
};

struct zipfile_s
{
    /// Base file instance.
    abstractfile_t base;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    PathDirectoryNode** lumpDirectoryMap;

    /// Vector of lump records.
    LumpRecord* lumpRecords;

    /// Lump cache data pointers.
    void** lumpCache;

    zipfile_s(DFile& file, const char* path, LumpInfo const& info)
        : lumpDirectory(0),
          lumpDirectoryMap(0),
          lumpRecords(0),
          lumpCache(0)
    {
        AbstractFile_Init(reinterpret_cast<abstractfile_t*>(this), FT_ZIPFILE, path, &file, &info);
    }

    ~zipfile_s()
    {
        F_ReleaseFile(reinterpret_cast<abstractfile_t*>(this));
        clearLumpCache();

        if(lumpDirectory)
        {
            if(PathDirectory_Size(lumpDirectory) > 1 && lumpCache)
            {
                M_Free(lumpCache);
            }

            PathDirectory_Iterate(lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, clearLumpRecordWorker);
            PathDirectory_Delete(lumpDirectory);
        }

        if(lumpDirectoryMap) M_Free(lumpDirectoryMap);
        if(lumpRecords) M_Free(lumpRecords);

        AbstractFile_Destroy(reinterpret_cast<abstractfile_t*>(this));
    }

    static bool recognise(DFile& file)
    {
        localfileheader_t hdr;
        if(!zipfile_s::readArchiveHeader(file, hdr)) return false;
        return hdr.signature == SIG_LOCAL_FILE_HEADER;
    }

    int lumpCount()
    {
        return lumpDirectory? PathDirectory_Size(lumpDirectory) : 0;
    }

    LumpRecord* lumpRecord(int lumpIdx)
    {
        if(lumpIdx < 0 || lumpIdx >= lumpCount()) return NULL;
        buildLumpDirectoryMap();
        return reinterpret_cast<LumpRecord*>( PathDirectoryNode_UserData(lumpDirectoryMap[lumpIdx]) );
    }

    PathDirectoryNode* lumpDirectoryNode(int lumpIdx)
    {
        if(lumpIdx < 0 || lumpIdx >= lumpCount()) return NULL;
        buildLumpDirectoryMap();
        return lumpDirectoryMap[lumpIdx];
    }

    LumpInfo const* lumpInfo(int lumpIdx)
    {
        LOG_AS("ZipFile");
        LumpRecord* lrec = lumpRecord(lumpIdx);
        if(!lrec) throw de::Error("ZipFile::lumpInfo", QString("Invalid lump index %1 (valid range: [0..%2])").arg(lumpIdx).arg(lumpCount()));
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

    /// @todo Do not position the stream here.
    static bool readArchiveHeader(DFile& file, localfileheader_t& hdr)
    {
        size_t readBytes, initPos = DFile_Tell(&file);
        // Seek to the start of the header.
        DFile_Seek(&file, 0, SEEK_SET);
        readBytes = DFile_Read(&file, (uint8_t*)&hdr, sizeof(localfileheader_t));
        // Return the stream to its original position.
        DFile_Seek(&file, initPos, SEEK_SET);
        if(!(readBytes < sizeof(localfileheader_t)))
        {
            hdr.signature       = de::littleEndianByteOrder.toNative(hdr.signature);
            hdr.requiredVersion = de::littleEndianByteOrder.toNative(hdr.requiredVersion);
            hdr.flags           = de::littleEndianByteOrder.toNative(hdr.flags);
            hdr.compression     = de::littleEndianByteOrder.toNative(hdr.compression);
            hdr.lastModTime     = de::littleEndianByteOrder.toNative(hdr.lastModTime);
            hdr.lastModDate     = de::littleEndianByteOrder.toNative(hdr.lastModDate);
            hdr.crc32           = de::littleEndianByteOrder.toNative(hdr.crc32);
            hdr.compressedSize  = de::littleEndianByteOrder.toNative(hdr.compressedSize);
            hdr.size            = de::littleEndianByteOrder.toNative(hdr.size);
            hdr.fileNameSize    = de::littleEndianByteOrder.toNative(hdr.fileNameSize);
            hdr.extraFieldSize  = de::littleEndianByteOrder.toNative(hdr.extraFieldSize);
            return true;
        }
        return false;
    }

    static bool readCentralEnd(DFile& file, centralend_t& end)
    {
        size_t readBytes = DFile_Read(&file, (uint8_t*)&end, sizeof(centralend_t));
        if(!(readBytes < sizeof(centralend_t)))
        {
            end.disk            = de::littleEndianByteOrder.toNative(end.disk);
            end.centralStartDisk= de::littleEndianByteOrder.toNative(end.centralStartDisk);
            end.diskEntryCount  = de::littleEndianByteOrder.toNative(end.diskEntryCount);
            end.totalEntryCount = de::littleEndianByteOrder.toNative(end.totalEntryCount);
            end.size            = de::littleEndianByteOrder.toNative(end.size);
            end.offset          = de::littleEndianByteOrder.toNative(end.offset);
            end.commentSize     = de::littleEndianByteOrder.toNative(end.commentSize);
            return true;
        }
        return false;
    }

    /**
     * Finds the central directory end record in the end of the file.
     *
     * @note: This gets awfully slow if the comment is long.
     *
     * @return  @c true= successful.
     */
    bool locateCentralDirectory()
    {
        uint32_t signature;
        // Start from the earliest location where the signature might be.
        int pos = CENTRAL_END_SIZE; // Offset from the end.
        while(pos < MAXIMUM_COMMENT_SIZE)
        {
            DFile_Seek(base._file, -pos, SEEK_END);

            // Is this the signature?
            DFile_Read(base._file, (uint8_t*)&signature, sizeof(signature));
            if(de::littleEndianByteOrder.toNative(signature) == SIG_END_OF_CENTRAL_DIR)
                return true; // Yes, this is it.

            // Move backwards.
            pos++;
        }
        return false;
    }

    void readLumpDirectory()
    {
        LOG_AS("ZipFile");

        // Scan the end of the file for the central directory end record.
        if(!locateCentralDirectory())
            throw de::Error("ZipFile::readLumpDirectory", QString("Central directory in %1 not found").arg(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this)))));

        // Read the central directory end record.
        centralend_t summary;
        readCentralEnd(*base._file, summary);

        // Does the summary say something we don't like?
        if(summary.diskEntryCount != summary.totalEntryCount)
            throw de::Error("ZipFile::readLumpDirectory", QString("Multipart zip file \"%1\" not supported").arg(Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this)))));

        // We'll load the file directory using one continous read into a temporary
        // local buffer before we process it into our runtime representation.
        // Read the entire central directory into memory.
        void* centralDirectory = M_Malloc(summary.size);
        if(!centralDirectory) throw de::Error("ZipFile::readLumpDirectory", QString("Failed on allocation of %1 bytes for temporary copy of the central centralDirectory").arg(summary.size));

        DFile_Seek(base._file, summary.offset, SEEK_SET);
        DFile_Read(base._file, (uint8_t*)centralDirectory, summary.size);

        /**
         * Pass 1: Validate support and count the number of lump records we need.
         * Pass 2: Read all zip entries and populate the lump directory.
         */
        char* pos;
        LumpRecord* record;
        int entryCount = 0;
        ddstring_t entryPath;
        Str_Init(&entryPath);
        for(int pass = 0; pass < 2; ++pass)
        {
            if(pass == 1)
            {
                if(entryCount == 0) break;

                // We can now allocate the records.
                lumpRecords = (LumpRecord*) M_Malloc(entryCount * sizeof(*lumpRecords));
                if(!lumpRecords) throw de::Error("ZipFile::readLumpDirectory", QString("Failed on allocation of %1 bytes for the lump record vector").arg(entryCount * sizeof(*lumpRecords)));

                // Get the first record.
                record = lumpRecords;
            }

            // Position the read cursor at the start of the buffered central centralDirectory.
            pos = (char*)centralDirectory;

            // Read all the entries.
            uint lumpIdx = 0;
            for(int index = 0; index < summary.totalEntryCount; ++index, pos += sizeof(centralfileheader_t))
            {
                centralfileheader_t const* header = (centralfileheader_t*) pos;
                char const* nameStart = pos + sizeof(centralfileheader_t);
                localfileheader_t localHeader;

                // Advance the cursor past the variable sized fields.
                pos += USHORT(header->fileNameSize) + USHORT(header->extraFieldSize) + USHORT(header->commentSize);

                // Copy characters up to fileNameSize.
                Str_Clear(&entryPath);
                Str_PartAppend(&entryPath, nameStart, 0, USHORT(header->fileNameSize));

                // Directories are skipped.
                if(ULONG(header->size) == 0 && Str_RAt(&entryPath, 0) == '/')
                {
                    continue;
                }

                // Do we support the format of this lump?
                if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
                   USHORT(header->compression) != ZFC_DEFLATED)
                {
                    if(pass != 0) continue;
                    LOG_WARNING("Zip %s:'%s' uses an unsupported compression algorithm, ignoring.")
                        << Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this)))
                        << Str_Text(&entryPath);
                }

                if(USHORT(header->flags) & ZFH_ENCRYPTED)
                {
                    if(pass != 0) continue;
                    LOG_WARNING("Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.")
                        << Str_Text(AbstractFile_Path(reinterpret_cast<abstractfile_t*>(this)))
                        << Str_Text(&entryPath);
                }

                if(pass == 0)
                {
                    // Another record will be needed.
                    ++entryCount;
                    continue;
                }

                // Convert all slashes to our internal separator.
                F_FixSlashes(&entryPath, &entryPath);

                // In some cases the path inside the file is mapped to another virtual location.
                ApplyPathMappings(&entryPath, &entryPath);

                // Make it absolute.
                F_PrependBasePath(&entryPath, &entryPath);

                // Have we yet to intialize the directory?
                if(!lumpDirectory)
                {
                    lumpDirectory = PathDirectory_NewWithFlags(PDF_ALLOW_DUPLICATE_LEAF);
                }

                F_InitLumpInfo(&record->info);
                PathDirectoryNode* node = PathDirectory_Insert2(lumpDirectory, Str_Text(&entryPath), '/');
                PathDirectoryNode_SetUserData(node, record);

                record->info.lumpIdx = lumpIdx++;
                record->info.size = ULONG(header->size);
                if(USHORT(header->compression) == ZFC_DEFLATED)
                {
                    // Compressed using the deflate algorithm.
                    record->info.compressedSize = ULONG(header->compressedSize);
                }
                else // No compression.
                {
                    record->info.compressedSize = record->info.size;
                }

                // The modification date is inherited from the real file (note recursion).
                record->info.lastModified = AbstractFile_LastModified(reinterpret_cast<abstractfile_t*>(this));
                record->info.container = reinterpret_cast<abstractfile_t*>(this);

                // Read the local file header, which contains the extra field size (Info-ZIP!).
                DFile_Seek(base._file, ULONG(header->relOffset), SEEK_SET);
                DFile_Read(base._file, (uint8_t*)&localHeader, sizeof(localHeader));

                record->baseOffset = ULONG(header->relOffset) + sizeof(localfileheader_t)
                                   + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);

                // Next record please!
                record++;
            }
        }

        // The file centralDirectory is no longer needed.
        M_Free(centralDirectory);
        Str_Free(&entryPath);
    }

    static int buildLumpDirectoryMapWorker(PathDirectoryNode* node, void* parameters)
    {
        ZipFile* zip = (ZipFile*)parameters;
        LumpRecord* lumpRecord = (LumpRecord*)PathDirectoryNode_UserData(node);
        DENG2_ASSERT(lumpRecord && lumpRecord->info.lumpIdx >= 0 && lumpRecord->info.lumpIdx < zip->lumpCount());
        zip->lumpDirectoryMap[lumpRecord->info.lumpIdx] = node;
        return 0; // Continue iteration.
    }

    void buildLumpDirectoryMap()
    {
        LOG_AS("ZipFile");
        // Been here already?
        if(lumpDirectoryMap) return;

        lumpDirectoryMap = (PathDirectoryNode**) M_Malloc(sizeof(*lumpDirectoryMap) * lumpCount());
        if(!lumpDirectoryMap) throw de::Error("ZipFile::buildLumpDirectoryMap", QString("Failed on allocation of %1 bytes for the lumpdirectory map").arg(sizeof(*lumpDirectoryMap) * lumpCount()));

        PathDirectory_Iterate2(lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH,
                               buildLumpDirectoryMapWorker, (void*)this);
    }

    /**
     * @param buffer  Must be large enough to hold the entire uncompressed data lump.
     */
    size_t bufferLump(LumpRecord const* lumpRecord, uint8_t* buffer)
    {
        DENG2_ASSERT(lumpRecord && buffer);
        LOG_AS("ZipFile");

        DFile_Seek(base._file, lumpRecord->baseOffset, SEEK_SET);

        if(lumpRecord->info.compressedSize != lumpRecord->info.size)
        {
            bool result;
            uint8_t* compressedData = (uint8_t*) M_Malloc(lumpRecord->info.compressedSize);
            if(!compressedData) throw de::Error("ZipFile::bufferLump", QString("Failed on allocation of %1 bytes for decompression buffer").arg(lumpRecord->info.compressedSize));

            // Read the compressed data into a temporary buffer for decompression.
            DFile_Read(base._file, compressedData, lumpRecord->info.compressedSize);

            // Uncompress into the buffer provided by the caller.
            result = uncompressRaw(compressedData, lumpRecord->info.compressedSize,
                                   buffer, lumpRecord->info.size);

            M_Free(compressedData);
            if(!result) return 0; // Inflate failed.
        }
        else
        {
            // Read the uncompressed data directly to the buffer provided by the caller.
            DFile_Read(base._file, buffer, lumpRecord->info.size);
        }
        return lumpRecord->info.size;
    }

    zipfile_s& clearCachedLump(int lumpIdx, bool* retCleared = 0)
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

    int publishLumpsToDirectory(LumpDirectory* directory)
    {
        int numPublished = 0;
        if(directory)
        {
            readLumpDirectory();
            if(lumpCount() > 0)
            {
                // Insert the lumps into their rightful places in the directory.
                LumpDirectory_CatalogLumps(directory, reinterpret_cast<abstractfile_t*>(this), 0, lumpCount());
                numPublished += lumpCount();
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

    zipfile_s& clearLumpCache()
    {
        LOG_AS("ZipFile");
        const int numLumps = lumpCount();
        for(int i = 0; i < numLumps; ++i)
        {
            clearCachedLump(i);
        }
        return *this;
    }

    uint8_t const* cacheLump(int lumpIdx, int tag)
    {
        LOG_AS("ZipFile::cacheLump");
        LumpInfo const* info = lumpInfo(lumpIdx);

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
                lumpCache = (void**) M_Calloc(lumpCount() * sizeof(*lumpCache));
                if(!lumpCache) throw de::Error("ZipFile::cacheLump", QString("Failed on allocation of %1 bytes for lump cache data list").arg(sizeof(*lumpCache)));
            }
        }

        void** cacheAdr = lumpCacheAddress(lumpIdx);
        bool isCached = cacheAdr && *cacheAdr;
        if(!isCached)
        {
            uint8_t* buffer = (uint8_t*) Z_Malloc(info->size, tag, cacheAdr);
            if(!buffer) throw de::Error("ZipFile::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(info->size).arg(lumpIdx));

            readLump(lumpIdx, buffer, false);
        }
        else
        {
            Z_ChangeTag2(*cacheAdr, tag);
        }

        return (uint8_t*)(*cacheAdr);
    }

    zipfile_s& changeLumpCacheTag(int lumpIdx, int tag)
    {
        LOG_AS("ZipFile::changeLumpCacheTag");
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
        LOG_AS("ZipFile::readLumpSection");
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

        size_t readBytes;
        if(!startOffset && length == lrec->info.size)
        {
            // Read it straight to the caller's data buffer.
            readBytes = bufferLump(lrec, buffer);
        }
        else
        {
            // Allocate a temporary buffer and read the whole lump into it(!).
            uint8_t* lumpData = (uint8_t*) M_Malloc(lrec->info.size);
            if(!lumpData) throw de::Error("ZipFile::readLumpSection", QString("Failed on allocation of %1 bytes for work buffer").arg(lrec->info.size));

            if(bufferLump(lrec, lumpData))
            {
                readBytes = MIN_OF(lrec->info.size, length);
                memcpy(buffer, lumpData + startOffset, readBytes);
            }
            else
            {
                readBytes = 0;
            }
            M_Free(lumpData);
        }

        /// @todo Do not check the read length here.
        if(readBytes < MIN_OF(lrec->info.size, length))
            throw de::Error("ZipFile::readLumpSection", QString("Only read %1 of %2 bytes of lump #%3").arg(readBytes).arg(length).arg(lumpIdx));

        return readBytes;
    }

    size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true)
    {
        LOG_AS("ZipFile::readLump");
        LumpInfo const* info = lumpInfo(lumpIdx);
        if(!info) return 0;
        return readLumpSection(lumpIdx, buffer, 0, info->size, tryCache);
    }

    static uint8_t* compressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level)
    {
#define CHUNK_SIZE 32768

        LOG_AS("ZipFile::compressAtLevel");

        z_stream stream;
        uint8_t chunk[CHUNK_SIZE];
        size_t allocSize = CHUNK_SIZE;
        uint8_t* output = (uint8_t*) M_Malloc(allocSize); // some initial space
        int result;
        int have;

        DENG2_ASSERT(outSize);
        *outSize = 0;

        memset(&stream, 0, sizeof(stream));
        stream.next_in = (Bytef*) in;
        stream.avail_in = (uInt) inSize;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        if(level < Z_NO_COMPRESSION)
        {
            level = Z_NO_COMPRESSION;
        }
        if(level > Z_BEST_COMPRESSION)
        {
            level = Z_BEST_COMPRESSION;
        }
        result = deflateInit(&stream, level);
        if(result != Z_OK)
        {
            M_Free(output);
            return 0;
        }

        // Compress until all the data has been exhausted.
        do
        {
            stream.next_out = chunk;
            stream.avail_out = CHUNK_SIZE;
            result = deflate(&stream, Z_FINISH);
            if(result == Z_STREAM_ERROR)
            {
                M_Free(output);
                *outSize = 0;
                return 0;
            }
            have = CHUNK_SIZE - stream.avail_out;
            if(have)
            {
                // Need more memory?
                if(*outSize + have > allocSize)
                {
                    // Need more memory.
                    allocSize *= 2;
                    output = (uint8_t*) M_Realloc(output, allocSize);
                }
                // Append.
                memcpy(output + *outSize, chunk, have);
                *outSize += have;
            }
        } while(!stream.avail_out); // output chunk full, more data may follow

        DENG2_ASSERT(result == Z_STREAM_END);
        DENG2_ASSERT(stream.total_out == *outSize);

        deflateEnd(&stream);
        return output;

#undef CHUNK_SIZE
    }

    static uint8_t* uncompress(uint8_t* in, size_t inSize, size_t* outSize)
    {
#define INF_CHUNK_SIZE 4096 // Uncompress in 4KB chunks.

        LOG_AS("ZipFile::uncompress");

        z_stream stream;
        uint8_t chunk[INF_CHUNK_SIZE];
        size_t allocSize = INF_CHUNK_SIZE;
        uint8_t* output = (uint8_t*) M_Malloc(allocSize); // some initial space
        int result;
        int have;

        DENG2_ASSERT(outSize);
        *outSize = 0;

        memset(&stream, 0, sizeof(stream));
        stream.next_in = (Bytef*) in;
        stream.avail_in = (uInt) inSize;

        result = inflateInit(&stream);
        if(result != Z_OK)
        {
            M_Free(output);
            return 0;
        }

        // Uncompress until all the input data has been exhausted.
        do
        {
            stream.next_out = chunk;
            stream.avail_out = INF_CHUNK_SIZE;
            result = inflate(&stream, Z_FINISH);
            if(result == Z_STREAM_ERROR)
            {
                M_Free(output);
                *outSize = 0;
                return 0;
            }
            have = INF_CHUNK_SIZE - stream.avail_out;
            if(have)
            {
                // Need more memory?
                if(*outSize + have > allocSize)
                {
                    // Need more memory.
                    allocSize *= 2;
                    output = (uint8_t*) M_Realloc(output, allocSize);
                }
                // Append.
                memcpy(output + *outSize, chunk, have);
                *outSize += have;
            }
        } while(!stream.avail_out); // output chunk full, more data may follow

        // We should now be at the end.
        DENG2_ASSERT(result == Z_STREAM_END);

        inflateEnd(&stream);
        return output;

#undef INF_CHUNK_SIZE
    }

    static bool uncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
    {
        LOG_AS("ZipFile::uncompressRaw");
        z_stream stream;
        int result;

        memset(&stream, 0, sizeof(stream));
        stream.next_in = (Bytef*) in;
        stream.avail_in = (uInt) inSize;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.next_out = (Bytef*) out;
        stream.avail_out = (uInt) outSize;

        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            return false;

        // Do the inflation in one call.
        result = inflate(&stream, Z_FINISH);

        if(stream.total_out != outSize)
        {
            inflateEnd(&stream);
            LOG_WARNING("Failure due to %s (result code %i).")
                << (result == Z_DATA_ERROR ? "corrupt data" : "zlib error")
                << result;
            return false;
        }

        // We're done.
        inflateEnd(&stream);
        return true;
    }
};

/**
 * The path inside the zip might be mapped to another virtual location.
 *
 * @todo This is clearly implemented in the wrong place. Path mapping
 *       should be done at a higher level.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 * Key-named directories at the root are mapped to another location.
 */
static void ApplyPathMappings(ddstring_t* dest, const ddstring_t* src)
{
    // Manually mapped to Defs?
    if(Str_At(src, 0) == '@')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);
        int dist;

        Str_Appendf(out, "%sauto/", Str_Text(Game_DefsPath(theGame)));
        dist = (Str_At(src, 1) == '/'? 2 : 1);
        Str_PartAppend(out, Str_Text(src), dist, Str_Length(src)-dist);

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    // Manually mapped to Data?
    if(Str_At(src, 0) == '#')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);
        int dist = 0;
        char* slash;

        Str_Appendf(out, "%sauto/", Str_Text(Game_DataPath(theGame)));
        slash = strrchr(Str_Text(src), '/');
        dist = slash - Str_Text(src);
        // Copy the path up to and including the last directory separator if present.
        if(slash - Str_Text(src) > 1)
            Str_PartAppend(out, Str_Text(src), 1, dist);

        if(slash)
        {
            // Is there a prefix to be omitted in the name?
            // The slash must not be too early in the string.
            if(slash >= Str_Text(src) + 2)
            {
                // Good old negative indices.
                if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
                    dist += slash[-1] - '1' + 1;
            }
        }

        Str_PartAppend(out, Str_Text(src), dist+1, Str_Length(src)-(dist+1));

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    if(strchr(Str_Text(src), '/') == NULL)
    {
        // No directory separators; i.e., a root file.
        resourcetype_t type = F_GuessResourceTypeByName(Str_Text(src));
        resourceclass_t rclass;
        ddstring_t mapped;

        // Certain resource files require special handling.
        // Something of a kludge, at this level.
        switch(type)
        {
        case RT_DEH: // Treat DeHackEd patches as packages so they are mapped to Data.
            rclass = RC_PACKAGE;
            break;
        case RT_NONE: { // *.lmp files must be mapped to Data.
            const char* ext = F_FindFileExtension(Str_Text(src));
            if(ext && !stricmp("lmp", ext))
            {
                rclass = RC_PACKAGE;
            }
            else
            {
                rclass = RC_UNKNOWN;
            }
            break;
          }
        default:
            rclass = F_DefaultResourceClassForType(type);
            break;
        }
        // Kludge end

        Str_Init(&mapped);
        switch(rclass)
        {
        case RC_PACKAGE: // Mapped to the Data directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(Game_DataPath(theGame)));
            break;
        case RC_DEFINITION: // Mapped to the Defs directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(Game_DefsPath(theGame)));
            break;
        default: /* Not mapped */ break;
        }
        Str_Append(&mapped, Str_Text(src));
        Str_Copy(dest, &mapped);

        Str_Free(&mapped);
        return;
    }

    // There is at least one level of directory structure.

    if(dest != src)
        Str_Copy(dest, src);

    // Key-named directories in the root might be mapped to another location.
    F_ApplyPathMapping(dest);
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<zipfile_s*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<zipfile_s const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    zipfile_s* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    zipfile_s const* self = TOINTERNAL_CONST(inst)

ZipFile* ZipFile_New(DFile* file, const char* path, const LumpInfo* info)
{
    if(!info) LegacyCore_FatalError("ZipFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<ZipFile*>(new zipfile_s(*file, path, *info));
    }
    catch(de::Error& er)
    {
        QString msg = QString("ZipFile_New: Failed to instantiate new ZipFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void ZipFile_Delete(ZipFile* zip)
{
    if(zip)
    {
        SELF(zip);
        delete self;
    }
}

int ZipFile_PublishLumpsToDirectory(ZipFile* zip, LumpDirectory* directory)
{
    SELF(zip);
    return self->publishLumpsToDirectory(directory);
}

PathDirectoryNode* ZipFile_LumpDirectoryNode(ZipFile* zip, int lumpIdx)
{
    SELF(zip);
    return self->lumpDirectoryNode(lumpIdx);
}

AutoStr* ZipFile_ComposeLumpPath(ZipFile* zip, int lumpIdx, char delimiter)
{
    SELF(zip);
    return self->composeLumpPath(lumpIdx, delimiter);
}

LumpInfo const* ZipFile_LumpInfo(ZipFile* zip, int lumpIdx)
{
    SELF(zip);
    return self->lumpInfo(lumpIdx);
}

void ZipFile_ClearLumpCache(ZipFile* zip)
{
    SELF(zip);
    self->clearLumpCache();
}

uint8_t* ZipFile_Compress(uint8_t* in, size_t inSize, size_t* outSize)
{
    return ZipFile_CompressAtLevel(in, inSize, outSize, Z_DEFAULT_COMPRESSION);
}

uint8_t* ZipFile_CompressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level)
{
    return zipfile_s::compressAtLevel(in, inSize, outSize, level);
}

uint8_t* ZipFile_Uncompress(uint8_t* in, size_t inSize, size_t* outSize)
{
    return zipfile_s::uncompress(in, inSize, outSize);
}

boolean ZipFile_UncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
{
    return zipfile_s::uncompressRaw(in, inSize, out, outSize);
}

size_t ZipFile_ReadLumpSection2(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    SELF(zip);
    return self->readLumpSection(lumpIdx, buffer, startOffset, length, tryCache);
}

size_t ZipFile_ReadLumpSection(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    SELF(zip);
    return self->readLumpSection(lumpIdx, buffer, startOffset, length);
}

size_t ZipFile_ReadLump2(ZipFile* zip, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    SELF(zip);
    return self->readLump(lumpIdx, buffer, tryCache);
}

size_t ZipFile_ReadLump(ZipFile* zip, int lumpIdx, uint8_t* buffer)
{
    SELF(zip);
    return self->readLump(lumpIdx, buffer);
}

uint8_t const* ZipFile_CacheLump(ZipFile* zip, int lumpIdx, int tag)
{
    SELF(zip);
    return self->cacheLump(lumpIdx, tag);
}

void ZipFile_ChangeLumpCacheTag(ZipFile* zip, int lumpIdx, int tag)
{
    SELF(zip);
    self->changeLumpCacheTag(lumpIdx, tag);
}

int ZipFile_LumpCount(ZipFile* zip)
{
    SELF(zip);
    return self->lumpCount();
}

boolean ZipFile_Recognise(DFile* file)
{
    if(!file) return false;
    return zipfile_s::recognise(*file);
}
