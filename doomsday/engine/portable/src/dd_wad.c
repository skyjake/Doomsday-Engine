/**\file dd_wad.c
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

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h" // For M_LimitedStrCat

#include "lumpdirectory.h"

#pragma pack(1)
typedef struct {
    char identification[4];
    int32_t lumpRecordCount;
    int32_t lumpRecordOffset;
} wadheader_t;

typedef struct {
    int32_t filePos;
    int32_t size;
    char name[8];
} wadlumprecord_t;
#pragma pack()

D_CMD(DumpLump);
D_CMD(ListLumps);

static boolean inited = false;
static boolean loadingForStartup;

// Head of the llist of wadfiles.
static wadfile_t* wadFileList;
static wadfile_t* auxiliaryWadFile; /// \note Also linked in wadFileList.

static lumpdirectory_t* primaryLumpDirectory;
static lumpdirectory_t* auxiliaryLumpDirectory;

// Currently selected lump directory.
static lumpdirectory_t* lumpDirectory;

void W_Register(void)
{
    C_CMD("dump", "s", DumpLump);
    C_CMD("listwadfiles", "", ListLumps);
}

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: WAD module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static wadfile_t* linkWadFile(wadfile_t* wad)
{
    assert(NULL != wad);
    wad->next = wadFileList;
    return wadFileList = wad;
}

static wadfile_t* unlinkWadFile(wadfile_t* wad)
{
    assert(NULL != wad);
    if(wadFileList == NULL) return wad;
    if(wad == wadFileList)
    {
        wadFileList = wadFileList->next;
    }
    else
    {
        wadfile_t* prev = wadFileList;
        while(prev->next && prev->next != wad) { prev = prev->next; }

        prev->next = prev->next->next;
    }
    return wad;
}

static wadfile_t* findWadFileForName(const char* path)
{
    wadfile_t* foundRecord = NULL;
    if(NULL != path && path[0] && NULL != wadFileList)
    {
        wadfile_t* cand;
        ddstring_t buf;

        // Transform the given path into one we can process.
        Str_Init(&buf); Str_Set(&buf, path);
        F_FixSlashes(&buf, &buf);

        // Perform the search.
        cand = wadFileList;
        do
        {
            if(!Str_CompareIgnoreCase(WadFile_AbsolutePath(cand), Str_Text(&buf)))
            {
                foundRecord = cand;
            }
        } while(foundRecord == NULL && NULL != (cand = cand->next));
        Str_Free(&buf);
    }
    return foundRecord;
}

static wadfile_t* newWadFile(DFILE* handle, int flags, const char* absolutePath,
    lumpdirectory_t* directory)
{
    assert(NULL != handle && NULL != absolutePath && NULL != directory);
    {
    wadfile_t* wad = (wadfile_t*)calloc(1, sizeof(*wad));
    if(NULL == wad)
        Con_Error("WadCollection::newWadFile: Failed on allocation of %lu bytes for "
            "new WadFile.", (unsigned long) sizeof(*wad));

    wad->_handle = handle;
    wad->_flags = flags;
    wad->_directory = directory;
    Str_Init(&wad->_absolutePath);
    Str_Set(&wad->_absolutePath, absolutePath);
    Str_Strip(&wad->_absolutePath);
    F_FixSlashes(&wad->_absolutePath, &wad->_absolutePath);

    return linkWadFile(wad);
    }
}

static void WadFile_ClearLumpCache(wadfile_t* wad)
{
    assert(NULL != wad);

    if(wad->_numLumps == 1)
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

    { size_t i;
    for(i = 0; i < wad->_numLumps; ++i)
    {
        if(!wad->_lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(wad->_lumpCache[i]) < PU_MAP)
            Z_ChangeTag(wad->_lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(wad->_lumpCache[i], (void*) 0x2);
    }}
}

static void WadFile_Destruct(wadfile_t* wad)
{
    assert(NULL != wad);
    unlinkWadFile(wad);
    WadFile_ClearLumpCache(wad);
    if(wad->_numLumps > 1 && NULL != wad->_lumpCache)
    {
        free(wad->_lumpCache);
    }
    if(wad->_lumpInfo)
        free(wad->_lumpInfo);
    Str_Free(&wad->_absolutePath);
    free(wad);
}

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
static uint WadFile_CalculateCRC(const wadfile_t* wad)
{
    assert(NULL != wad);
    {
    uint crc = 0;
    size_t i;
    int k;
    for(i = 0; i < wad->_numLumps; ++i)
    {
        wadfile_lumpinfo_t* info = wad->_lumpInfo + i;
        crc += (uint) info->size;
        for(k = 0; k < LUMPNAME_T_LASTINDEX; ++k)
            crc += info->name[k];
    }
    return crc;
    }
}

// We'll load the lump directory using one continous read into a temporary
// local buffer before we process it into our runtime representation.
static wadfile_lumpinfo_t* WadFile_ReadArchivedLumpDirectory(wadfile_t* wad,
    size_t lumpRecordOffset, size_t lumpRecordCount, size_t* numLumpsRead)
{
    assert(NULL != wad);
    {
    DFILE* handle = wad->_handle;
    size_t lumpRecordsSize = lumpRecordCount * sizeof(wadlumprecord_t);
    wadlumprecord_t* lumpRecords = (wadlumprecord_t*)malloc(lumpRecordsSize);
    wadfile_lumpinfo_t* lumpInfo, *dst;
    const wadlumprecord_t* src;
    size_t i;
    int j;

    if(NULL == lumpRecords)
        Con_Error("WadFile::readArchivedLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary lump directory buffer.\n", (unsigned long) lumpRecordsSize);

    // Buffer the archived lump directory.
    F_Seek(handle, lumpRecordOffset, SEEK_SET);
    F_Read(handle, lumpRecords, lumpRecordsSize);

    // Allocate and populate the final lump info list.
    lumpInfo = (wadfile_lumpinfo_t*)malloc(lumpRecordCount * sizeof(*lumpInfo));
    if(NULL == lumpInfo)
        Con_Error("WadFile::readArchivedLumpDirectory: Failed on allocation of %lu bytes for "
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
        memset(dst->name, 0, sizeof(dst->name));
        for(j = 0; j < 8; ++j)
            dst->name[j] = src->name[j] & 0x7f;
        dst->position = (size_t)LONG(src->filePos);
        dst->size = (size_t)LONG(src->size);
    }
    // We are finished with the temporary lump records.
    free(lumpRecords);

    if(numLumpsRead)
        *numLumpsRead = lumpRecordCount;
    return lumpInfo;
    }
}

static __inline size_t WadFile_CacheIndexForLump(const wadfile_t* wad,
    const wadfile_lumpinfo_t* info)
{
    assert(NULL != wad && NULL != info);
    assert((info - wad->_lumpInfo) >= 0 && (unsigned)(info - wad->_lumpInfo) < wad->_numLumps);
    return info - wad->_lumpInfo;
}

static void WadFile_ReadLumpSection(wadfile_t* wad, lumpnum_t lumpNum, void* dest,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != wad);
    {
    const wadfile_lumpinfo_t* info = LumpDirectory_LumpInfo(wad->_directory, lumpNum);
    size_t readBytes;

    assert(NULL != info);

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != wad->_lumpCache)
    {
        boolean isCached;
        void** cachePtr;

        if(wad->_numLumps > 1)
        {
            size_t cacheIdx = WadFile_CacheIndexForLump(wad, info);
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
            memcpy(dest, (char*)*cachePtr + startOffset, MIN_OF(info->size, length));
            return;
        }
    }

    F_Seek(wad->_handle, info->position + startOffset, SEEK_SET);
    readBytes = F_Read(wad->_handle, dest, length);
    if(readBytes < length)
    {
        Con_Error("WadFile::readLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpNum);
    }
    }
}

static void WadFile_ReadLump(wadfile_t* wad, lumpnum_t lumpNum, char* dest, boolean tryCache)
{
    const wadfile_lumpinfo_t* info = LumpDirectory_LumpInfo(wad->_directory, lumpNum);
    assert(NULL != info);
    WadFile_ReadLumpSection(wad, lumpNum, dest, 0, info->size, tryCache);
}

static const char* WadFile_CacheLump(wadfile_t* wad, lumpnum_t lumpNum, int tag)
{
    assert(NULL != wad);
    {
    const wadfile_lumpinfo_t* info = LumpDirectory_LumpInfo(wad->_directory, lumpNum);
    size_t cacheIdx = WadFile_CacheIndexForLump(wad, info);
    boolean isCached;
    void** cachePtr;

    assert(NULL != info);

    if(wad->_numLumps > 1)
    {
        // Time to allocate the cache ptr table?
        if(NULL == wad->_lumpCache)
            wad->_lumpCache = (void**)calloc(wad->_numLumps, sizeof(*wad->_lumpCache));
        cachePtr = &wad->_lumpCache[cacheIdx];
        isCached = (NULL != wad->_lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&wad->_lumpCache;
        isCached = (NULL != wad->_lumpCache);
    }

    if(!isCached)
    {
        char* ptr = (char*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("WadFile::cacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpNum);
        WadFile_ReadLump(wad, lumpNum, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (char*)(*cachePtr);
    }
}

static void WadFile_ChangeLumpCacheTag(wadfile_t* wad, lumpnum_t lumpNum, int tag)
{
    assert(NULL != wad);
    {
    boolean isCached;
    void** cachePtr;

    if(wad->_numLumps > 1)
    {
        size_t cacheIdx;
        if(NULL == wad->_lumpCache) return; // Obviously not cached.

        cacheIdx = WadFile_CacheIndexForLump(wad, LumpDirectory_LumpInfo(wad->_directory, lumpNum));
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
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

static void WadFile_ReadLumpDirectory(wadfile_t* wad, size_t lumpRecordOffset,
    size_t lumpRecordCount)
{
    assert(NULL != wad);
    /// \fixme What if the directory is already loaded?
    if(lumpRecordCount > 0)
    {
        wad->_lumpInfo = WadFile_ReadArchivedLumpDirectory(wad, lumpRecordOffset,
            lumpRecordCount, &wad->_numLumps);
        // Insert the lumps into their rightful places in the directory.
        LumpDirectory_Append(wad->_directory, wad->_lumpInfo, wad->_numLumps, wad);
    }
    else
    {
        wad->_lumpInfo = NULL;
        wad->_numLumps = 0;
    }
}

static void WadFile_InitLumpDirectoryForSingleFile(wadfile_t* wad, lumpname_t name,
    size_t size, size_t lumpRecordOffset)
{
    assert(NULL != wad);
    {
    wadfile_lumpinfo_t* info = (wadfile_lumpinfo_t*)malloc(sizeof(*info));

    if(NULL == info)
        Con_Error("WadFile::initLumpDirectoryForSingleFile: Failed on allocation of %lu bytes for "
            "lump info.", (unsigned long) sizeof(*info));

    memcpy(info->name, name, sizeof(info->name));
    info->size = size;
    info->position = lumpRecordOffset;

    wad->_lumpInfo = info;
    wad->_numLumps = 1;

    // Insert the lump into it's rightful place in the directory.
    LumpDirectory_Append(wad->_directory, wad->_lumpInfo, wad->_numLumps, wad);
    }
}

wadfile_t* W_AddArchive(const char* path, DFILE* handle)
{
    wadfile_t* wad;
    wadheader_t header;
    int flags = 0;

    errorIfNotInited("W_AddArchive");

    F_Read(handle, &header, sizeof(header));
    header.lumpRecordCount = LONG(header.lumpRecordCount);
    header.lumpRecordOffset = LONG(header.lumpRecordOffset);

    if(strncmp(header.identification, "IWAD", 4))
    {
        if(strncmp(header.identification, "PWAD", 4))
        {   // Bad file id
            Con_Error("F_AddFile: Wad file %s does not have IWAD or PWAD id.\n", path);
        }
    }
    else
    {   // Found an IWAD.
        flags |= WFF_IWAD;
    }

    if(!loadingForStartup)
        flags |= WFF_RUNTIME;

    // Get a new file record.
    wad = newWadFile(handle, flags, path, lumpDirectory);
    WadFile_ReadLumpDirectory(wad, header.lumpRecordOffset, header.lumpRecordCount);

    // Print the 'CRC' number of the IWAD, so it can be identified.
    /// \todo Do not do this here.
    if(WadFile_Flags(wad) & WFF_IWAD)
        Con_Message("  IWAD identification: %08x\n", WadFile_CalculateCRC(wad));

    return wad;
}

wadfile_t* W_AddFile(const char* path, DFILE* handle, boolean isDehackedPatch)
{
    wadfile_t* wad;
    lumpname_t name;

    errorIfNotInited("W_AddArchive");

    // Prepare the name of this single-file lump.
    if(isDehackedPatch)
    {
        strncpy(name, "DEHACKED", LUMPNAME_T_MAXLEN);
    }
    else
    {
        // Is there a prefix to be omitted in the name?
        char* slash = strrchr(path, DIR_SEP_CHAR);
        int offset = 0;
        // The slash must not be too early in the string.
        if(slash && slash >= path + 2)
        {
            // Good old negative indices.
            if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
            {
                offset = slash[-1] - '1' + 1;
            }
        }
        F_ExtractFileBase2(name, path, LUMPNAME_T_MAXLEN, offset);
    }

    wad = newWadFile(handle, (!loadingForStartup ? WFF_RUNTIME : 0), path, lumpDirectory);
    WadFile_InitLumpDirectoryForSingleFile(wad, name, F_Length(handle), 0);

    return wad;
}

static void WadFile_CloseFile(wadfile_t* wad)
{
    assert(NULL != wad);
    F_Close(wad->_handle), wad->_handle = NULL;
    F_ReleaseFileId(Str_Text(&wad->_absolutePath));
}

static boolean removeWadFile(wadfile_t* wad)
{
    assert(NULL != wad);
    WadFile_ClearLumpCache(wad);
    LumpDirectory_PruneByFile(WadFile_Directory(wad), wad);
    // Close the file; we don't need it any more.
    WadFile_CloseFile(wad);
    WadFile_Destruct(wad);
    return true;
}

/**
 * Handles conversion to a logical index that is independent of the lump directory currently in use.
 */
