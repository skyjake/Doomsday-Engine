/**\file dd_wad.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "sys_file.h"
#include "pathdirectory.h"
#include "sys_reslocator.h"

/**
 * @defgroup fileRecordFlags File record flags.
 */
/*@{*/
#define FRF_IWAD                    0x1 // File is marked IWAD (else its a PWAD).
#define FRF_RUNTIME                 0x2 // Loaded at runtime (for reset).
/*@}*/

typedef struct {
    ddstring_t absolutePath;
    int numLumps;
    int flags; /// @see fileRecordFlags
    DFILE* handle;
} filerecord_t;

typedef struct {
    size_t position; // Offset from start of WAD file.
    size_t size;
    DFILE* handle;
    lumpname_t name; /// Ends in '\0'.
} lumpinfo_t;

#pragma pack(1)
typedef struct {
    char identification[4];
    int32_t numLumps;
    int32_t infoTableOfs;
} wadheader_t;

typedef struct {
    int32_t filePos;
    int32_t size;
    char name[8];
} wadlumprecord_t;
#pragma pack()

D_CMD(DumpLump);
D_CMD(ListWadFiles);

static lumpinfo_t* lumpInfo = 0;
static int numLumps = 0;
static void** lumpCache = 0;
static int numCache = 0;

// The file records.
static int numRecords = 0;
static filerecord_t* records = 0;

static lumpinfo_t* PrimaryLumpInfo;
static int PrimaryNumLumps;
static void** PrimaryLumpCache;

static lumpinfo_t* AuxiliaryLumpInfo;
static int AuxiliaryNumLumps;
static void** AuxiliaryLumpCache;
static DFILE* AuxiliaryHandle;
static boolean AuxiliaryOpened = false;

static boolean loadingForStartup = true;

void W_Register(void)
{
    C_CMD("dump", "s", DumpLump);
    C_CMD("listwadfiles", "", ListWadFiles);
}

static lumpinfo_t* lumpInfoForLump(lumpnum_t lumpNum)
{
    assert(lumpNum >= 0 && lumpNum < numLumps);
    return lumpInfo + lumpNum;
}

static filerecord_t* findFileRecordForLump(lumpnum_t lumpNum)
{
    lumpinfo_t* info = lumpInfoForLump(lumpNum);
    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->handle == info->handle)
            return rec;
    }}
    return 0;
}

static filerecord_t* newFileRecord(void)
{
    filerecord_t* rec;
    records = M_Realloc(records, sizeof(filerecord_t) * (++numRecords));
    rec = records + numRecords - 1;
    memset(rec, 0, sizeof(*rec));
    return rec;
}

static boolean destroyFileRecord(int idx)
{
    if(idx < 0 || idx > numRecords - 1)
        return false; // Huh?

    // Collapse the record array.
    if(idx != numRecords - 1)
    {
        memmove(records + idx, records + idx + 1, sizeof(filerecord_t) * (numRecords - idx - 1));
    }

    // Reallocate the records memory.
    numRecords--; // One less record.
    records = M_Realloc(records, sizeof(filerecord_t) * numRecords);

    return true;
}

static int fileRecordIndexByName(const char* fileName)
{
    int foundRecordIdx = -1;
    if(NULL != fileName && fileName[0] && numRecords > 0)
    {
        ddstring_t buf;
        int idx;

        // Transform the given path into one we can process.
        Str_Init(&buf); Str_Set(&buf, fileName);
        F_FixSlashes(&buf, &buf);

        // Perform the search.
        idx = 0;
        do
        {
            if(!Str_CompareIgnoreCase(&records[idx].absolutePath, Str_Text(&buf)))
            {
                foundRecordIdx = idx;
            }
        } while(foundRecordIdx == -1 && ++idx < numRecords);
        Str_Free(&buf);
    }
    return foundRecordIdx;
}

static lumpnum_t findLumpByName(char* lumpname, int startfrom)
{
    char name8[9];
    int i, v1, v2;
    lumpinfo_t* lump_p;

    if(startfrom < 0 || startfrom > numLumps - 1)
        return -1;

    memset(name8, 0, sizeof(name8));
    strncpy(name8, lumpname, 8);
    v1 = *(int*)  name8;
    v2 = *(int*) (name8 + 4);

    // Start from the beginning.
    for(i = startfrom, lump_p = lumpInfo + startfrom; i < numLumps; i++, lump_p++)
    {
        if(v1 == *(int *) lump_p->name && v2 == *(int *) (lump_p->name + 4))
            return i;
    }
    return -1;
}

