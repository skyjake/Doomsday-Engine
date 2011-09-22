/**\file dd_zip.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <zlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "zipfile.h"

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

/**
 * The path inside the zip might be mapped to another virtual location.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 * Key-named directories at the root are mapped to another location.
 */
static void ZipFile_ApplyPathMappings(ddstring_t* dest, const ddstring_t* src)
{
    // Manually mapped to Defs?
    if(Str_At(src, 0) == '@')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);

        Str_Appendf(out, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DefsPath(DD_GameInfo())));
        Str_PartAppend(out, Str_Text(src), 1, Str_Length(src)-1);

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

        Str_Appendf(out, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DataPath(DD_GameInfo())));
        Str_PartAppend(out, Str_Text(src), 1, Str_Length(src)-1);

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    if(strchr(Str_Text(src), DIR_SEP_CHAR) == NULL)
    {   // No directory separators; i.e., a root file.
        resourcetype_t type = F_GuessResourceTypeByName(Str_Text(src));
        resourceclass_t rclass;
        ddstring_t mapped;

        /// \kludge Treat DeHackEd patches as packages so they are mapped to Data.
        rclass = (type == RT_DEH? RC_PACKAGE : F_DefaultResourceClassForType(type));
        /// < kludge end

        Str_Init(&mapped);
        switch(rclass)
        {
        case RC_UNKNOWN: // Not mapped.
            break;
        case RC_DEFINITION: // Mapped to the Defs directory.
            Str_Appendf(&mapped, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DefsPath(DD_GameInfo())));
            break;
        default: // Some other type of known resource. Mapped to the Data directory.
            Str_Appendf(&mapped, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            break;
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
 * Finds the central directory end record in the end of the file.
 * Note: This gets awfully slow if the comment is long.
 *
 * @return  @c true, if successful.
 */
static boolean ZipFile_LocateCentralDirectory(zipfile_t* zip)
{
    assert(zip);
    {
    int pos = CENTRAL_END_SIZE; // Offset from the end.
    uint32_t signature;

    // Start from the earliest location where the signature might be.
    while(pos < MAXIMUM_COMMENT_SIZE)
    {
        DFile_Seek(zip->_base._file, -pos, SEEK_END);

        // Is this the signature?
        DFile_Read(zip->_base._file, (uint8_t*)&signature, 4);
        if(ULONG(signature) == SIG_END_OF_CENTRAL_DIR)
        {
            // This is it!
            return true;
        }
        // Move backwards.
        pos++;
    }
    // Scan was not successful.
    return false;
    }
}

static void ZipFile_ReadLumpDirectory(zipfile_t* zip)
{
    assert(zip);
    {
    zipfile_lumprecord_t* record = NULL; // Shutup gcc!
    int index, entryCount, pass;
    void* centralDirectory;
    ddstring_t entryPath;
    centralend_t summary;
    char* pos;

    VERBOSE( Con_Message("ZipFile::readLumpDirectory: \"%s\"\n",
        F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip)))) );

    // Scan the end of the file for the central centralDirectory end record.
    if(!ZipFile_LocateCentralDirectory(zip))
    {
        Con_Error("ZipFile::readLumpDirectory: Central centralDirectory in %s not found!",
            Str_Text(AbstractFile_Path((abstractfile_t*)zip)));
    }

    // Read the central centralDirectory end record.
    DFile_Read(zip->_base._file, (uint8_t*)&summary, sizeof(summary));

    // Does the summary say something we don't like?
    if(USHORT(summary.diskEntryCount) != USHORT(summary.totalEntryCount))
    {
        Con_Error("ZipFile::readLumpDirectory: Multipart Zip file \"%s\" not supported.",
            Str_Text(AbstractFile_Path((abstractfile_t*)zip)));
    }

    // Read the entire central centralDirectory into memory.
    centralDirectory = malloc(ULONG(summary.size));
    if(NULL == centralDirectory)
        Con_Error("ZipFile::readLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary copy of the central centralDirectory.", (unsigned long) ULONG(summary.size));
    DFile_Seek(zip->_base._file, ULONG(summary.offset), SEEK_SET);
    DFile_Read(zip->_base._file, (uint8_t*)centralDirectory, ULONG(summary.size));

    /**
     * Pass 1: Validate support and count the number of lump entries we need.
     * Pass 2: Read all the entries.
     */
    entryCount = 0;
    Str_Init(&entryPath);
    for(pass = 0; pass < 2; ++pass)
    {
        // Position the read cursor at the start of the buffered central centralDirectory.
        pos = centralDirectory;

        if(pass == 1)
        {
            // We can now allocate the record list.
            zip->_lumpRecords = (zipfile_lumprecord_t*)malloc(entryCount * sizeof *zip->_lumpRecords);
            if(NULL == zip->_lumpRecords)
                Con_Error("ZipFile::readLumpDirectory: Failed on allocation of %lu bytes for record list.",
                    (unsigned long) (entryCount * sizeof *zip->_lumpRecords));
            zip->_lumpCount = entryCount;

            // Get the first record.
            record = zip->_lumpRecords;
        }

        // Read all the entries.
        for(index = 0; index < USHORT(summary.totalEntryCount);
            ++index, pos += sizeof(centralfileheader_t))
        {
            const centralfileheader_t* header = (void*) pos;
            const char* nameStart = pos + sizeof(centralfileheader_t);
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
                Con_Message("Warning: Zip %s:'%s' uses an unsupported compression algorithm, ignoring.\n",
                            Str_Text(AbstractFile_Path((abstractfile_t*)zip)), Str_Text(&entryPath));
            }

            if(USHORT(header->flags) & ZFH_ENCRYPTED)
            {
                if(pass != 0) continue;
                Con_Message("Warning: Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.\n",
                            Str_Text(AbstractFile_Path((abstractfile_t*)zip)), Str_Text(&entryPath));
            }

            if(pass == 0)
            {
                // Another record will be needed.
                ++entryCount;
                continue;
            }

            // Convert all slashes to the host OS's centralDirectory separator,
            // for compatibility with the sys_filein routines.
            F_FixSlashes(&entryPath, &entryPath);

            // In some cases the path inside the file is mapped to another virtual location.
            ZipFile_ApplyPathMappings(&entryPath, &entryPath);

            // Make it absolute.
            F_PrependBasePath(&entryPath, &entryPath);

            // Take a copy of the name.
            Str_Init(&record->info.path); Str_Set(&record->info.path, Str_Text(&entryPath));
            memset(record->info.name, 0, sizeof(record->info.name));
            record->info.size = ULONG(header->size);
            if(USHORT(header->compression) == ZFC_DEFLATED)
            {   // Compressed using the deflate algorithm.
                record->info.compressedSize = ULONG(header->compressedSize);
            }
            else
            {   // No compression.
                record->info.compressedSize = record->info.size;
            }

            // The modification date is inherited from the real file (note recursion).
            record->info.lastModified = AbstractFile_LastModified((abstractfile_t*)zip);

            // Read the local file header, which contains the extra field size (Info-ZIP!).
            DFile_Seek(zip->_base._file, ULONG(header->relOffset), SEEK_SET);
            DFile_Read(zip->_base._file, (uint8_t*)&localHeader, sizeof(localHeader));

            record->info.lumpIdx = index;
            record->info.container = (abstractfile_t*)zip;
            record->baseOffset = ULONG(header->relOffset) + sizeof(localfileheader_t) + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);

            // Next record please!
            record++;
        }
    }

    // The file centralDirectory is no longer needed.
    free(centralDirectory);
    Str_Free(&entryPath);
    }
}