static __inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
{
    return (lumpDirectory == auxiliaryLumpDirectory? lumpNum += AUXILIARY_BASE : lumpNum);
}

static void usePrimaryDirectory(void)
{
    lumpDirectory = primaryLumpDirectory;
}

static boolean useAuxiliaryDirectory(void)
{
    if(NULL == auxiliaryWadFile)
    {
        // The auxiliary directory is not available at this time.
        return false;
    }
    lumpDirectory = auxiliaryLumpDirectory;
    return true;
}

/**
 * Selects which lump directory to use, given a logical lump index.
 * This should be called in all functions that access directories by lump index.
 */
static lumpnum_t chooseDirectory(lumpnum_t lumpNum)
{
    if(lumpNum >= AUXILIARY_BASE)
    {
        useAuxiliaryDirectory();
        lumpNum -= AUXILIARY_BASE;
    }
    else
    {
        usePrimaryDirectory();
    }
    return lumpNum;
}

static void clearWadFileList(void)
{
    while(wadFileList) { removeWadFile(wadFileList); }
}

void W_Init(void)
{
    if(inited) return; // Already been here.

    // This'll force the loader NOT to flag new files as "runtime".
    loadingForStartup = true;

    wadFileList = NULL;
    auxiliaryWadFile = NULL;

    primaryLumpDirectory   = LumpDirectory_Construct();
    auxiliaryLumpDirectory = LumpDirectory_Construct();
    lumpDirectory = primaryLumpDirectory;

    inited = true;
}