static void populateLumpInfo(int liIndex, const wadlumprecord_t* flump,
    const filerecord_t* rec)
{
    lumpinfo_t* lump = &lumpInfo[liIndex];
    lump->handle = rec->handle;
    lump->position = (size_t)LONG(flump->filePos);
    lump->size = (size_t)LONG(flump->size);
    memset(lump->name, 0, sizeof(lump->name));
    strncpy(lump->name, flump->name, 8);
}

/**
 * Moves 'count' lumpinfos, starting from 'from'. Updates the lumpcache.
 * Offset can only be positive.
 *
 * Lumpinfo and lumpcache are assumed to have enough memory for the
 * operation!
 */
static void moveLumps(int from, int count, int offset)
{
    // Check that our information is valid.
    if(!offset || count <= 0 || from < 0 || from > numLumps - 1)
        return;

    // First update the lumpcache.
    memmove(lumpCache + from + offset, lumpCache + from, sizeof(*lumpCache) * count);
    { int i;
    for(i = from + offset; i < from + count + offset; ++i)
    {
        if(!lumpCache[i])
            continue;
        Z_ChangeUser(lumpCache[i], lumpCache + i); // Update the user.
    }}

    // Clear the 'revealed' memory.
    if(offset > 0) // Revealed from the beginning.
        memset(lumpCache + from, 0, offset * sizeof(*lumpCache));
    else  // Revealed from the end.
        memset(lumpCache + from + count + offset, 0, -offset * sizeof(*lumpCache));

    // Lumpinfo.
    memmove(lumpInfo + from + offset, lumpInfo + from, sizeof(lumpinfo_t) * count);
}

/**
 * Moves the rest of the lumps forward.
 */
static void insertAndFillLumpRange(int toIndex, wadlumprecord_t* lumps, int num, filerecord_t* rec)
{
    // Move lumps if needed.
    if(toIndex < numLumps)
        moveLumps(toIndex, numLumps - toIndex, num);

    // Now we can just fill in the lumps.
    { int i;
    for(i = 0; i < num; ++i)
        populateLumpInfo(toIndex + i, lumps + i, rec);
    }

    // Update the number of lumps.
    numLumps += num;
}

static void removeLumpsWithHandle(DFILE* handle)
{
    // Do this one lump at a time...
    { int i;
    for(i = 0; i < numLumps; ++i)
    {
        if(lumpInfo[i].handle != handle)
            continue;

        // Free the memory allocated for the lump.
        if(lumpCache[i])
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(lumpCache[i]) < PU_MAP)
                Z_ChangeTag(lumpCache[i], PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(lumpCache[i], (void*) 0x2);
        }

        // Move the data in the lump storage.
        moveLumps(i+1, 1, -1);
        --numLumps;
        --i;
    }}
}

/**
 * Reallocates lumpInfo and lumpcache.
 */
static void resizeLumpStorage(int numItems)
{
    if(0 != numItems)
    {
        lumpInfo = (lumpinfo_t*) Z_Realloc(lumpInfo, sizeof(*lumpInfo) * numItems, PU_APPSTATIC);
        if(NULL == lumpInfo)
            Con_Error("Wad::resizeLumpStorage: Failed on (re)allocation of %lu bytes for "
                "LumpInfo record list.", (unsigned long) (sizeof(*lumpInfo) * numItems));
    }
    else if(NULL != lumpInfo)
    {
        Z_Free(lumpInfo), lumpInfo = NULL;
    }

    // Updating the cache is a bit more difficult. We need to make sure
    // the user pointers in the memory zone remain valid.
    if(numCache != numItems)
    {
        if(0 != numItems)
        {
            void** newCache = Z_Calloc(sizeof(*newCache) * numItems, PU_APPSTATIC, 0);
            if(NULL == newCache)
                Con_Error("Wad::resizeLumpStorage: Failed on allocation of %lu bytes for "
                    "cached lump index.", (unsigned long) (sizeof(*newCache) * numItems));

            // Need to copy the old cache?
            if(NULL != lumpCache)
            {
                int numToMod;

                if(numCache < numItems)
                    numToMod = numCache;
                else
                    numToMod = numItems;
                memcpy(newCache, lumpCache, sizeof(*lumpCache) * numToMod);

                // Also update the user information in the memory zone.
                { int i;
                for(i = 0; i < numToMod; ++i)
                    if(newCache[i])
                        Z_ChangeUser(newCache[i], newCache + i);
                }
                Z_Free(lumpCache);
            }
            lumpCache = newCache;
        }
        else if(NULL != lumpCache)
        {
            Z_Free(lumpCache), lumpCache = NULL;
        }

        numCache = numItems;
    }
}