zipfile_t* ZipFile_New(DFile* file, const lumpinfo_t* info)
{
    assert(info);
    {
    zipfile_t* zip = (zipfile_t*)malloc(sizeof *zip);
    if(!zip) Con_Error("ZipFile::New: Failed on allocation of %lu bytes for new ZipFile.",
                (unsigned long) sizeof *zip);

    AbstractFile_Init((abstractfile_t*)zip, FT_ZIPFILE, file, info);
    zip->_lumpCount = 0;
    zip->_lumpRecords = NULL;
    zip->_lumpCache = NULL;
    return zip;
    }
}

int ZipFile_PublishLumpsToDirectory(zipfile_t* zip, lumpdirectory_t* directory)
{
    assert(zip && directory);
    {
    int numPublished = 0;
    ZipFile_ReadLumpDirectory(zip);
    if(zip->_lumpCount > 0)
    {
        // Insert the lumps into their rightful places in the directory.
        LumpDirectory_Append(directory, (abstractfile_t*)zip, 0, zip->_lumpCount);
        LumpDirectory_PruneDuplicateRecords(directory, false);
        numPublished += zip->_lumpCount;
    }
    return numPublished;
    }
}

const lumpinfo_t* ZipFile_LumpInfo(zipfile_t* zip, int lumpIdx)
{
    assert(zip);
    if(lumpIdx < 0 || lumpIdx >= zip->_lumpCount)
    {
        Con_Error("ZipFile::LumpInfo: Invalid lump index %i (valid range: [0...%i]).", lumpIdx, zip->_lumpCount);
        exit(1); // Unreachable.
    }
    return &zip->_lumpRecords[lumpIdx].info;
}

