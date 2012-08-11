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
#include "pathdirectory.h"
#include "wadfile.h"

typedef struct {
    size_t baseOffset;
    LumpInfo info;
} wadfile_lumprecord_t;

struct wadfile_s {
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
    wadfile_lumprecord_t* lumpRecords;

    /// Lump cache data pointers.
    void** lumpCache;
};

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
static void WadFile_ReadLumpDirectory(WadFile* wad)
{
    wadfile_lumprecord_t* record;
    const wadlumprecord_t* src;
    wadlumprecord_t* lumpDir;
    PathDirectoryNode* node;
    size_t lumpDirSize;
    ddstring_t absPath;
    const char* ext;
    int i, j;
    assert(wad);

    if(wad->lumpRecordsCount <= 0) return;

    lumpDirSize = wad->lumpRecordsCount * sizeof(wadlumprecord_t);
    lumpDir = (wadlumprecord_t*)malloc(lumpDirSize);
    if(!lumpDir)
        Con_Error("WadFile::readLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary lump directory buffer.\n", (unsigned long) lumpDirSize);

    // Buffer the archived lump directory.
    DFile_Seek(wad->base._file, wad->lumpRecordsOffset, SEEK_SET);
    DFile_Read(wad->base._file, (uint8_t*)lumpDir, lumpDirSize);

    wad->lumpRecords = (wadfile_lumprecord_t*)malloc(wad->lumpRecordsCount * sizeof(*wad->lumpRecords));
    if(!wad->lumpRecords)
        Con_Error("WadFile::readLumpDirectory: Failed on allocation of %lu bytes for new lump record vector.", (unsigned long) (wad->lumpRecordsCount * sizeof(*wad->lumpRecords)));

    Str_Init(&absPath);
    // Lumpnames are upto eight characters in length.
    Str_Reserve(&absPath, LUMPNAME_T_LASTINDEX + 4/*.lmp*/);

    src = lumpDir;
    record = wad->lumpRecords;
    for(i = 0; i < wad->lumpRecordsCount; ++i, src++, record++)
    {
        record->baseOffset = (size_t)LONG(src->filePos);

        F_InitLumpInfo(&record->info);
        record->info.lumpIdx = i;

        Str_Clear(&absPath);
        for(j = 0; j < LUMPNAME_T_LASTINDEX; ++j)
        {
            /// The Hexen demo on Mac uses the 0x80 on some lumps, maybe has significance?
            /// @todo Ensure that this doesn't break other IWADs. The 0x80-0xff
            ///       range isn't normally used in lump names, right??
            char ch = src->name[j] & 0x7f;
            Str_AppendChar(&absPath, ch);
        }
        Str_StripRight(&absPath);

        // Lump names allow characters the file system does not. Therefore they
        // will be percent-encoded here and later decoded if/when necessary.
        Str_PercentEncode(&absPath);

        /// We do not consider zero-length names to be valid, so replace with
        /// with _something_.
        /// @todo Handle this more elegantly...
        if(Str_IsEmpty(&absPath))
            Str_Set(&absPath, "________");

        // All lumps are ordained with the .lmp extension if they don't have one.
        ext = F_FindFileExtension(Str_Text(&absPath));
        if(!(ext && Str_Length(&absPath) > ext - Str_Text(&absPath) + 1))
            Str_Append(&absPath, ".lmp");

        // Make it absolute.
        F_PrependBasePath(&absPath, &absPath);

        record->info.size = record->info.compressedSize = (size_t)LONG(src->size);
        record->info.container = (abstractfile_t*)wad;

        // The modification date is inherited from the real file (note recursion).
        record->info.lastModified = AbstractFile_LastModified((abstractfile_t*)wad);

        // Have we yet to intialize the directory?
        if(!wad->lumpDirectory)
        {
            wad->lumpDirectory = PathDirectory_NewWithFlags(PDF_ALLOW_DUPLICATE_LEAF);
        }
        node = PathDirectory_Insert2(wad->lumpDirectory, Str_Text(&absPath), '/');
        PathDirectoryNode_SetUserData(node, record);
    }

    Str_Free(&absPath);

    // We are finished with the temporary lump records.
    free(lumpDir);
}