/**
 * Inserts the lumps in the fileinfo/record to their correct places in the
 * lumpInfo. Also maintains lumpInfo/records that all data is valid.
 *
 * The current placing of the lumps is that flats and sprites are added to
 * previously existing flat and sprite groups. All other lumps are just
 * appended to the end of the list.
 */
static void insertLumps(wadlumprecord_t* fileinfo, filerecord_t* rec)
{
    wadlumprecord_t* flump = fileinfo;
    int maxNumLumps = numLumps + rec->numLumps; // This must be enough.

    // Allocate more memory for the lumpInfo.
    resizeLumpStorage(maxNumLumps);

    { int i;
    for(i = 0; i < rec->numLumps; ++i, flump++)
    {
        /**
         * The Hexen demo on Mac uses the 0x80 on some lumps, maybe has
         * significance?
         * \todo: Ensure that this doesn't break other IWADs. The 0x80-0xff
         * range isn't normally used in lump names, right??
         */
        { int to;
        for(to = 0; to < 8; ++to)
            flump->name[to] = flump->name[to] & 0x7f;
        }

        populateLumpInfo(numLumps, flump, rec);
        ++numLumps;
    }}

    // It may be that all lumps weren't added. Make sure we don't have
    // excess memory allocated (=synchronize the storage size with the
    // real numLumps).
    resizeLumpStorage(numLumps);

    // Update the record with the number of lumps that were loaded.
    rec->numLumps -= maxNumLumps - numLumps;
}

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
static uint calcCRCNumberForRecord(const filerecord_t* record)
{
    uint crc = 0;
    lumpnum_t i;
    int k;

    for(i = 0; i < numLumps; ++i)
    {
        lumpinfo_t* l = &lumpInfo[i];

        if(l->handle != record->handle)
            continue;

        crc += (uint) l->size;
        for(k = 0; k < 8; ++k)
            crc += l->name[k];
    }

    return crc;
}

