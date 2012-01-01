/**\file wadfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

static void WadFile_ReadLumpDirectory(wadfile_t* wad);

static boolean WadFile_ReadArchiveHeader(DFile* file, wadheader_t* hdr)
{
    assert(file && hdr);
    {
    size_t readBytes, initPos = DFile_Tell(file);
    // Seek to the start of the header.
    DFile_Seek(file, 0, SEEK_SET);
    readBytes = DFile_Read(file, (uint8_t*)hdr, sizeof(wadheader_t));
    // Return the stream to its original position.
    DFile_Seek(file, initPos, SEEK_SET);
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
static wadfile_lumprecord_t* WadFile_ReadArchiveLumpDirectory(wadfile_t* wad,
    size_t lumpDirOffset, int lumpDirRecordCount, int* numLumpsRead)
{
    assert(wad);
    {
    size_t lumpDirSize = lumpDirRecordCount * sizeof(wadlumprecord_t);
    wadlumprecord_t* lumpDir = (wadlumprecord_t*)malloc(lumpDirSize);
    wadfile_lumprecord_t* lumpRecords, *dst;
    const wadlumprecord_t* src;
    int i, j;

    if(!lumpDir)
        Con_Error("WadFile::readArchiveLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary lump directory buffer.\n", (unsigned long) lumpDirSize);

    // Buffer the archived lump directory.
    DFile_Seek(wad->_base._file, lumpDirOffset, SEEK_SET);
    DFile_Read(wad->_base._file, (uint8_t*)lumpDir, lumpDirSize);

    // Allocate and populate the final lump info list.
    lumpRecords = (wadfile_lumprecord_t*)malloc(lumpDirRecordCount * sizeof *lumpRecords);
    if(NULL == lumpRecords)
        Con_Error("WadFile::readArchiveLumpDirectory: Failed on allocation of %lu bytes for "
            "lump record list.\n", (unsigned long) (lumpDirRecordCount * sizeof *lumpRecords));

    src = lumpDir;
    dst = lumpRecords;
    for(i = 0; i < lumpDirRecordCount; ++i, src++, dst++)
    {
        /**
         * The Hexen demo on Mac uses the 0x80 on some lumps, maybe has
         * significance?
         * \todo: Ensure that this doesn't break other IWADs. The 0x80-0xff
         * range isn't normally used in lump names, right??
         */
        for(j = 0; j < 8; ++j)
            dst->info.name[j] = src->name[j] & 0x7f;
        dst->info.name[j] = '\0';
        Str_Init(&dst->info.path);
        dst->baseOffset = (size_t)LONG(src->filePos);
        dst->info.size = dst->info.compressedSize = (size_t)LONG(src->size);
        dst->info.lumpIdx = i;
        dst->info.container = (abstractfile_t*)wad;

        // The modification date is inherited from the real file (note recursion).
        dst->info.lastModified = AbstractFile_LastModified((abstractfile_t*)wad);
    }
    // We are finished with the temporary lump records.
    free(lumpDir);

    if(numLumpsRead)
        *numLumpsRead = lumpDirRecordCount;
    return lumpRecords;
    }
}

wadfile_t* WadFile_New(DFile* file, const lumpinfo_t* info)
{
    assert(info);
    {
    wadfile_t* wad;
    wadheader_t hdr;

    if(!WadFile_ReadArchiveHeader(file, &hdr))
        Con_Error("WadFile::Construct: File %s does not appear to be of WAD format."
            " Missing a call to WadFile::Recognise?", Str_Text(&info->path));

    wad = (wadfile_t*)malloc(sizeof *wad);
    if(!wad) Con_Error("WadFile::Construct:: Failed on allocation of %lu bytes for new WadFile.",
                (unsigned long) sizeof *wad);

    AbstractFile_Init((abstractfile_t*)wad, FT_WADFILE, file, info);
    wad->_lumpCount = hdr.lumpRecordsCount;
    wad->_lumpRecordsOffset = hdr.lumpRecordsOffset;
    wad->_lumpRecords = NULL;
    wad->_lumpCache = NULL;

    if(!strncmp(hdr.identification, "IWAD", 4))
        AbstractFile_SetIWAD((abstractfile_t*)wad, true); // Found an IWAD!

    return wad;
    }
}

int WadFile_PublishLumpsToDirectory(wadfile_t* wad, lumpdirectory_t* directory)
{
    assert(wad && directory);
    {
    int numPublished = 0;
    WadFile_ReadLumpDirectory(wad);
    if(wad->_lumpCount > 0)
    {
        // Insert the lumps into their rightful places in the directory.
        LumpDirectory_Append(directory, (abstractfile_t*)wad, 0, wad->_lumpCount);
        numPublished += wad->_lumpCount;
    }
    return numPublished;
    }
}

const lumpinfo_t* WadFile_LumpInfo(wadfile_t* wad, int lumpIdx)
{
    assert(wad);
    if(lumpIdx < 0 || lumpIdx >= wad->_lumpCount)
    {
        Con_Error("WadFile::LumpInfo: Invalid lump index %i (valid range: [0...%i]).", lumpIdx, wad->_lumpCount);
        exit(1); // Unreachable.
    }
    return &wad->_lumpRecords[lumpIdx].info;
}

void WadFile_Delete(wadfile_t* wad)
{
    assert(NULL != wad);
    F_ReleaseFile((abstractfile_t*)wad);
    WadFile_ClearLumpCache(wad);
    if(wad->_lumpCount > 1 && NULL != wad->_lumpCache)
    {
        free(wad->_lumpCache);
    }
    if(wad->_lumpRecords)
    {
        int i;
        for(i = 0; i < wad->_lumpCount; ++i)
            F_DestroyLumpInfo(&wad->_lumpRecords[i].info);
        free(wad->_lumpRecords);
    }
    AbstractFile_Destroy((abstractfile_t*)wad);
    free(wad);
}