static int insertNodeInLumpDirectoryMap(PathDirectoryNode* node, void* parameters)
{
    WadFile* wad = (WadFile*)parameters;
    wadfile_lumprecord_t* lumpRecord = (wadfile_lumprecord_t*)PathDirectoryNode_UserData(node);
    assert(lumpRecord && lumpRecord->info.lumpIdx >= 0 && lumpRecord->info.lumpIdx < WadFile_LumpCount(wad));
    wad->lumpDirectoryMap[lumpRecord->info.lumpIdx] = node;
    return 0; // Continue iteration.
}

static void buildLumpDirectoryMap(WadFile* wad)
{
    assert(wad);
    // Time to build the lump directory map?
    if(!wad->lumpDirectoryMap)
    {
        int lumpCount = WadFile_LumpCount(wad);

        wad->lumpDirectoryMap = malloc(sizeof(*wad->lumpDirectoryMap) * lumpCount);
        if(!wad->lumpDirectoryMap)
            Con_Error("WadFile::LumpRecord: Failed on allocation of %lu bytes for the lumpdirectory map.", (unsigned long) (sizeof(*wad->lumpDirectoryMap) * lumpCount));

        PathDirectory_Iterate2(wad->lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH,
                               insertNodeInLumpDirectoryMap, (void*)wad);
    }
}

static wadfile_lumprecord_t* WadFile_LumpRecord(WadFile* wad, int lumpIdx)
{
    assert(wad);
    if(lumpIdx < 0 || lumpIdx >= WadFile_LumpCount(wad)) return NULL;
    buildLumpDirectoryMap(wad);
    return (wadfile_lumprecord_t*)PathDirectoryNode_UserData(wad->lumpDirectoryMap[lumpIdx]);
}

WadFile* WadFile_New(DFile* file, const char* path, const LumpInfo* info)
{
    WadFile* wad;
    wadheader_t hdr;

    if(!info)
        Con_Error("WadFile::New: Received invalid LumpInfo (=NULL).");

    if(!WadFile_ReadArchiveHeader(file, &hdr))
        Con_Error("WadFile::New: File %s does not appear to be of WAD format."
            " Missing a call to WadFile::Recognise?", path);

    wad = (WadFile*)malloc(sizeof *wad);
    if(!wad) Con_Error("WadFile::Construct:: Failed on allocation of %lu bytes for new WadFile.",
                (unsigned long) sizeof *wad);

    AbstractFile_Init((abstractfile_t*)wad, FT_WADFILE, path, file, info);
    wad->lumpRecordsCount = hdr.lumpRecordsCount;
    wad->lumpRecordsOffset = hdr.lumpRecordsOffset;
    wad->lumpRecords = NULL;
    wad->lumpDirectory = NULL;
    wad->lumpDirectoryMap = NULL;
    wad->lumpCache = NULL;

    return wad;
}

int WadFile_PublishLumpsToDirectory(WadFile* wad, LumpDirectory* directory)
{
    int numPublished = 0;
    assert(wad);
    if(directory)
    {
        WadFile_ReadLumpDirectory(wad);
        if(WadFile_LumpCount(wad) > 0)
        {
            // Insert the lumps into their rightful places in the directory.
            const int lumpCount = WadFile_LumpCount(wad);
            LumpDirectory_CatalogLumps(directory, (abstractfile_t*)wad, 0, lumpCount);
            numPublished += lumpCount;
        }
    }
    return numPublished;
}

PathDirectoryNode* WadFile_LumpDirectoryNode(WadFile* wad, int lumpIdx)
{
    if(lumpIdx < 0 || lumpIdx >= WadFile_LumpCount(wad)) return NULL;
    buildLumpDirectoryMap(wad);
    return wad->lumpDirectoryMap[lumpIdx];
}