static boolean addFile(const char* fileName, boolean allowDuplicate)
{
    wadlumprecord_t singleInfo, *fileInfo, *freeFileInfo;
    resourcetype_t resourceType;
    unsigned int length;
    filerecord_t* rec;
    wadheader_t header;
    DFILE* handle;

    // Filename given?
    if(!fileName || !fileName[0])
        return true;

    if(!(handle = F_Open(fileName, "rb")))
    {
        Con_Message("Warning:W_AddFile: Resource \"%s\" not found, aborting.\n", fileName);
        return false;
    }

    // Do not read files twice.
    if(!allowDuplicate && !F_CheckFileId(fileName))
    {
        Con_Message("\"%s\" already loaded.\n", F_PrettyPath(fileName));
        F_Close(handle); // The file is not used.
        return false;
    }

    VERBOSE( Con_Message("Loading \"%s\"...\n", F_PrettyPath(fileName)) )

    resourceType = F_GuessResourceTypeByName(fileName);
    // Is it a zip/pk3 package?
    if(resourceType == RT_ZIP)
    {
        return Zip_Open2(fileName, handle);
    }

    // Get a new file record.
    rec = newFileRecord();
    rec->handle = handle;
    if(!loadingForStartup)
        rec->flags = FRF_RUNTIME;

    Str_Init(&rec->absolutePath);
    Str_Set(&rec->absolutePath, fileName);
    Str_Strip(&rec->absolutePath);
    F_FixSlashes(&rec->absolutePath, &rec->absolutePath);

    if(resourceType != RT_WAD) // lmp?
    {
        int offset = 0;
        char* slash = 0;

        // Single lump file.
        fileInfo = &singleInfo;
        freeFileInfo = 0;
        singleInfo.filePos = 0;
        singleInfo.size = (int32_t)LONG(F_Length(handle));

        // Is there a prefix to be omitted in the name?
        slash = strrchr(fileName, DIR_SEP_CHAR);
        // The slash mustn't be too early in the string.
        if(slash && slash >= fileName + 2)
        {
            // Good old negative indices.
            if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
            {
                offset = slash[-1] - '1' + 1;
            }
        }

        F_ExtractFileBase2(singleInfo.name, fileName, 8, offset);
        rec->numLumps = 1;

        if(resourceType == RT_DEH)
            strncpy(fileInfo->name, "DEHACKED", 8);
    }
    else
    {
        // WAD file.
        F_Read(handle, &header, sizeof(header));
        if(strncmp(header.identification, "IWAD", 4))
        {
            if(strncmp(header.identification, "PWAD", 4))
            {   // Bad file id
                Con_Error("W_AddFile: Wad file %s does not have IWAD or PWAD id.\n", fileName);
            }
        }
        else
        {   // Found an IWAD.
            rec->flags |= FRF_IWAD;
        }

        header.numLumps = LONG(header.numLumps);
        header.infoTableOfs = LONG(header.infoTableOfs);
        length = header.numLumps * sizeof(wadlumprecord_t);
        if(!(fileInfo = M_Malloc(length)))
        {
            Con_Error("W_AddFile: Fileinfo allocation failed.\n");
        }

        freeFileInfo = fileInfo;
        F_Seek(handle, header.infoTableOfs, SEEK_SET);
        F_Read(handle, fileInfo, length);
        rec->numLumps = header.numLumps;
    }

    // Insert the lumps to lumpInfo, into their rightful places.
    insertLumps(fileInfo, rec);

    if(freeFileInfo)
        M_Free(freeFileInfo);

    PrimaryLumpInfo = lumpInfo;
    PrimaryLumpCache = lumpCache;
    PrimaryNumLumps = numLumps;

    // Print the 'CRC' number of the IWAD, so it can be identified.
    if(rec->flags & FRF_IWAD)
        Con_Message("  IWAD identification: %08x\n", calcCRCNumberForRecord(rec));

    return true;
}

static boolean removeFile(const char* fileName)
{
    filerecord_t* rec;
    int idx;

    VERBOSE( Con_Message("Unloading \"%s\"...\n", F_PrettyPath(fileName)) )

    // Is it a zip/pk3 package?
    if(F_GuessResourceTypeByName(fileName) == RT_ZIP)
    {
        return Zip_Close(fileName);
    }

    idx = fileRecordIndexByName(fileName);
    if(idx == -1)
        return false; // No such file loaded.
    rec = records + idx;

    // We must remove all the data of this file from the lump storage
    // (lumpInfo + lumpcache).
    removeLumpsWithHandle(rec->handle);

    // Resize the lump storage to match numLumps.
    resizeLumpStorage(numLumps);

    // Close the file, we don't need it any more.
    F_Close(rec->handle);
    F_ReleaseFileId(Str_Text(&rec->absolutePath));
    Str_Free(&rec->absolutePath);

    // Destroy the file record.
    destroyFileRecord(idx);

    PrimaryLumpInfo = lumpInfo;
    PrimaryLumpCache = lumpCache;
    PrimaryNumLumps = numLumps;

    // Success!
    return true;
}

/**
 * Handles the conversion to a logical index that is independent of the
 * lump cache currently in use.
 */
static __inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
{
    return (lumpCache == AuxiliaryLumpCache? lumpNum += AUXILIARY_BASE : lumpNum);
}

/// \warning: closeAuxiliary() must be called before any further auxiliary lump processing.
static void closeAuxiliaryFile(void)
{
    if(AuxiliaryHandle)
    {
        F_Close(AuxiliaryHandle), AuxiliaryHandle = NULL;
    }
}

static void usePrimaryCache(void)
{
    lumpInfo = PrimaryLumpInfo;
    numLumps = PrimaryNumLumps;
    lumpCache = PrimaryLumpCache;
}