void W_Shutdown(void)
{
    if(!inited) return;

    W_CloseAuxiliary();
    LumpDirectory_Destruct(primaryLumpDirectory);
    LumpDirectory_Destruct(auxiliaryLumpDirectory);
    clearWadFileList();
    inited = false;
}

void W_EndStartup(void)
{
    errorIfNotInited("W_EndStartup");
    loadingForStartup = false;
    usePrimaryDirectory();
}

int W_Reset(void)
{
    int unloadedResources = 0;
    if(inited)
    {
        wadfile_t* wad = wadFileList, *next;
        while(wad)
        {
            next = wad->next;
            if(WadFile_Flags(wad) & WFF_RUNTIME)
            {
                if(W_RemoveFile(Str_Text(WadFile_AbsolutePath(wad))))
                {
                    ++unloadedResources;
                }
            }
            wad = next;
        }
    }
    return unloadedResources;
}

int W_LumpCount(void)
{
    if(inited)
        return LumpDirectory_NumLumps(lumpDirectory);
    return 0;
}

boolean W_RemoveFile(const char* path)
{
    wadfile_t* wad;
    errorIfNotInited("W_RemoveFile");
    wad = findWadFileForName(path);
    if(NULL != wad)
        return removeWadFile(wad);
    return false; // No such file loaded.
}

