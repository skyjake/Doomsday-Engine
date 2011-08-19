/**\file wadfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#include "de_base.h"
#include "de_console.h"

#include "lumpdirectory.h"
#include "wadfile.h"

/**
 * @defgroup wadFileFlags  Wad file flags.
 */
/*@{*/
#define WFF_IWAD                    0x1 // File is marked IWAD (else its a PWAD).
/*@}*/

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

static void WadFile_ReadLumpDirectory(wadfile_t* file);

static boolean WadFile_ReadArchiveHeader(DFILE* handle, wadheader_t* hdr)
{
    assert(NULL != handle && NULL != hdr);
    // Seek to the start of the header.
    F_Rewind(handle);
    if(!(F_Read(handle, hdr, sizeof(wadheader_t)) < sizeof(wadheader_t)))
    {
        hdr->lumpRecordsCount  = LONG(hdr->lumpRecordsCount);
        hdr->lumpRecordsOffset = LONG(hdr->lumpRecordsOffset);
        return true;
    }
    return false;
}

// We'll load the lump directory using one continous read into a temporary
// local buffer before we process it into our runtime representation.
static lumpinfo_t* WadFile_ReadArchiveLumpDirectory(wadfile_t* file,
    size_t lumpRecordOffset, int lumpRecordCount, int* numLumpsRead)
{
    assert(NULL != file);
    {
    size_t lumpRecordsSize = lumpRecordCount * sizeof(wadlumprecord_t);
    wadlumprecord_t* lumpRecords = (wadlumprecord_t*)malloc(lumpRecordsSize);
    DFILE* handle = file->_base._handle;
    lumpinfo_t* lumpInfo, *dst;
    const wadlumprecord_t* src;
    int i, j;

    if(NULL == lumpRecords)
        Con_Error("WadFile::readArchiveLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary lump directory buffer.\n", (unsigned long) lumpRecordsSize);

    // Buffer the archived lump directory.
    F_Seek(handle, lumpRecordOffset, SEEK_SET);
    F_Read(handle, lumpRecords, lumpRecordsSize);

    // Allocate and populate the final lump info list.
    lumpInfo = (lumpinfo_t*)malloc(lumpRecordCount * sizeof(*lumpInfo));
    if(NULL == lumpInfo)
        Con_Error("WadFile::readArchiveLumpDirectory: Failed on allocation of %lu bytes for "
            "lump info list.\n", (unsigned long) (lumpRecordCount * sizeof(*lumpInfo)));
    
    src = lumpRecords;
    dst = lumpInfo;
    for(i = 0; i < lumpRecordCount; ++i, src++, dst++)
    {
        /**
         * The Hexen demo on Mac uses the 0x80 on some lumps, maybe has
         * significance?
         * \todo: Ensure that this doesn't break other IWADs. The 0x80-0xff
         * range isn't normally used in lump names, right??
         */
        for(j = 0; j < 8; ++j)
            dst->name[j] = src->name[j] & 0x7f;
        dst->name[j] = '\0';
        Str_Init(&dst->path);
        dst->baseOffset = (size_t)LONG(src->filePos);
        dst->size = dst->compressedSize = (size_t)LONG(src->size);

        // The modification date is inherited from the real file (note recursion).
        dst->lastModified = file->_base._handle->lastModified;
    }
    // We are finished with the temporary lump records.
    free(lumpRecords);

    if(numLumpsRead)
        *numLumpsRead = lumpRecordCount;
    return lumpInfo;
    }
}

wadfile_t* WadFile_Construct(DFILE* handle, const char* absolutePath,
    lumpdirectory_t* directory)
{
    assert(NULL != handle && NULL != absolutePath && NULL != directory);
    {
    wadfile_t* file;
    wadheader_t hdr;

    if(!WadFile_ReadArchiveHeader(handle, &hdr))
        Con_Error("WadFile::Construct: File %s does not appear to be of WAD format."
            " Missing a call to WadFile::Recognise?", absolutePath);

    file = (wadfile_t*)malloc(sizeof(*file));
    if(NULL == file)
        Con_Error("WadFile::Construct:: Failed on allocation of %lu bytes for new WadFile.",
            (unsigned long) sizeof(*file));

    file->_lumpCount = hdr.lumpRecordsCount;
    file->_lumpRecordsOffset = hdr.lumpRecordsOffset;
    file->_flags = (!strncmp(hdr.identification, "IWAD", 4)? WFF_IWAD : 0); // Found an IWAD.
    file->_lumpInfo = NULL;
    file->_lumpCache = NULL;
    AbstractFile_Init((abstractfile_t*)file, FT_WADFILE, handle, absolutePath, directory);

    /// \todo Defer this operation.
    WadFile_ReadLumpDirectory(file);

    return file;
    }
}

void WadFile_Destruct(wadfile_t* file)
{
    assert(NULL != file);
    WadFile_ClearLumpCache(file);
    if(file->_lumpCount > 1 && NULL != file->_lumpCache)
    {
        free(file->_lumpCache);
    }
    if(file->_lumpInfo)
    {
        int i;
        for(i = 0; i < file->_lumpCount; ++i)
            Str_Free(&file->_lumpInfo[i].path);
        free(file->_lumpInfo);
    }
    Str_Free(&file->_base._absolutePath);
    free(file);
}