static boolean useAuxiliaryCache(void)
{
    if(AuxiliaryOpened == false)
    {
        // The auxiliary cache is not available at this time.
        return false;
    }
    lumpInfo = AuxiliaryLumpInfo;
    numLumps = AuxiliaryNumLumps;
    lumpCache = AuxiliaryLumpCache;
    return true;
}

/**
 * Selects which lump cache to use, given a logical lump index. This is
 * called in all the functions that access the lump cache.
 */
static lumpnum_t chooseCache(lumpnum_t lumpNum)
{
    if(lumpNum >= AUXILIARY_BASE)
    {
        useAuxiliaryCache();
        lumpNum -= AUXILIARY_BASE;
    }
    else
    {
        usePrimaryCache();
    }
    return lumpNum;
}

/**
 * Scan backwards so patch lump files take precedence.
 */
static lumpnum_t scanLumpInfo(int v[2])
{
    lumpinfo_t* lump_p = lumpInfo + numLumps;

    while(lump_p-- != lumpInfo)
    {
        if(*(int*)  lump_p->name    == v[0] &&
           *(int*) &lump_p->name[4] == v[1])
        {
            return logicalLumpNum(lump_p - lumpInfo);
        }
    }
    return -1;
}

void W_Init(void)
{
    // This'll force the loader NOT the flag new records Runtime. (?)
    loadingForStartup = true;

    numLumps = 0;
    lumpInfo = Z_Malloc(1, PU_APPSTATIC, 0); // Will be realloced as lumps are added.
}

void W_EndStartup(void)
{
    loadingForStartup = false;
}

int W_Reset(void)
{
    int i, unloadedResources = 0;

    Z_FreeTags(PU_CACHE, PU_CACHE);

    for(i = 0; i < numRecords; ++i)
    {
        if(!(records[i].flags & FRF_RUNTIME))
            continue;
        if(removeFile(Str_Text(&records[i].absolutePath)))
        {
            i = -1;
            ++unloadedResources;
        }
    }
    return unloadedResources;
}

int W_LumpCount(void)
{
    return numLumps;
}

boolean W_AddFile(const char* fileName, boolean allowDuplicate)
{
    return addFile(fileName, allowDuplicate);
}

boolean W_RemoveFile(const char* fileName)
{
    boolean unloadedResources = removeFile(fileName);
    if(unloadedResources)
        DD_UpdateEngineState();
    return unloadedResources;
}

boolean W_AddFiles(const char* const* filenames, size_t num, boolean allowDuplicate)
{
    boolean succeeded = false;
    { size_t i;
    for(i = 0; i < num; ++i)
    {
        if(addFile(filenames[i], allowDuplicate))
        {
            VERBOSE2( Con_Message("Done loading %s\n", F_PrettyPath(filenames[i])) );
            succeeded = true; // At least one has been loaded.
        }
        else
            Con_Message("Warning: Errors occured while loading %s\n", filenames[i]);
    }}

    // A changed file list may alter the main lump directory.
    if(succeeded)
    {
        DD_UpdateEngineState();
    }
    return succeeded;
}

boolean W_RemoveFiles(const char* const* filenames, size_t num)
{
    boolean succeeded = false;
    { size_t i;
    for(i = 0; i < num; ++i)
    {
        if(removeFile(filenames[i]))
        {
            VERBOSE2( Con_Message("Done unloading %s\n", F_PrettyPath(filenames[i])) );
            succeeded = true; // At least one has been unloaded.
        }
        else
            Con_Message("Warning: Errors occured while unloading %s\n", filenames[i]);
    }}

    // A changed file list may alter the main lump directory.
    if(succeeded)
    {
        DD_UpdateEngineState();
    }
    return succeeded;
}