lumpnum_t W_OpenAuxiliary3(const char* path, DFILE* prevOpened, boolean silent)
{
    boolean isIWAD = false;
    wadheader_t header;
    wadfile_t* wad;
    DFILE* handle;
    int flags = 0;

    errorIfNotInited("W_OpenAuxiliary3");

    if(prevOpened == NULL)
    {   // Try to open the file.
        if((handle = F_Open(path, "rb")) == NULL)
        {
            if(!silent)
            {
                Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
            }
            return -1;
        }
    }
    else
    {   // Use the previously opened file.
        handle = prevOpened;
    }

    if(F_Read(handle, &header, sizeof(header)) != sizeof(header));
    {
        if(!silent)
        {
            Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" does not appear to be a WAD archive.\n", path);
        }
        return -1;
    }

    header.lumpRecordCount = LONG(header.lumpRecordCount);
    header.lumpRecordOffset = LONG(header.lumpRecordOffset);

    if(strncmp(header.identification, "IWAD", 4))
    {
        if(strncmp(header.identification, "PWAD", 4))
        {
            if(!silent)
            {
                Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" does not appear to be a WAD archive.\n", path);
            }
            return -1;
        }
    }
    else
    {   // Found an IWAD.
        flags |= WFF_IWAD;
    }

    if(NULL != auxiliaryWadFile)
    {
        W_CloseAuxiliary();
    }
    lumpDirectory = auxiliaryLumpDirectory;

    if(!loadingForStartup)
        flags |= WFF_RUNTIME;

    // Get a new file record.
    wad = newWadFile(handle, flags, path, lumpDirectory);
    WadFile_ReadLumpDirectory(wad, header.lumpRecordOffset, header.lumpRecordCount);

    auxiliaryWadFile = wad;

    return AUXILIARY_BASE;
}