void ZipFile_Delete(zipfile_t* zip)
{
    assert(zip);
    F_ReleaseFile((abstractfile_t*)zip);
    ZipFile_ClearLumpCache(zip);
    if(zip->_lumpCount > 1 && NULL != zip->_lumpCache)
    {
        free(zip->_lumpCache);
    }
    if(zip->_lumpCount != 0)
    {
        int i;
        for(i = 0; i < zip->_lumpCount; ++i)
            F_DestroyLumpInfo(&zip->_lumpRecords[i].info);
        free(zip->_lumpRecords);
    }
    AbstractFile_Destroy((abstractfile_t*)zip);
    free(zip);
}

void ZipFile_ClearLumpCache(zipfile_t* zip)
{
    assert(zip);

    if(zip->_lumpCount == 1)
    {
        if(zip->_lumpCache)
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(zip->_lumpCache) < PU_MAP)
                Z_ChangeTag(zip->_lumpCache, PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(zip->_lumpCache, (void*) 0x2);
        }
        return;
    }

    if(NULL == zip->_lumpCache) return;

    { int i;
    for(i = 0; i < zip->_lumpCount; ++i)
    {
        if(!zip->_lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(zip->_lumpCache[i]) < PU_MAP)
            Z_ChangeTag(zip->_lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(zip->_lumpCache[i], (void*) 0x2);
    }}
}

/**
 * Use zlib to inflate a compressed lump.
 * @return  @c true if successful.
 */