lumpnum_t W_OpenAuxiliary2(const char* fileName, DFILE* prevOpened)
{
    wadlumprecord_t* fileInfo, *sourceLump;
    lumpinfo_t* destLump;
    int i, size, length;
    wadheader_t header;
    DFILE* handle;

    if(prevOpened == NULL)
    {   // Try to open the file.
        if((handle = F_Open(fileName, "rb")) == NULL)
        {
            Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", fileName);
            return -1;
        }
    }
    else
    {   // Use the previously opened file.
        handle = prevOpened;
    }

    F_Read(handle, &header, sizeof(header));
    if(strncmp(header.identification, "IWAD", 4))
    {
        if(strncmp(header.identification, "PWAD", 4))
        {
            Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" does not appear to be a WAD archive.\n", fileName);
            return -1;
        }
    }

    if(AuxiliaryOpened)
    {
        W_CloseAuxiliary();
    }
    AuxiliaryHandle = handle;

    header.numLumps = LONG(header.numLumps);
    header.infoTableOfs = LONG(header.infoTableOfs);
    length = header.numLumps * sizeof(wadlumprecord_t);

    fileInfo = M_Malloc(length);
    if(NULL == fileInfo)
        Con_Error("W_OpenAuxiliary: Failed on allocation of %lu bytes for temporary "
            "lump directory records.", (unsigned long) length);

    F_Seek(handle, header.infoTableOfs, SEEK_SET);
    F_Read(handle, fileInfo, length);
    numLumps = header.numLumps;

    // Init the auxiliary lumpInfo array
    lumpInfo = Z_Malloc(numLumps * sizeof(lumpinfo_t), PU_APPSTATIC, 0);
    if(NULL == lumpInfo)
        Con_Error("W_OpenAuxiliary: Failed on allocation of %lu bytes for new lump info.",
            (unsigned long) (numLumps * sizeof(lumpinfo_t)));

    sourceLump = fileInfo;
    destLump = lumpInfo;
    for(i = 0; i < numLumps; ++i, destLump++, sourceLump++)
    {
        destLump->handle = handle;
        destLump->position = (size_t)LONG(sourceLump->filePos);
        destLump->size = (size_t)LONG(sourceLump->size);
        strncpy(destLump->name, sourceLump->name, 8);
    }
    M_Free(fileInfo);

    // Allocate the auxiliary lumpcache array
    size = numLumps * sizeof(*lumpCache);
    lumpCache = Z_Calloc(size, PU_APPSTATIC, 0);
    if(NULL == lumpCache)
        Con_Error("W_OpenAuxiliary: Failed on allocation of %lu bytes for lump cache.",
            (unsigned long) size);

    AuxiliaryLumpInfo = lumpInfo;
    AuxiliaryLumpCache = lumpCache;
    AuxiliaryNumLumps = numLumps;
    AuxiliaryOpened = true;

    return AUXILIARY_BASE;
}

lumpnum_t W_OpenAuxiliary(const char* fileName)
{
    return W_OpenAuxiliary2(fileName, NULL);
}

void W_CloseAuxiliary(void)
{
    if(AuxiliaryOpened)
    {
        lumpnum_t i;

        useAuxiliaryCache();

        for(i = 0; i < numLumps; ++i)
        {
            if(lumpCache[i])
            {
                Z_Free(lumpCache[i]);
            }
        }
        Z_Free(AuxiliaryLumpInfo), AuxiliaryLumpInfo = NULL;
        Z_Free(AuxiliaryLumpCache), AuxiliaryLumpCache = NULL;
        AuxiliaryNumLumps = 0;

        closeAuxiliaryFile();
        AuxiliaryOpened = false;
    }

    usePrimaryCache();
}

lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t idx = -1;
    char name8[9];
    int v[2];

    // If the name string is empty, don't bother to search.
    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n") );
        return -1;
    }

    memset(name8, 0, sizeof(name8));

    // Make the name into two integers for easy compares
    strncpy(name8, name, 8);
    strupr(name8); // case insensitive
    v[0] = *(int *) name8;
    v[1] = *(int *) &name8[4];

    // We have to check both the primary and auxiliary caches because
    // we've only got a name and don't know where it is located. Start with
    // the auxiliary lumps because they take precedence.
    if(useAuxiliaryCache())
    {
        idx = scanLumpInfo(v);
        if(idx != -1)
            return idx;
    }

    usePrimaryCache();
    idx = scanLumpInfo(v);
    if(idx != -1)
        return idx;

    if(!silent)
        VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Lump \"%s\" not found.\n", name8) );
    return -1;
}

lumpnum_t W_CheckLumpNumForName(const char* name)
{
    return W_CheckLumpNumForName2(name, false);
}

lumpnum_t W_GetLumpNumForName(const char* name)
{
    lumpnum_t           i;

    i = W_CheckLumpNumForName(name);
    if(i != -1)
    {
        return i;
    }

    Con_Error("W_GetLumpNumForName: Lump \"%s\" not found.", name);
    return -1;
}