ddstring_t* WadFile_ComposeLumpPath(WadFile* wad, int lumpIdx, char delimiter)
{
    PathDirectoryNode* node = WadFile_LumpDirectoryNode(wad, lumpIdx);
    if(node)
    {
        return PathDirectoryNode_ComposePath2(node, Str_New(), NULL, delimiter);
    }
    return Str_New();
}

const LumpInfo* WadFile_LumpInfo(WadFile* wad, int lumpIdx)
{
    wadfile_lumprecord_t* lumpRecord = WadFile_LumpRecord(wad, lumpIdx);
    if(!lumpRecord)
    {
        Con_Error("WadFile::LumpInfo: Invalid lump index %i (valid range: [0..%i]).", lumpIdx, WadFile_LumpCount(wad));
        exit(1); // Unreachable.
    }
    return &lumpRecord->info;
}

static int destroyRecord(PathDirectoryNode* node, void* parameters)
{
    wadfile_lumprecord_t* rec = PathDirectoryNode_UserData(node);

    DENG_UNUSED(parameters);

    if(rec)
    {
        // Detach our user data from this node.
        PathDirectoryNode_SetUserData(node, 0);
        F_DestroyLumpInfo(&rec->info);
        // The record itself is free'd later.
    }
    return 0; // Continue iteration.
}

void WadFile_Delete(WadFile* wad)
{
    assert(wad);

    F_ReleaseFile((abstractfile_t*)wad);
    WadFile_ClearLumpCache(wad);

    if(wad->lumpDirectory)
    {
        if(PathDirectory_Size(wad->lumpDirectory) > 1 && wad->lumpCache)
        {
            free(wad->lumpCache);
        }

        PathDirectory_Iterate(wad->lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, destroyRecord);
        PathDirectory_Delete(wad->lumpDirectory);
    }

    if(wad->lumpDirectoryMap) free(wad->lumpDirectoryMap);
    if(wad->lumpRecords) free(wad->lumpRecords);

    AbstractFile_Destroy((abstractfile_t*)wad);
    free(wad);
}

void WadFile_ClearLumpCache(WadFile* wad)
{
    int i, lumpCount = WadFile_LumpCount(wad);
    assert(wad);

    if(lumpCount == 1)
    {
        if(wad->lumpCache)
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(wad->lumpCache) < PU_MAP)
                Z_ChangeTag(wad->lumpCache, PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(wad->lumpCache, (void*) 0x2);
        }
        return;
    }

    if(!wad->lumpCache) return;

    for(i = 0; i < lumpCount; ++i)
    {
        if(!wad->lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(wad->lumpCache[i]) < PU_MAP)
            Z_ChangeTag(wad->lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(wad->lumpCache[i], (void*) 0x2);
    }
}

uint WadFile_CalculateCRC(WadFile* wad)
{
    int i, k, lumpCount = WadFile_LumpCount(wad);
    uint crc = 0;
    assert(wad);

    for(i = 0; i < lumpCount; ++i)
    {
        PathDirectoryNode* node = WadFile_LumpDirectoryNode(wad, i);
        wadfile_lumprecord_t* rec = PathDirectoryNode_UserData(node);
        const ddstring_t* lumpName = PathDirectoryNode_PathFragment(node);

        crc += (uint) rec->info.size;
        for(k = 0; k < LUMPNAME_T_LASTINDEX; ++k)
        {
            if(k < Str_Length(lumpName))
                crc += Str_At(lumpName, k);
        }
    }
    return crc;
}

