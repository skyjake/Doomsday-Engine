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

#include "lumpdirectory.h"
#include "dd_zip.h"

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
static boolean ZipFile_LocateCentralDirectory(zipfile_t* file)
{
    DFILE* handle = file->_base._handle;
    int pos = CENTRAL_END_SIZE; // Offset from the end.
    uint32_t signature;

    // Start from the earliest location where the signature might be.
    while(pos < MAXIMUM_COMMENT_SIZE)
    {
        F_Seek(handle, -pos, SEEK_END);

        // Is this the signature?
        F_Read(handle, &signature, 4);
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

static void ZipFile_ReadArchiveLumpDirectory(zipfile_t* file)
{
    assert(NULL != file);
    {
    int index, entryCount, pass;
    ddstring_t entryPath;
    centralend_t summary;
    lumpinfo_t* info;
    void* centralDirectory;
    char* pos;

    // Do we need to (re)open this file?
    if(NULL == file->_base._handle)
    {
        file->_base._handle = F_Open(Str_Text(&file->_base._absolutePath), "rb");
        if(NULL == file->_base._handle)
        {
            Con_Message("Warning:ZipFile::readArchiveFileDirectory: \"%s\" not found!\n", Str_Text(&file->_base._absolutePath));
            return;
        }
    }

    VERBOSE( Con_Message("ZipFile::readArchiveFileDirectory: \"%s\"\n", F_PrettyPath(Str_Text(&file->_base._absolutePath))) );

    // Scan the end of the file for the central centralDirectory end record.
    if(!ZipFile_LocateCentralDirectory(file))
    {
        Con_Error("ZipFile::readArchiveFileDirectory: Central centralDirectory in %s not found!", Str_Text(&file->_base._absolutePath));
    }

    // Read the central centralDirectory end record.
    F_Read(file->_base._handle, &summary, sizeof(summary));

    // Does the summary say something we don't like?
    if(USHORT(summary.diskEntryCount) != USHORT(summary.totalEntryCount))
    {
        Con_Error("ZipFile::readArchiveFileDirectory: Multipart Zip file \"%s\" not supported.", Str_Text(&file->_base._absolutePath));
    }

    // Read the entire central centralDirectory into memory.
    centralDirectory = malloc(ULONG(summary.size));
    if(NULL == centralDirectory)
        Con_Error("ZipFile::readArchiveFileDirectory: Failed on allocation of %lu bytes for temporary copy of the central centralDirectory.", (unsigned long) ULONG(summary.size));
    F_Seek(file->_base._handle, ULONG(summary.offset), SEEK_SET);
    F_Read(file->_base._handle, centralDirectory, ULONG(summary.size));

    /**
     * Pass 1: Validate support and count the number of file entries we need.
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
            // We can now allocate the zipentry list.
            file->_lumpInfo = (lumpinfo_t*)malloc(sizeof(*file->_lumpInfo) * entryCount);
            if(NULL == file->_lumpInfo)
                Con_Error("ZipFile::readArchiveFileDirectory: Failed on allocation of %lu bytes for file file list.",
                    (unsigned long) (sizeof(*file->_lumpInfo) * entryCount));
            file->_lumpCount = entryCount;

            // Get the first info.
            info = file->_lumpInfo;
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

            // Do we support the format of this file?
            if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
               USHORT(header->compression) != ZFC_DEFLATED)
            {
                if(pass != 0) continue;
                Con_Message("Warning: Zip %s:'%s' uses an unsupported compression algorithm, ignoring.\n",
                            Str_Text(&file->_base._absolutePath), Str_Text(&entryPath));
            }

            if(USHORT(header->flags) & ZFH_ENCRYPTED)
            {
                if(pass != 0) continue;
                Con_Message("Warning: Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.\n",
                            Str_Text(&file->_base._absolutePath), Str_Text(&entryPath));
            }

            if(pass == 0)
            {
                // Another info will be needed.
                ++entryCount;
                continue;
            }

            // Convert all slashes to the host OS's centralDirectory separator,
            // for compatibility with the sys_filein routines.
            F_FixSlashes(&entryPath, &entryPath);

            // In some cases the path inside the zip is mapped to another virtual location.
            ZipFile_ApplyPathMappings(&entryPath, &entryPath);

            // Make it absolute.
            F_PrependBasePath(&entryPath, &entryPath);

            // Take a copy of the name. This is freed in Zip_Shutdown().
            Str_Init(&info->path); Str_Set(&info->path, Str_Text(&entryPath));
            memset(info->name, 0, sizeof(info->name));
            info->size = ULONG(header->size);
            if(USHORT(header->compression) == ZFC_DEFLATED)
            {   // Compressed using the deflate algorithm.
                info->compressedSize = ULONG(header->compressedSize);
            }
            else
            {   // No compression.
                info->compressedSize = info->size;
            }

            // The modification date is inherited from the real file (note recursion).
            info->lastModified = file->_base._handle->lastModified;

            // Read the local file header, which contains the extra field size (Info-ZIP!).
            F_Seek(file->_base._handle, ULONG(header->relOffset), SEEK_SET);
            F_Read(file->_base._handle, &localHeader, sizeof(localHeader));

            info->baseOffset = ULONG(header->relOffset) + sizeof(localfileheader_t) + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);

            // Next info please!
            info++;
        }
    }

    // The zip centralDirectory is no longer needed.
    free(centralDirectory);
    Str_Free(&entryPath);

    // Populate the directory.
    LumpDirectory_Append(AbstractFile_Directory((abstractfile_t*)file), file->_lumpInfo, file->_lumpCount, (abstractfile_t*)file);
    LumpDirectory_PruneDuplicateRecords(AbstractFile_Directory((abstractfile_t*)file), false);
    }
}

zipfile_t* ZipFile_Construct(DFILE* handle, const char* absolutePath, lumpdirectory_t* directory)
{
    zipfile_t* file = (zipfile_t*)malloc(sizeof(*file));
    if(NULL == file)
        Con_Error("ZipFile::Construct: Failed on allocation of %lu bytes for new ZipFile.",
            (unsigned long) sizeof(*file));

    file->_lumpCount = 0;
    file->_lumpInfo = NULL;
    file->_lumpCache = NULL;
    AbstractFile_Init((abstractfile_t*)file, FT_ZIPFILE, handle, absolutePath, directory);

    /// \todo Defer this operation.
    ZipFile_ReadArchiveLumpDirectory(file);

    return file;
}

void ZipFile_Destruct(zipfile_t* file)
{
    assert(NULL != file);
    ZipFile_ClearLumpCache(file);
    if(file->_lumpCount > 1 && NULL != file->_lumpCache)
    {
        free(file->_lumpCache);
    }
    if(file->_lumpCount != 0)
    {
        int i;
        for(i = 0; i < file->_lumpCount; ++i)
            Str_Free(&file->_lumpInfo[i].path);
        free(file->_lumpInfo);
    }
    Str_Free(&file->_base._absolutePath);
    free(file);
}

void ZipFile_ClearLumpCache(zipfile_t* file)
{
    assert(NULL != file);

    if(file->_lumpCount == 1)
    {
        if(file->_lumpCache)
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(file->_lumpCache) < PU_MAP)
                Z_ChangeTag(file->_lumpCache, PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(file->_lumpCache, (void*) 0x2);
        }
        return;
    }

    if(NULL == file->_lumpCache) return;

    { int i;
    for(i = 0; i < file->_lumpCount; ++i)
    {
        if(!file->_lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(file->_lumpCache[i]) < PU_MAP)
            Z_ChangeTag(file->_lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(file->_lumpCache[i], (void*) 0x2);
    }}
}

static __inline uint ZipFile_CacheIndexForLump(const zipfile_t* file,
    const lumpinfo_t* info)
{
    assert(NULL != file && NULL != info);
    assert((info - file->_lumpInfo) >= 0 && (info - file->_lumpInfo) < file->_lumpCount);
    return info - file->_lumpInfo;
}

/**
 * Use zlib to inflate a compressed lump.
 * @return  @c true if successful.
 */
static boolean ZipFile_InflateLump(char* in, size_t inSize, char* out, size_t outSize)
{
    z_stream stream;
    int result;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*) in;
    stream.avail_in = (uInt) inSize;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.next_out = out;
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

static size_t ZipFile_BufferLump(zipfile_t* file, const lumpinfo_t* lumpInfo, char* buffer)
{
    assert(NULL != file && NULL != lumpInfo && NULL != buffer);
    F_Seek(file->_base._handle, lumpInfo->baseOffset, SEEK_SET);
    if(lumpInfo->compressedSize != lumpInfo->size)
    {
        boolean result;
        char* compressedData = (char*)malloc(lumpInfo->compressedSize);
        if(NULL == compressedData)
            Con_Error("ZipFile::BufferLump: Failed on allocation of %lu bytes for decompression buffer.", lumpInfo->compressedSize);

        // Read the compressed data into a temporary buffer for decompression.
        F_Read(file->_base._handle, compressedData, lumpInfo->compressedSize);
        result = ZipFile_InflateLump(compressedData, lumpInfo->compressedSize, buffer, lumpInfo->size);
        free(compressedData);
        if(!result)
            return 0; // Inflate failed.
    }
    else
    {
        // Read the uncompressed data directly to the buffer provided by the caller.
        F_Read(file->_base._handle, buffer, lumpInfo->size);
    }
    return lumpInfo->size;
}

void ZipFile_ReadLumpSection2(zipfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);

    VERBOSE2(
        Con_Message("ZipFile::ReadLump: \"%s:%s\" (%lu bytes%s)\n", F_PrettyPath(Str_Text(&file->_base._absolutePath)),
                F_PrettyPath(Str_Text(&info->path)), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : "")) );

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != file->_lumpCache)
    {
        boolean isCached;
        void** cachePtr;

        if(file->_lumpCount > 1)
        {
            uint cacheIdx = ZipFile_CacheIndexForLump(file, info);
            cachePtr = &file->_lumpCache[cacheIdx];
            isCached = (NULL != file->_lumpCache[cacheIdx]);
        }
        else
        {
            cachePtr = (void**)&file->_lumpCache;
            isCached = (NULL != file->_lumpCache);
        }

        if(isCached)
        {
            memcpy(buffer, (char*)*cachePtr + startOffset, MIN_OF(info->size, length));
            return;
        }
    }

    ZipFile_BufferLump(file, info, buffer);
    }
}

