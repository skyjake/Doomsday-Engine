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
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "wadfile.h"

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
    {
    size_t readBytes, initPos = F_Tell(handle);
    // Seek to the start of the header.
    F_Seek(handle, 0, SEEK_SET);
    readBytes = F_Read(handle, (uint8_t*)hdr, sizeof(wadheader_t));
    // Return the stream to its original position.
    F_Seek(handle, initPos, SEEK_SET);
    if(!(readBytes < sizeof(wadheader_t)))
    {
        hdr->lumpRecordsCount  = LONG(hdr->lumpRecordsCount);
        hdr->lumpRecordsOffset = LONG(hdr->lumpRecordsOffset);
        return true;
    }
    return false;
    }
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
    lumpinfo_t* lumpInfo, *dst;
    const wadlumprecord_t* src;
    int i, j;

    if(NULL == lumpRecords)
        Con_Error("WadFile::readArchiveLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary lump directory buffer.\n", (unsigned long) lumpRecordsSize);

    // Buffer the archived lump directory.
    F_Seek(&file->_base._dfile, lumpRecordOffset, SEEK_SET);
    F_Read(&file->_base._dfile, (uint8_t*)lumpRecords, lumpRecordsSize);

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
        dst->lastModified = AbstractFile_LastModified((abstractfile_t*)file);
    }
    // We are finished with the temporary lump records.
    free(lumpRecords);

    if(numLumpsRead)
        *numLumpsRead = lumpRecordCount;
    return lumpInfo;
    }
}

wadfile_t* WadFile_New(DFILE* hndl, const char* absolutePath)
{
    assert(NULL != hndl && NULL != absolutePath);
    {
    wadfile_t* file;
    wadheader_t hdr;

    if(!WadFile_ReadArchiveHeader(hndl, &hdr))
        Con_Error("WadFile::Construct: File %s does not appear to be of WAD format."
            " Missing a call to WadFile::Recognise?", absolutePath);

    file = (wadfile_t*)malloc(sizeof(*file));
    if(NULL == file)
        Con_Error("WadFile::Construct:: Failed on allocation of %lu bytes for new WadFile.",
            (unsigned long) sizeof(*file));

    AbstractFile_Init((abstractfile_t*)file, FT_WADFILE, absolutePath);
    file->_lumpCount = hdr.lumpRecordsCount;
    file->_lumpRecordsOffset = hdr.lumpRecordsOffset;
    file->_lumpInfo = NULL;
    file->_lumpCache = NULL;

    if(!strncmp(hdr.identification, "IWAD", 4))
        AbstractFile_SetIWAD((abstractfile_t*)file, true); // Found an IWAD!

    // Copy the handle.
    memcpy(&file->_base._dfile, hndl, sizeof(file->_base._dfile));
    return file;
    }
}

int WadFile_PublishLumpsToDirectory(wadfile_t* file, lumpdirectory_t* directory)
{
    assert(NULL != file && NULL != directory);
    {
    int numPublished = 0;
    WadFile_ReadLumpDirectory(file);
    if(file->_lumpCount > 0)
    {
        // Insert the lumps into their rightful places in the directory.
        LumpDirectory_Append(directory, (abstractfile_t*)file, 0, file->_lumpCount);
        numPublished += file->_lumpCount;
    }
    return numPublished;
    }
}

const lumpinfo_t* WadFile_LumpInfo(wadfile_t* file, int lumpIdx)
{
    assert(NULL != file);
    if(lumpIdx < 0 || lumpIdx >= file->_lumpCount)
    {
        Con_Error("WadFile::LumpInfo: Invalid lump index %i (valid range: [0...%i)).", lumpIdx, file->_lumpCount);
        exit(1); // Unreachable.
    }
    return file->_lumpInfo + lumpIdx;
}

void WadFile_Delete(wadfile_t* file)
{
    assert(NULL != file);
    WadFile_Close(file);
    F_ReleaseFile((abstractfile_t*)file);
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
    Str_Free(&file->_base._path);
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

size_t WadFile_ReadLumpSection2(wadfile_t* file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = WadFile_LumpInfo(file, lumpIdx);
    size_t readBytes;

    VERBOSE2(
        Con_Printf("WadFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(&file->_base._path)),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length) )

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
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(info->size, length);
            memcpy(buffer, (char*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    F_Seek(&file->_base._dfile, info->baseOffset + startOffset, SEEK_SET);
    readBytes = F_Read(&file->_base._dfile, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("WadFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
    }
}

size_t WadFile_ReadLumpSection(wadfile_t* file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return WadFile_ReadLumpSection2(file, lumpIdx, buffer, startOffset, length, true);
}

size_t WadFile_ReadLump2(wadfile_t* file, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = WadFile_LumpInfo(file, lumpIdx);
    return WadFile_ReadLumpSection2(file, lumpIdx, buffer, 0, info->size, tryCache);
    }
}

size_t WadFile_ReadLump(wadfile_t* file, int lumpIdx, uint8_t* buffer)
{
    return WadFile_ReadLump2(file, lumpIdx, buffer, true);
}

const uint8_t* WadFile_CacheLump(wadfile_t* file, int lumpIdx, int tag)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = WadFile_LumpInfo(file, lumpIdx);
    uint cacheIdx = WadFile_CacheIndexForLump(file, info);
    boolean isCached;
    void** cachePtr;

    VERBOSE2(
        Con_Printf("WadFile::CacheLump: \"%s:%s\" (%lu bytes%s)",
                F_PrettyPath(Str_Text(&file->_base._path)),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : "")) )

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

    VERBOSE2( Con_Printf(" %s\n", isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("WadFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        WadFile_ReadLump2(file, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
    }
}

void WadFile_ChangeLumpCacheTag(wadfile_t* file, int lumpIdx, int tag)
{
    assert(NULL != file);
    {
    boolean isCached;
    void** cachePtr;

    if(file->_lumpCount > 1)
    {
        uint cacheIdx;
        if(NULL == file->_lumpCache) return; // Obviously not cached.

        cacheIdx = WadFile_CacheIndexForLump(file, WadFile_LumpInfo(file, lumpIdx));
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
    F_CloseFile(&file->_base._dfile);
}

int WadFile_LumpCount(wadfile_t* file)
{
    assert(NULL != file);
    return file->_lumpCount;
}

boolean WadFile_Recognise(DFILE* handle)
{
    boolean knownFormat = false;
    wadheader_t hdr;
    if(WadFile_ReadArchiveHeader(handle, &hdr) &&
       !(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)))
    {
        knownFormat = true;
    }
    return knownFormat;
}