size_t WadFile_ReadLumpSection2(WadFile* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    const wadfile_lumprecord_t* lumpRecord = WadFile_LumpRecord(wad, lumpIdx);
    size_t readBytes;

    if(!lumpRecord) return 0;

    VERBOSE2(
        ddstring_t* path = WadFile_ComposeLumpPath(wad, lumpIdx, '/');
        Con_Printf("WadFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                F_PrettyPath(Str_Text(path)),
                (unsigned long) lumpRecord->info.size,
                (lumpRecord->info.compressedSize != lumpRecord->info.size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length);
        Str_Delete(path)
    )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && wad->lumpCache)
    {
        boolean isCached;
        void** cachePtr;

        if(WadFile_LumpCount(wad) > 1)
        {
            const uint cacheIdx = lumpIdx;
            cachePtr = &wad->lumpCache[cacheIdx];
            isCached = (NULL != wad->lumpCache[cacheIdx]);
        }
        else
        {
            cachePtr = (void**)&wad->lumpCache;
            isCached = (NULL != wad->lumpCache);
        }

        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(lumpRecord->info.size, length);
            memcpy(buffer, (char*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    DFile_Seek(wad->base._file, lumpRecord->baseOffset + startOffset, SEEK_SET);
    readBytes = DFile_Read(wad->base._file, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("WadFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
}

size_t WadFile_ReadLumpSection(WadFile* wad, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return WadFile_ReadLumpSection2(wad, lumpIdx, buffer, startOffset, length, true);
}

size_t WadFile_ReadLump2(WadFile* wad, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const LumpInfo* info = WadFile_LumpInfo(wad, lumpIdx);
    if(!info) return 0;
    return WadFile_ReadLumpSection2(wad, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t WadFile_ReadLump(WadFile* wad, int lumpIdx, uint8_t* buffer)
{
    return WadFile_ReadLump2(wad, lumpIdx, buffer, true);
}

const uint8_t* WadFile_CacheLump(WadFile* wad, int lumpIdx, int tag)
{
    const LumpInfo* info = WadFile_LumpInfo(wad, lumpIdx);
    const uint cacheIdx = lumpIdx;
    boolean isCached;
    void** cachePtr;

    VERBOSE2(
        ddstring_t* path = WadFile_ComposeLumpPath(wad, lumpIdx, '/');
        Con_Printf("WadFile::CacheLump: \"%s:%s\" (%lu bytes%s)",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                F_PrettyPath(Str_Text(path)), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""));
        Str_Delete(path)
    )

    if(WadFile_LumpCount(wad) > 1)
    {
        // Time to allocate the cache ptr table?
        if(!wad->lumpCache)
            wad->lumpCache = (void**)calloc(WadFile_LumpCount(wad), sizeof(*wad->lumpCache));
        cachePtr = &wad->lumpCache[cacheIdx];
        isCached = (NULL != wad->lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&wad->lumpCache;
        isCached = (NULL != wad->lumpCache);
    }

    VERBOSE2( Con_Printf(" %s\n", isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* buffer = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(!buffer)
            Con_Error("WadFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        WadFile_ReadLump2(wad, lumpIdx, buffer, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
}

void WadFile_ChangeLumpCacheTag(WadFile* wad, int lumpIdx, int tag)
{
    boolean isCached;
    void** cachePtr;
    assert(wad);

    if(WadFile_LumpCount(wad) > 1)
    {
        const uint cacheIdx = lumpIdx;
        if(!wad->lumpCache) return; // Obviously not cached.

        cachePtr = &wad->lumpCache[cacheIdx];
        isCached = (NULL != wad->lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&wad->lumpCache;
        isCached = (NULL != wad->lumpCache);
    }

    if(isCached)
    {
        VERBOSE2(
            ddstring_t* path = WadFile_ComposeLumpPath(wad, lumpIdx, '/');
            Con_Printf("WadFile::ChangeLumpCacheTag: \"%s:%s\" tag=%i\n",
                    F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)wad))),
                    F_PrettyPath(Str_Text(path)), tag);
            Str_Delete(path)
        )

        Z_ChangeTag2(*cachePtr, tag);
    }
}

int WadFile_LumpCount(WadFile* wad)
{
    assert(wad);
    return wad->lumpDirectory? PathDirectory_Size(wad->lumpDirectory) : 0;
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