void ZipFile_ReadLumpSection(zipfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length)
{
    ZipFile_ReadLumpSection2(file, lumpNum, buffer, startOffset, length, true);
}

void ZipFile_ReadLump2(zipfile_t* file, lumpnum_t lumpNum, char* buffer, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    ZipFile_ReadLumpSection2(file, lumpNum, buffer, 0, info->size, tryCache);
    }
}

void ZipFile_ReadLump(zipfile_t* file, lumpnum_t lumpNum, char* buffer)
{
    ZipFile_ReadLump2(file, lumpNum, buffer, true);
}

const char* ZipFile_CacheLump(zipfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    uint cacheIdx = ZipFile_CacheIndexForLump(file, info);
    boolean isCached;
    void** cachePtr;

    assert(NULL != info);

    if(file->_lumpCount > 1)
    {
        // Time to allocate the cache ptr table?
        if(NULL == file->_lumpCache)
            file->_lumpCache = (void**)calloc(file->_lumpCount, sizeof(*file->_lumpCache));
        cachePtr = &file->_lumpCache[cacheIdx];
        isCached = (NULL != file->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&file->_lumpCache;
        isCached = (NULL != file->_lumpCache);
    }

    if(!isCached)
    {
        char* ptr = (char*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("ZipFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpNum);
        ZipFile_ReadLump2(file, lumpNum, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (char*)(*cachePtr);
    }
}

void ZipFile_ChangeLumpCacheTag(zipfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    boolean isCached;
    void** cachePtr;

    if(file->_lumpCount > 1)
    {
        uint cacheIdx;
        if(NULL == file->_lumpCache) return; // Obviously not cached.

        cacheIdx = ZipFile_CacheIndexForLump(file, LumpDirectory_LumpInfo(file->_base._directory, lumpNum));
        cachePtr = &file->_lumpCache[cacheIdx];
        isCached = (NULL != file->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&file->_lumpCache;
        isCached = (NULL != file->_lumpCache);
    }

    if(isCached)
    {
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

void ZipFile_Close(zipfile_t* file)
{
    assert(NULL != file);
    if(NULL != file->_base._handle)
    {
        F_Close(file->_base._handle), file->_base._handle = NULL;
    }
    F_ReleaseFileId(Str_Text(&file->_base._absolutePath));
}

int ZipFile_LumpCount(zipfile_t* file)
{
    assert(NULL != file);
    return file->_lumpCount;
}

boolean ZipFile_IsIWAD(zipfile_t* file)
{
    return false; // Never.
}

boolean ZipFile_Recognise(DFILE* handle)
{
    boolean knownFormat = false;
    size_t initPos = F_Tell(handle);
    if(F_Length(handle) >= sizeof(localfileheader_t))
    {
        uint32_t signature;
        // Seek to the start of the signature.
        F_Rewind(handle);
        F_Read(handle, &signature, 4);
        if(ULONG(signature) == SIG_LOCAL_FILE_HEADER)
        {
            knownFormat = true;
        }
    }
    // Reposition the file stream in case another handler needs to process this file.
    F_Seek(handle, initPos, SEEK_SET);
    return knownFormat;
}