lumpnum_t W_OpenAuxiliary2(const char* path, DFILE* prevOpened)
{
    return W_OpenAuxiliary3(path, prevOpened, false);
}

lumpnum_t W_OpenAuxiliary(const char* path)
{
    return W_OpenAuxiliary2(path, NULL);
}

void W_CloseAuxiliary(void)
{
    errorIfNotInited("W_CloseAuxiliary");

    if(useAuxiliaryDirectory())
    {
        wadfile_t* wad = auxiliaryWadFile;

        WadFile_ClearLumpCache(wad);
        LumpDirectory_PruneByFile(WadFile_Directory(wad), wad);
        WadFile_CloseFile(wad);

        auxiliaryWadFile = NULL;
    }

    usePrimaryDirectory();
}

lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t idx;

    errorIfNotInited("W_CheckLumpNumForName2");

    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n") )
        return -1;
    }

    // We have to check both the primary and auxiliary caches because
    // we've only got a name and don't know where it is located. Start with
    // the auxiliary lumps because they take precedence.
    if(useAuxiliaryDirectory())
    {
        idx = LumpDirectory_IndexForName(lumpDirectory, name);
        if(idx != -1)
            return idx;
    }

    usePrimaryDirectory();
    idx = LumpDirectory_IndexForName(lumpDirectory, name);

    if(idx == -1 && !silent)
        VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Lump \"%s\" not found.\n", name) )

    return idx;
}