void WadFile_ClearLumpCache(wadfile_t* file)
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

uint WadFile_CalculateCRC(const wadfile_t* file)
{
    assert(NULL != file);
    {
    int i, k;
    uint crc = 0;
    for(i = 0; i < file->_lumpCount; ++i)
    {
        lumpinfo_t* info = file->_lumpInfo + i;
        crc += (uint) info->size;
        for(k = 0; k < LUMPNAME_T_LASTINDEX; ++k)
            crc += info->name[k];
    }
    return crc;
    }
}

static __inline uint WadFile_CacheIndexForLump(const wadfile_t* file,
    const lumpinfo_t* info)
{
    assert(NULL != file && NULL != info);
    assert((info - file->_lumpInfo) >= 0 && (info - file->_lumpInfo) < file->_lumpCount);
    return info - file->_lumpInfo;
}

void WadFile_ReadLumpSection2(wadfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    size_t readBytes;

    VERBOSE2(
        Con_Message("WadFile::ReadLump: \"%s:%s\" (%lu bytes%s)\n", F_PrettyPath(Str_Text(&file->_base._absolutePath)),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : "")) );

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != file->_lumpCache)
    {
        boolean isCached;
        void** cachePtr;

        if(file->_lumpCount > 1)
        {
            uint cacheIdx = WadFile_CacheIndexForLump(file, info);
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

    F_Seek(file->_base._handle, info->baseOffset + startOffset, SEEK_SET);
    readBytes = F_Read(file->_base._handle, buffer, length);
    if(readBytes < length)
    {
        Con_Error("WadFile::ReadLump: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpNum);
    }
    }
}

void WadFile_ReadLumpSection(wadfile_t* file, lumpnum_t lumpNum, char* buffer,
    size_t startOffset, size_t length)
{
    WadFile_ReadLumpSection2(file, lumpNum, buffer, startOffset, length, true);
}

void WadFile_ReadLump2(wadfile_t* file, lumpnum_t lumpNum, char* buffer, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    WadFile_ReadLumpSection2(file, lumpNum, buffer, 0, info->size, tryCache);
    }
}

void WadFile_ReadLump(wadfile_t* file, lumpnum_t lumpNum, char* buffer)
{
    WadFile_ReadLump2(file, lumpNum, buffer, true);
}

const char* WadFile_CacheLump(wadfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpDirectory_LumpInfo(file->_base._directory, lumpNum);
    uint cacheIdx = WadFile_CacheIndexForLump(file, info);
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
            Con_Error("WadFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpNum);
        WadFile_ReadLump2(file, lumpNum, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (char*)(*cachePtr);
    }
}

void WadFile_ChangeLumpCacheTag(wadfile_t* file, lumpnum_t lumpNum, int tag)
{
    assert(NULL != file);
    {
    boolean isCached;
    void** cachePtr;

    if(file->_lumpCount > 1)
    {
        uint cacheIdx;
        if(NULL == file->_lumpCache) return; // Obviously not cached.

        cacheIdx = WadFile_CacheIndexForLump(file, LumpDirectory_LumpInfo(file->_base._directory, lumpNum));
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

static void WadFile_ReadLumpDirectory(wadfile_t* file)
{
    assert(NULL != file);
    /// \fixme What if the directory is already loaded?
    if(file->_lumpCount > 0)
    {
        file->_lumpInfo = WadFile_ReadArchiveLumpDirectory(file, file->_lumpRecordsOffset,
            file->_lumpCount, &file->_lumpCount);
        // Insert the lumps into their rightful places in the directory.
        LumpDirectory_Append(file->_base._directory, file->_lumpInfo, file->_lumpCount, (abstractfile_t*)file);
    }
    else
    {
        file->_lumpInfo = NULL;
        file->_lumpCount = 0;
    }
}

void WadFile_Close(wadfile_t* file)
{
    assert(NULL != file);
    if(NULL != file->_base._handle)
    {
        F_Close(file->_base._handle), file->_base._handle = NULL;
    }
    F_ReleaseFileId(Str_Text(&file->_base._absolutePath));
}

int WadFile_LumpCount(wadfile_t* file)
{
    assert(NULL != file);
    return file->_lumpCount;
}

boolean WadFile_IsIWAD(wadfile_t* file)
{
    assert(NULL != file);
    return ((file->_flags & WFF_IWAD) != 0);
}

boolean WadFile_Recognise(DFILE* handle)
{
    boolean knownFormat = false;
    size_t initPos = F_Tell(handle);
    wadheader_t hdr;
    if(WadFile_ReadArchiveHeader(handle, &hdr) &&
       !(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)))
    {
        knownFormat = true;
    }
    // Reposition the file stream in case another handler needs to process this file.
    F_Seek(handle, initPos, SEEK_SET);
    return knownFormat;
}