static boolean ZipFile_InflateLump(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
{
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
        Con_Message("ZipFile::InflateLump: Failure due to %s.\n", (result == Z_DATA_ERROR ? "corrupt data" : "zlib error"));
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

static size_t ZipFile_BufferLump(zipfile_t* zip, const lumpinfo_t* lumpInfo, uint8_t* buffer)
{
    assert(zip && lumpInfo && buffer);
    DFile_Seek(zip->_base._file, zip->_lumpRecords[lumpInfo->lumpIdx].baseOffset, SEEK_SET);
    if(lumpInfo->compressedSize != lumpInfo->size)
    {
        boolean result;
        uint8_t* compressedData = (uint8_t*)malloc(lumpInfo->compressedSize);
        if(NULL == compressedData)
            Con_Error("ZipFile::BufferLump: Failed on allocation of %lu bytes for decompression buffer.", lumpInfo->compressedSize);

        // Read the compressed data into a temporary buffer for decompression.
        DFile_Read(zip->_base._file, compressedData, lumpInfo->compressedSize);
        result = ZipFile_InflateLump(compressedData, lumpInfo->compressedSize, buffer, lumpInfo->size);
        free(compressedData);
        if(!result)
            return 0; // Inflate failed.
    }
    else
    {
        // Read the uncompressed data directly to the buffer provided by the caller.
        DFile_Read(zip->_base._file, buffer, lumpInfo->size);
    }
    return lumpInfo->size;
}

size_t ZipFile_ReadLumpSection2(zipfile_t* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(zip);
    {
    const lumpinfo_t* info = ZipFile_LumpInfo(zip, lumpIdx);
    if(!info) return 0;

    VERBOSE2(
        Con_Printf("ZipFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip))),
                F_PrettyPath(Str_Text(&info->path)), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length) )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && zip->_lumpCache)
    {
        size_t readBytes;
        boolean isCached;
        void** cachePtr;

        if(zip->_lumpCount > 1)
        {
            const uint cacheIdx = lumpIdx;
            cachePtr = &zip->_lumpCache[cacheIdx];
            isCached = (NULL != zip->_lumpCache[cacheIdx]);
        }
        else
        {
            cachePtr = (void**)&zip->_lumpCache;
            isCached = (NULL != zip->_lumpCache);
        }

        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(info->size, length);
            memcpy(buffer, (uint8_t*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    return ZipFile_BufferLump(zip, info, buffer);
    }
}

size_t ZipFile_ReadLumpSection(zipfile_t* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return ZipFile_ReadLumpSection2(zip, lumpIdx, buffer, startOffset, length, true);
}

size_t ZipFile_ReadLump2(zipfile_t* zip, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const lumpinfo_t* info = ZipFile_LumpInfo(zip, lumpIdx);
    if(!info) return 0;
    return ZipFile_ReadLumpSection2(zip, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t ZipFile_ReadLump(zipfile_t* zip, int lumpIdx, uint8_t* buffer)
{
    return ZipFile_ReadLump2(zip, lumpIdx, buffer, true);
}

const uint8_t* ZipFile_CacheLump(zipfile_t* zip, int lumpIdx, int tag)
{
    assert(zip);
    {
    const lumpinfo_t* info = ZipFile_LumpInfo(zip, lumpIdx);
    const uint cacheIdx = lumpIdx;
    boolean isCached;
    void** cachePtr;

    VERBOSE2(
        Con_Printf("ZipFile::CacheLump: \"%s:%s\" (%lu bytes%s)",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip))),
                F_PrettyPath(Str_Text(&info->path)), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : "")) )

    if(zip->_lumpCount > 1)
    {
        // Time to allocate the cache ptr table?
        if(NULL == zip->_lumpCache)
            zip->_lumpCache = (void**)calloc(zip->_lumpCount, sizeof *zip->_lumpCache);
        cachePtr = &zip->_lumpCache[cacheIdx];
        isCached = (NULL != zip->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&zip->_lumpCache;
        isCached = (NULL != zip->_lumpCache);
    }

    VERBOSE2( Con_Printf(" %s\n", isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("ZipFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        ZipFile_ReadLump2(zip, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
    }
}

void ZipFile_ChangeLumpCacheTag(zipfile_t* zip, int lumpIdx, int tag)
{
    assert(zip);
    {
    boolean isCached;
    void** cachePtr;

    if(zip->_lumpCount > 1)
    {
        const uint cacheIdx = lumpIdx;
        if(NULL == zip->_lumpCache) return; // Obviously not cached.

        cachePtr = &zip->_lumpCache[cacheIdx];
        isCached = (NULL != zip->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&zip->_lumpCache;
        isCached = (NULL != zip->_lumpCache);
    }

    if(isCached)
    {
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

int ZipFile_LumpCount(zipfile_t* zip)
{
    assert(zip);
    return zip->_lumpCount;
}

boolean ZipFile_Recognise(DFile* file)
{
    boolean knownFormat = false;
    localfileheader_t hdr;
    size_t readBytes, initPos = DFile_Tell(file);
    DFile_Seek(file, 0, SEEK_SET);
    readBytes = DFile_Read(file, (uint8_t*)&hdr, sizeof(hdr));
    if(!(readBytes < sizeof(hdr)))
    {
        // Seek to the start of the signature.
        if(ULONG(hdr.signature) == SIG_LOCAL_FILE_HEADER)
        {
            knownFormat = true;
        }
    }
    // Reposition the stream in case another handler needs to process this file.
    DFile_Seek(file, initPos, SEEK_SET);
    return knownFormat;
}