lumpnum_t W_CheckLumpNumForName(const char* name)
{
    return W_CheckLumpNumForName2(name, false);
}

lumpnum_t W_GetLumpNumForName(const char* name)
{
    lumpnum_t idx = W_CheckLumpNumForName(name);
    if(idx != -1)
        return idx;
    Con_Error("W_GetLumpNumForName: Lump \"%s\" not found.", name);
    exit(1); // Unreachable.
}

size_t W_LumpLength(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpLength");
    lumpNum = chooseDirectory(lumpNum);
    return LumpDirectory_LumpSize(lumpDirectory, lumpNum);
}

const char* W_LumpName(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpName");
    lumpNum = chooseDirectory(lumpNum);
    return LumpDirectory_LumpName(lumpDirectory, lumpNum);
}

void W_ReadLump(lumpnum_t lumpNum, char* dest)
{
    errorIfNotInited("W_ReadLump");
    lumpNum = chooseDirectory(lumpNum);
    { wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    WadFile_ReadLump(wad, lumpNum, dest, true); }
}

void W_ReadLumpSection(lumpnum_t lumpNum, void* dest, size_t startOffset, size_t length)
{
    errorIfNotInited("W_ReadLumpSection");
    lumpNum = chooseDirectory(lumpNum);
    { wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    WadFile_ReadLumpSection(wad, lumpNum, dest, startOffset, length, true); }
}

boolean W_DumpLump(lumpnum_t lumpNum, const char* path)
{
    char buf[LUMPNAME_T_LASTINDEX + 4/*.ext*/ + 1];
    const byte* lumpPtr;
    const char* fname;
    wadfile_t* wad;
    FILE* file;

    errorIfNotInited("W_DumpLump");
    lumpNum = chooseDirectory(lumpNum);
    if(LumpDirectory_IsValidIndex(lumpDirectory, lumpNum))
        return false;

    wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    lumpPtr = (byte*)WadFile_CacheLump(wad, lumpNum, PU_APPSTATIC);

    if(path && path[0])
    {
        fname = path;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        dd_snprintf(buf, 13, "%s.lmp", LumpDirectory_LumpName(lumpDirectory, lumpNum));
        fname = buf;
    }

    if(!(file = fopen(fname, "wb")))
    {
        Con_Printf("Warning: Failed to open %s for writing (%s), aborting.\n", fname, strerror(errno));
        WadFile_ChangeLumpCacheTag(wad, lumpNum, PU_CACHE);
        return false;
    }

    fwrite(lumpPtr, 1, LumpDirectory_LumpSize(lumpDirectory, lumpNum), file);
    fclose(file);
    WadFile_ChangeLumpCacheTag(wad, lumpNum, PU_CACHE);

    Con_Printf("%s dumped to %s.\n", LumpDirectory_LumpName(lumpDirectory, lumpNum), fname);
    return true;
}

const char* W_CacheLump(lumpnum_t lumpNum, int tag)
{
    errorIfNotInited("W_CacheLump");
    lumpNum = chooseDirectory(lumpNum);
    { wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    return WadFile_CacheLump(wad, lumpNum, tag); }
}

void W_CacheChangeTag(lumpnum_t lumpNum, int tag)
{
    errorIfNotInited("W_CacheChangeTag");
    lumpNum = chooseDirectory(lumpNum);
    { wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    WadFile_ChangeLumpCacheTag(wad, lumpNum, tag); }
}

const char* W_LumpSourceFile(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpSourceFile");
    lumpNum = chooseDirectory(lumpNum);
    { wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
    if(NULL != wad)
    {
        return Str_Text(WadFile_AbsolutePath(wad));
    }}
    return "";
}