size_t W_LumpLength(lumpnum_t lumpNum)
{
    lumpNum = chooseCache(lumpNum);

    if(lumpNum >= numLumps)
    {
        Con_Error("W_LumpLength: Lumpnum #%i >= numLumps.", lumpNum);
    }

    return lumpInfo[lumpNum].size;
}

const char* W_LumpName(lumpnum_t lumpNum)
{
    lumpNum = chooseCache(lumpNum);

    if(lumpNum >= numLumps || lumpNum < 0)
    {
        return NULL; // The caller must be able to handle this...
    }

    return lumpInfo[lumpNum].name;
}

void W_ReadLump(lumpnum_t lumpNum, char* dest)
{
    lumpinfo_t* l;
    size_t c;

    lumpNum = chooseCache(lumpNum);
    if(0 > lumpNum || lumpNum >= numLumps)
    {
        Con_Error("W_ReadLump: Lump #%i >= numLumps.", lumpNum);
    }

    l = &lumpInfo[lumpNum];
    F_Seek(l->handle, l->position, SEEK_SET);
    c = F_Read(l->handle, (void*)dest, l->size);
    if(c < l->size)
    {
        Con_Error("W_ReadLump: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) c, (unsigned long) l->size, lumpNum);
    }
}

void W_ReadLumpSection(lumpnum_t lumpNum, void* dest, size_t startOffset, size_t length)
{
    lumpinfo_t* l;
    size_t c;

    lumpNum = chooseCache(lumpNum);
    if(0 > lumpNum || lumpNum >= numLumps)
    {
        Con_Error("W_ReadLumpSection: Lump #%i >= numLumps.", lumpNum);
    }

    l = &lumpInfo[lumpNum];
    F_Seek(l->handle, l->position + startOffset, SEEK_SET);
    c = F_Read(l->handle, dest, length);
    if(c < length)
    {
        Con_Error("W_ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) c, (unsigned long) length, lumpNum);
    }
}

boolean W_DumpLump(lumpnum_t lumpNum, const char* fileName)
{
    FILE*               file;
    const byte*         lumpPtr;
    const char*         fname;
    char                buf[13]; // 8 (max lump chars) + 4 (ext) + 1.

    lumpNum = chooseCache(lumpNum);
    if(lumpNum >= numLumps)
        return false;

    lumpPtr = (byte*)W_CacheLump(lumpNum, PU_APPSTATIC);

    if(fileName && fileName[0])
    {
        fname = fileName;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        dd_snprintf(buf, 13, "%s.dum", lumpInfo[lumpNum].name);
        fname = buf;
    }

    if(!(file = fopen(fname, "wb")))
    {
        Con_Printf("Warning: Failed to open %s for writing (%s), aborting.\n", fname, strerror(errno));
        W_CacheChangeTag(lumpNum, PU_CACHE);
        return false;
    }

    fwrite(lumpPtr, 1, lumpInfo[lumpNum].size, file);
    fclose(file);
    W_CacheChangeTag(lumpNum, PU_CACHE);

    Con_Printf("%s dumped to %s.\n", lumpInfo[lumpNum].name, fname);
    return true;
}

const char* W_CacheLump(lumpnum_t absoluteLump, int tag)
{
    lumpnum_t lumpNum = chooseCache(absoluteLump);
    char* ptr;

    if(0 > lumpNum || lumpNum >= numLumps)
    {
        Con_Message("Warning:W_CacheLump: Lump index #%i >= numLumps, ignoring.", lumpNum);
        return NULL;
    }

    if(!lumpCache[lumpNum])
    {    // Need to read the lump in
        size_t lumpSize = W_LumpLength(absoluteLump);
        ptr = (char*)Z_Malloc(lumpSize, tag, &lumpCache[lumpNum]);
        if(NULL == ptr)
            Con_Error("W_CacheLump: Failed on allocation of %lu bytes for cache copy of lump #%i.",
                (unsigned long) lumpSize, lumpNum);
        W_ReadLump(lumpNum, ptr);
    }
    else
    {
        Z_ChangeTag(lumpCache[lumpNum], tag);
    }

    return (char*)lumpCache[lumpNum];
}

void W_CacheChangeTag(lumpnum_t lumpNum, int tag)
{
    lumpNum = chooseCache(lumpNum);
    if(lumpNum < 0 || lumpNum >= numLumps)
        Con_Error("W_CacheChangeTag: Invalid lump index %i.", lumpNum);
    if(!lumpCache[lumpNum])
        return;
    Z_ChangeTag2(lumpCache[lumpNum], tag);
}

const char* W_LumpSourceFile(lumpnum_t lumpNum)
{
    lumpNum = chooseCache(lumpNum);
    if(lumpNum < 0 || lumpNum >= numLumps)
        Con_Error("W_LumpSourceFile: Invalid lump index %i.", lumpNum);

    { filerecord_t* rec = findFileRecordForLump(lumpNum);
    if(NULL != rec)
    {
        return Str_Text(&rec->absolutePath);
    }}
    return "";
}

boolean W_LumpIsFromIWAD(lumpnum_t lumpNum)
{
    lumpNum = chooseCache(lumpNum);
    if(lumpNum < 0 || lumpNum >= numLumps)
    {
        return false;
    }

    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->handle == lumpInfo[lumpNum].handle)
            return ((rec->flags & FRF_IWAD) != 0);
    }
    }
    return false;
}