void WadFile_ClearLumpCache(wadfile_t* wad)
{
    assert(NULL != wad);

    if(wad->_lumpCount == 1)
    {
        if(wad->_lumpCache)
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(wad->_lumpCache) < PU_MAP)
                Z_ChangeTag(wad->_lumpCache, PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(wad->_lumpCache, (void*) 0x2);
        }
        return;
    }

    if(NULL == wad->_lumpCache) return;

    { int i;
    for(i = 0; i < wad->_lumpCount; ++i)
    {
        if(!wad->_lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(wad->_lumpCache[i]) < PU_MAP)
            Z_ChangeTag(wad->_lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(wad->_lumpCache[i], (void*) 0x2);
    }}
}

uint WadFile_CalculateCRC(const wadfile_t* wad)
{
    assert(wad);
    {
    int i, k;
    uint crc = 0;
    for(i = 0; i < wad->_lumpCount; ++i)
    {
        const lumpinfo_t* info = &wad->_lumpRecords[i].info;
        crc += (uint) info->size;
        for(k = 0; k < LUMPNAME_T_LASTINDEX; ++k)
            crc += info->name[k];
    }
    return crc;
    }
}

size_t WadFile_ReadLumpSection2(wadfile_t* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(wad);
    {
    const lumpinfo_t* info = WadFile_LumpInfo(wad, lumpIdx);
    size_t readBytes;

    if(!info) return 0;

    VERBOSE2(
        Con_Printf("WadFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length) )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != wad->_lumpCache)
    {
        boolean isCached;
        void** cachePtr;

        if(wad->_lumpCount > 1)
        {
            const uint cacheIdx = lumpIdx;
            cachePtr = &wad->_lumpCache[cacheIdx];
            isCached = (NULL != wad->_lumpCache[cacheIdx]);
        }
        else
        {
            cachePtr = (void**)&wad->_lumpCache;
            isCached = (NULL != wad->_lumpCache);
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
    DFile_Seek(wad->_base._file, wad->_lumpRecords[info->lumpIdx].baseOffset + startOffset, SEEK_SET);
    readBytes = DFile_Read(wad->_base._file, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("WadFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
    }
}

size_t WadFile_ReadLumpSection(wadfile_t* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return WadFile_ReadLumpSection2(wad, lumpIdx, buffer, startOffset, length, true);
}

size_t WadFile_ReadLump2(wadfile_t* wad, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const lumpinfo_t* info = WadFile_LumpInfo(wad, lumpIdx);
    if(!info) return 0;
    return WadFile_ReadLumpSection2(wad, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t WadFile_ReadLump(wadfile_t* wad, int lumpIdx, uint8_t* buffer)
{
    return WadFile_ReadLump2(wad, lumpIdx, buffer, true);
}

const uint8_t* WadFile_CacheLump(wadfile_t* wad, int lumpIdx, int tag)
{
    assert(wad);
    {
    const lumpinfo_t* info = WadFile_LumpInfo(wad, lumpIdx);
    const uint cacheIdx = lumpIdx;
    boolean isCached;
    void** cachePtr;

    VERBOSE2(
        Con_Printf("WadFile::CacheLump: \"%s:%s\" (%lu bytes%s)",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : "")) )

    if(wad->_lumpCount > 1)
    {
        // Time to allocate the cache ptr table?
        if(NULL == wad->_lumpCache)
            wad->_lumpCache = (void**)calloc(wad->_lumpCount, sizeof(*wad->_lumpCache));
        cachePtr = &wad->_lumpCache[cacheIdx];
        isCached = (NULL != wad->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&wad->_lumpCache;
        isCached = (NULL != wad->_lumpCache);
    }

    VERBOSE2( Con_Printf(" %s\n", isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("WadFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        WadFile_ReadLump2(wad, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
    }
}

void WadFile_ChangeLumpCacheTag(wadfile_t* wad, int lumpIdx, int tag)
{
    assert(wad);
    {
    boolean isCached;
    void** cachePtr;

    if(wad->_lumpCount > 1)
    {
        const uint cacheIdx = lumpIdx;
        if(NULL == wad->_lumpCache) return; // Obviously not cached.

        cachePtr = &wad->_lumpCache[cacheIdx];
        isCached = (NULL != wad->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&wad->_lumpCache;
        isCached = (NULL != wad->_lumpCache);
    }

    if(isCached)
    {
        VERBOSE2(
            const lumpinfo_t* info = WadFile_LumpInfo(wad, lumpIdx);
            Con_Printf("WadFile::ChangeLumpCacheTag: \"%s:%s\" tag=%i\n",
                    F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                    (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), tag) )

        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

static void WadFile_ReadLumpDirectory(wadfile_t* wad)
{
    assert(wad);
    /// \fixme What if the directory is already loaded?
    if(wad->_lumpCount > 0)
    {
        wad->_lumpRecords = WadFile_ReadArchiveLumpDirectory(wad, wad->_lumpRecordsOffset,
            wad->_lumpCount, &wad->_lumpCount);
    }
    else
    {
        wad->_lumpRecords = NULL;
        wad->_lumpCount = 0;
    }
}

int WadFile_LumpCount(wadfile_t* wad)
{
    assert(wad);
    return wad->_lumpCount;
}

boolean WadFile_Recognise(DFile* file)
{
    boolean knownFormat = false;
    wadheader_t hdr;
    if(WadFile_ReadArchiveHeader(file, &hdr) &&
       !(memcmp(hdr.identification, "IWAD", 4) && memcmp(hdr.identification, "PWAD", 4)))
    {
        knownFormat = true;
    }
    return knownFormat;
}