boolean W_LumpIsFromIWAD(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpIsFromIWAD");
    lumpNum = chooseDirectory(lumpNum);
    if(LumpDirectory_IsValidIndex(lumpDirectory, lumpNum))
    {
        wadfile_t* wad = LumpDirectory_LumpSourceFile(lumpDirectory, lumpNum);
        return ((WadFile_Flags(wad) & WFF_IWAD) != 0);
    }
    return false;
}

uint W_CRCNumber(void)
{
    errorIfNotInited("W_CRCNumber");
    // Find the IWAD's record.
    { wadfile_t* wad;
    for(wad = wadFileList; NULL != wad; wad = wad->next)
    {
        if(WadFile_Flags(wad) & WFF_IWAD)
            return WadFile_CalculateCRC(wad);
    }}
    return 0;
}

void W_GetPWADFileNames(char* outBuf, size_t outBufSize, char delimiter)
{
    ddstring_t buf;

    if(NULL == outBuf || 0 == outBufSize) return;

    memset(outBuf, 0, outBufSize);

    if(!inited) return;

    /// \fixme Do not use the global wadFileList, pull records from the primaryLumpDirectory
    Str_Init(&buf);
    { wadfile_t* wad;
    for(wad = wadFileList; NULL != wad; wad = wad->next)
    {
        if(WadFile_Flags(wad) & WFF_IWAD) continue;

        Str_Clear(&buf);
        F_FileNameAndExtension(&buf, Str_Text(WadFile_AbsolutePath(wad)));
        if(stricmp(Str_Text(&buf) + Str_Length(&buf) - 3, "lmp"))
            M_LimitedStrCat(outBuf, Str_Text(&buf), 64, delimiter, outBufSize);
    }}
    Str_Free(&buf);
}

void W_PrintLumpDirectory(void)
{
    if(!inited) return;
    // Always the primary directory.
    LumpDirectory_Print(primaryLumpDirectory);
}

lumpdirectory_t* WadFile_Directory(wadfile_t* wad)
{
    assert(NULL != wad);
    return wad->_directory;
}

size_t WadFile_NumLumps(wadfile_t* wad)
{
    assert(NULL != wad);
    return wad->_numLumps;
}

int WadFile_Flags(wadfile_t* wad)
{
    assert(NULL != wad);
    return wad->_flags;
}

DFILE* WadFile_Handle(wadfile_t* wad)
{
    assert(NULL != wad);
    return wad->_handle;
}

const ddstring_t* WadFile_AbsolutePath(wadfile_t* wad)
{
    assert(NULL != wad);
    return &wad->_absolutePath;
}

D_CMD(DumpLump)
{
    if(inited)
    {
        lumpnum_t lumpNum = W_CheckLumpNumForName(argv[1]);
        if(-1 != lumpNum)
        {
            return W_DumpLump(lumpNum, NULL);
        }
        Con_Printf("No such lump.\n");
        return false;
    }
    Con_Printf("WAD module is not presently initialized.\n");
    return false;
}

D_CMD(ListLumps)
{
    size_t totalLumps = 0, totalFiles = 0;
    if(inited)
    {
        wadfile_t* wad;
        for(wad = wadFileList; NULL != wad; wad = wad->next)
        {
            Con_Printf("\"%s\" (%lu %s%s)", F_PrettyPath(Str_Text(WadFile_AbsolutePath(wad))),
                (unsigned long) WadFile_NumLumps(wad), WadFile_NumLumps(wad) != 1 ? "lumps" : "lump",
                !(WadFile_Flags(wad) & WFF_RUNTIME) ? ", startup" : "");
            if(WadFile_Flags(wad) & WFF_IWAD)
                Con_Printf(" [%08x]", WadFile_CalculateCRC(wad));
            Con_Printf("\n");

            totalLumps += WadFile_NumLumps(wad);
            ++totalFiles;
        }
    }
    Con_Printf("Total: %lu lumps in %lu files.\n", (unsigned long) totalLumps, (unsigned long)totalFiles);
    return true;
}