uint W_CRCNumber(void)
{
    int i;
    // Find the IWAD's record.
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->flags & FRF_IWAD)
            return calcCRCNumberForRecord(rec);
    }
    return 0;
}

void W_GetIWADFileName(char* outBuf, size_t outBufSize)
{
    filerecord_t* rec = NULL;
    ddstring_t buf;

    if(NULL == outBuf || 0 == outBufSize) return;

    if(numRecords > 0)
    {
        int idx = 0;
        do
        {
            if(records[idx].flags & FRF_IWAD)
                rec = &records[idx];
        } while(NULL == rec && ++idx < numRecords);
    }

    if(NULL == rec) return;

    Str_Init(&buf);
    F_FileNameAndExtension(&buf, Str_Text(&rec->absolutePath));
    strncpy(outBuf, Str_Text(&buf), outBufSize);
    Str_Free(&buf);
}

void W_GetPWADFileNames(char* outBuf, size_t outBufSize, char delimiter)
{
    ddstring_t buf;
    if(NULL == outBuf || 0 == outBufSize) return;

    Str_Init(&buf);
    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->flags & FRF_IWAD) continue;

        Str_Clear(&buf);
        F_FileNameAndExtension(&buf, Str_Text(&rec->absolutePath));
        if(stricmp(Str_Text(&buf) + Str_Length(&buf) - 3, "lmp"))
            M_LimitedStrCat(outBuf, Str_Text(&buf), 64, delimiter, outBufSize);
    }}
    Str_Free(&buf);
}

void W_PrintLumpDirectory(void)
{
    char buff[10];
    int p;

    printf("Lumps (%d total):\n", numLumps);
    for(p = 0; p < numLumps; p++)
    {
        strncpy(buff, W_LumpName(p), 8);
        buff[8] = 0;
        printf("%04i - %-8s (hndl: %p, pos: %lu, size: %lu)\n", p, buff,
               lumpInfo[p].handle, (unsigned long) lumpInfo[p].position,
               (unsigned long) lumpInfo[p].size);
    }
    Con_Error("---End of lumps---\n");
}

D_CMD(DumpLump)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName(argv[1]);
    if(-1 == lumpNum)
    {
        Con_Printf("No such lump.\n");
        return false;
    }
    if(!W_DumpLump(lumpNum, NULL))
        return false;
    return true;
}

D_CMD(ListWadFiles)
{
    int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        Con_Printf("\"%s\" (%d %s%s)", F_PrettyPath(Str_Text(&rec->absolutePath)),
                   rec->numLumps, rec->numLumps != 1 ? "lumps" : "lump",
                   !(rec->flags & FRF_RUNTIME) ? ", startup" : "");
        if(rec->flags & FRF_IWAD)
            Con_Printf(" [%08x]", calcCRCNumberForRecord(rec));
        Con_Printf("\n");
    }
    Con_Printf("Total: %d lumps in %d files.\n", numLumps, numRecords);
    return true;
}
