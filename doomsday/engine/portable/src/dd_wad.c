/**\file
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

/**
 * WAD Files and Data Lump Cache
 *
 * This version supports runtime (un)loading.
 *
 * Internally, the cache has two parts: the Primary cache, which is loaded
 * from data files, and the Auxiliary cache, which is generated at runtime.
 * To outsiders, there is no difference between these two caches. The
 * only visible difference is that lumps in the auxiliary cache use indices
 * starting from AUXILIARY_BASE. Everything in the auxiliary cache takes
 * precedence over lumps in the primary cache.
 *
 * The W_Select() function is responsible for activating the right cache
 * when a lump index is provided. Functions that don't know the lump index
 * will have to check both the primary and the auxiliary caches (e.g.,
 * W_CheckNumForName()).
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "sys_file.h"
#include "pathdirectory.h"
#include "sys_reslocator.h"

// MACROS ------------------------------------------------------------------

#define RECORD_FILENAMELEN  FILENAME_T_MAXLEN

/**
 * @defgroup fileRecordFlags File record flags.
 */
/*@{*/
#define FRF_RUNTIME         0x1 // Loaded at runtime (for reset).
/*@}*/

#define AUXILIARY_BASE      100000000

// TYPES -------------------------------------------------------------------

typedef struct {
    lumpname_t      name; // End in '\0'.
    DFILE*          handle;
    int             position;
    size_t          size;
} lumpinfo_t;

typedef struct {
    char            identification[4];
    int             numLumps;
    int             infoTableOfs;
} wadinfo_t;

typedef struct {
    int             filePos;
    int             size;
    char            name[8];
} filelump_t;

typedef struct {
    filename_t      fileName; // Full filename.
    int             numLumps;
    int             flags; /// @see fileRecordFlags
    DFILE*          handle;
    char            iwad;
} filerecord_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Dir);
D_CMD(Dump);
D_CMD(ListWadFiles);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int markerForLumpGroup(char* name, boolean begin);
static void W_CloseAuxiliary(void);
static void W_CloseAuxiliaryFile(void);
static void W_UsePrimary(void);
static boolean W_UseAuxiliary(void); // may not be available
static uint W_CRCNumberForRecord(int idx);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static lumpinfo_t* lumpInfo = 0;
static int numLumps = 0;
static void** lumpCache = 0;
static int numCache = 0;

// The file records.
static int numRecords = 0;
static filerecord_t* records = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumpinfo_t* PrimaryLumpInfo;
static int PrimaryNumLumps;
static void** PrimaryLumpCache;

static lumpinfo_t* AuxiliaryLumpInfo;
static int AuxiliaryNumLumps;
static void** AuxiliaryLumpCache;

static boolean loadingForStartup = true;
boolean AuxiliaryOpened = false;
static DFILE* AuxiliaryHandle;

// CODE --------------------------------------------------------------------

void DD_RegisterVFS(void)
{
    C_CMD("dir", "", Dir);
    C_CMD("dir", "s*", Dir);
    C_CMD("ls", "", Dir);
    C_CMD("ls", "s*", Dir);
    C_CMD("dump", "s", Dump);
    C_CMD("listwadfiles", "", ListWadFiles);
}

static lumpnum_t W_Index(lumpnum_t lump)
{
    if(lumpCache == AuxiliaryLumpCache)
    {
        lump += AUXILIARY_BASE;
    }
    return lump;
}

/**
 * Selects which lump cache to use, given a logical lump index. This is
 * called in all the functions that access the lump cache.
 */
static lumpnum_t W_Select(lumpnum_t lump)
{
    if(lump >= AUXILIARY_BASE)
    {
        W_UseAuxiliary();
        lump -= AUXILIARY_BASE;
    }
    else
    {
        W_UsePrimary();
    }
    return lump;
}

int W_NumLumps(void)
{
    return numLumps;
}

/*
 * File Record Handling
 * (all functions named W_Record*)
 */

static lumpinfo_t* lumpInfoForLump(lumpnum_t lump)
{
    assert(lump >= 0 && lump < numLumps);
    return lumpInfo + lump;
}

static filerecord_t* findFileRecordForLump(lumpnum_t lump)
{
    lumpinfo_t* info = lumpInfoForLump(lump);
    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->handle == info->handle)
            return rec;
    }}
    return 0;
}

/**
 * Allocate a new file record.
 */
filerecord_t* W_RecordNew(void)
{
    filerecord_t* rec;
    records = M_Realloc(records, sizeof(filerecord_t) * (++numRecords));
    rec = records + numRecords - 1;
    memset(rec, 0, sizeof(*rec));
    return rec;
}

int W_RecordGetIdx(const char* fileName)
{
    filename_t buf;
    // We have to make sure the slashes are correct.
    strncpy(buf, fileName, FILENAME_T_MAXLEN);
    Dir_FixSlashes(buf, FILENAME_T_MAXLEN);

    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        if(!strnicmp(records[i].fileName, buf, FILENAME_T_MAXLEN))
            return i;
    }}
    return -1;
}

/**
 * Destroy the specified record. Returns true if successful.
 */
boolean W_RecordDestroy(int idx)
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

/**
 * Look for the named lump, starting from the specified index.
 */
lumpnum_t W_ScanForName(char* lumpname, int startfrom)
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

/*
 * Lumpinfo inserting, filling, removal and other operations.
 */

/**
 * Writes the correct data into a lumpinfo_t entry.
 */
static void populateLumpInfo(int liIndex, const filelump_t* flump,
    const filerecord_t* rec)
{
    lumpinfo_t* lump = &lumpInfo[liIndex];
    lump->handle = rec->handle;
    lump->position = LONG(flump->filePos);
    lump->size = LONG(flump->size);
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
static void insertAndFillLumpRange(int toIndex, filelump_t* lumps, int num, filerecord_t* rec)
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
                "LumpInfo record list.", (unsigned int) (sizeof(*lumpInfo) * numItems));
    }
    else if(NULL != lumpInfo)
    {
        Z_Free(lumpInfo); lumpInfo = NULL;
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
                    "cached lump index.", (unsigned int) (sizeof(*newCache) * numItems));

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
            Z_Free(lumpCache); lumpCache = NULL;
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
static void insertLumps(filelump_t* fileinfo, filerecord_t* rec)
{
    filelump_t* flump = fileinfo;
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

static boolean addFile(const char* fileName, boolean allowDuplicate)
{
    wadinfo_t           header;
    DFILE*              handle;
    unsigned int        length;
    filelump_t          singleInfo, *fileInfo, *freeFileInfo;
    filerecord_t*       rec;
    const char*         extension;

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
        Con_Message("%s (0) already loaded.\n", M_PrettyPath(fileName));
        F_Close(handle); // The file is not used.
        return false;
    }

    VERBOSE( Con_Message("Loading %s ...\n", M_PrettyPath(fileName)) );

    // Determine the file name extension.
    extension = strrchr(fileName, '.');
    if(!extension)
        extension = "";
    else
        extension++; // Move to point after the dot.

    // Is it a zip/pk3 package?
    if(!stricmp(extension, "zip") || !stricmp(extension, "pk3"))
    {
        return Zip_Open(fileName, handle);
    }

    // Get a new file record.
    rec = W_RecordNew();
    strncpy(rec->fileName, fileName, FILENAME_T_MAXLEN);
    Dir_FixSlashes(rec->fileName, FILENAME_T_MAXLEN);
    rec->handle = handle;

    // If we're not loading for startup, flag the record to be a Runtime one.
    if(!loadingForStartup)
        rec->flags = FRF_RUNTIME;

    if(stricmp(extension, "wad")) // lmp?
    {
        int offset = 0;
        char* slash = 0;

        // Single lump file.
        fileInfo = &singleInfo;
        freeFileInfo = 0;
        singleInfo.filePos = 0;
        singleInfo.size = LONG(F_Length(handle));

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

        M_ExtractFileBase2(singleInfo.name, fileName, 8, offset);
        rec->numLumps = 1;

        if(!stricmp(extension, "deh"))
            strncpy(fileInfo->name, "DEHACKED", 8);
    }
    else
    {
        // WAD file.
        F_Read(&header, sizeof(header), handle);
        if(strncmp(header.identification, "IWAD", 4))
        {
            if(strncmp(header.identification, "PWAD", 4))
            {   // Bad file id
                Con_Error("Error:W_AddFile: Wad file %s does not have IWAD or PWAD id.\n", fileName);
            }
        }
        else
        {   // Found an IWAD.
            if(!stricmp(extension, "wad"))
                rec->iwad = true;
        }

        header.numLumps = LONG(header.numLumps);
        header.infoTableOfs = LONG(header.infoTableOfs);
        length = header.numLumps * sizeof(filelump_t);
        if(!(fileInfo = M_Malloc(length)))
        {
            Con_Error("Error:W_AddFile: Fileinfo allocation failed.\n");
        }

        freeFileInfo = fileInfo;
        F_Seek(handle, header.infoTableOfs, SEEK_SET);
        F_Read(fileInfo, length, handle);
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
    if(rec->iwad)
        Con_Message("  IWAD identification: %08x\n", W_CRCNumberForRecord(rec - records));

    return true;
}

boolean W_AddFile(const char* fileName, boolean allowDuplicate)
{
    return addFile(fileName, allowDuplicate);
}

static boolean removeFile(const char* fileName)
{
    int idx = W_RecordGetIdx(fileName);
    filerecord_t* rec;

    VERBOSE( Con_Message("Unloading %s ...\n", M_PrettyPath(fileName)) );

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
    F_ReleaseFileId(rec->fileName);

    // Destroy the file record.
    W_RecordDestroy(idx);

    PrimaryLumpInfo = lumpInfo;
    PrimaryLumpCache = lumpCache;
    PrimaryNumLumps = numLumps;

    // Success!
    return true;
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
            VERBOSE2( Con_Message("Done loading %s\n", M_PrettyPath(filenames[i])) );
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
            VERBOSE2( Con_Message("Done unloading %s\n", M_PrettyPath(filenames[i])) );
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

void W_Reset(void)
{
    boolean unloadedResources = false;
    Z_FreeTags(PU_CACHE, PU_CACHE);
    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        if(!(records[i].flags & FRF_RUNTIME))
            continue;
        if(removeFile(records[i].fileName))
        {
            i = -1;
            unloadedResources = true;
        }
    }}
    if(unloadedResources)
        DD_UpdateEngineState();
}

/**
 * @return              @c true, iff the given filename exists and is an IWAD.
 */
int W_IsIWAD(const char* fn)
{
    FILE*               file;
    char                id[5];
    const char*         ext;

    if(!M_FileExists(fn))
        return false;

    // Determine the file name extension.
    ext = strrchr(fn, '.');
    if(!ext)
        ext = "";
    else
        ext++; // Move to point after the dot.

    // Is it a gwa file? these are NOT IWADs even though they may be marked
    // as such in the header...
    if(!stricmp(ext, "gwa"))
        return false;

    if((file = fopen(fn, "rb")) == NULL)
        return false;

    fread(id, 4, 1, file);
    id[4] = 0;
    fclose(file);

    return !stricmp(id, "IWAD");
}

static void initLumpInfo(void)
{
    numLumps = 0;
    lumpInfo = Z_Malloc(1, PU_APPSTATIC, 0); // Will be realloced as lumps are added.
}

void W_Init(void)
{
    // This'll force the loader NOT the flag new records Runtime. (?)
    loadingForStartup = true;

    initLumpInfo();
}

void W_EndStartup(void)
{
    loadingForStartup = false;
}

lumpnum_t W_OpenAuxiliary(const char *fileName)
{
    int                 i;
    int                 size;
    wadinfo_t           header;
    DFILE              *handle;
    int                 length;
    filelump_t         *fileInfo;
    filelump_t         *sourceLump;
    lumpinfo_t         *destLump;

    if(AuxiliaryOpened)
    {
        W_CloseAuxiliary();
    }

    if((handle = F_Open(fileName, "rb")) == NULL)
    {
        Con_Error("Error:W_OpenAuxiliary: Failed to open %s", fileName);
        return -1; // Unreachable.
    }

    AuxiliaryHandle = handle;
    F_Read(&header, sizeof(header), handle);
    if(strncmp(header.identification, "IWAD", 4))
    {
        if(strncmp(header.identification, "PWAD", 4))
        {   // Bad file id
            Con_Error("Error:W_OpenAuxiliary: Wad file %s doesn't have IWAD or PWAD id.", fileName);
        }
    }

    header.numLumps = LONG(header.numLumps);
    header.infoTableOfs = LONG(header.infoTableOfs);
    length = header.numLumps * sizeof(filelump_t);
    fileInfo = M_Malloc(length);
    F_Seek(handle, header.infoTableOfs, SEEK_SET);
    F_Read(fileInfo, length, handle);
    numLumps = header.numLumps;

    // Init the auxiliary lumpInfo array
    lumpInfo = Z_Malloc(numLumps * sizeof(lumpinfo_t), PU_APPSTATIC, 0);
    sourceLump = fileInfo;
    destLump = lumpInfo;
    for(i = 0; i < numLumps; ++i, destLump++, sourceLump++)
    {
        destLump->handle = handle;
        destLump->position = LONG(sourceLump->filePos);
        destLump->size = LONG(sourceLump->size);
        strncpy(destLump->name, sourceLump->name, 8);
    }
    M_Free(fileInfo);

    // Allocate the auxiliary lumpcache array
    size = numLumps * sizeof(*lumpCache);
    lumpCache = Z_Malloc(size, PU_APPSTATIC, 0);
    memset(lumpCache, 0, size);

    AuxiliaryLumpInfo = lumpInfo;
    AuxiliaryLumpCache = lumpCache;
    AuxiliaryNumLumps = numLumps;
    AuxiliaryOpened = true;

    return AUXILIARY_BASE;
}

static void W_CloseAuxiliary(void)
{
    lumpnum_t           i;

    if(AuxiliaryOpened)
    {
        W_UseAuxiliary();
        for(i = 0; i < numLumps; ++i)
        {
            if(lumpCache[i])
            {
                Z_Free(lumpCache[i]);
            }
        }
        Z_Free(AuxiliaryLumpInfo);
        Z_Free(AuxiliaryLumpCache);
        W_CloseAuxiliaryFile();
        AuxiliaryOpened = false;
    }

    W_UsePrimary();
}

/**
 * WARNING: W_CloseAuxiliary() must be called before any further auxiliary
 * lump processing.
 */
static void W_CloseAuxiliaryFile(void)
{
    if(AuxiliaryHandle)
    {
        F_Close(AuxiliaryHandle);
        AuxiliaryHandle = 0;
    }
}

static void W_UsePrimary(void)
{
    lumpInfo = PrimaryLumpInfo;
    numLumps = PrimaryNumLumps;
    lumpCache = PrimaryLumpCache;
}

static boolean W_UseAuxiliary(void)
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
 * Scan backwards so patch lump files take precedence.
 */
static lumpnum_t W_ScanLumpInfo(int v[2])
{
    lumpinfo_t* lump_p = lumpInfo + numLumps;

    while(lump_p-- != lumpInfo)
    {
        if(*(int*)  lump_p->name    == v[0] &&
           *(int*) &lump_p->name[4] == v[1])
        {
            // W_Index handles the conversion to a logical index that is
            // independent of the
            return W_Index(lump_p - lumpInfo);
        }
    }
    return -1;
}

lumpnum_t W_CheckNumForName2(const char* name, boolean silent)
{
    lumpnum_t idx = -1;
    char name8[9];
    int v[2];

    // If the name string is empty, don't bother to search.
    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning:W_CheckNumForName: Empty name, returning invalid lumpnum.\n") );
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
    if(W_UseAuxiliary())
    {
        idx = W_ScanLumpInfo(v);
        if(idx != -1)
            return idx;
    }

    W_UsePrimary();
    idx = W_ScanLumpInfo(v);
    if(idx != -1)
        return idx;

    if(!silent)
        VERBOSE2( Con_Message("Warning:W_CheckNumForName: Lump \"%s\" not found.\n", name8) );
    return -1;
}

lumpnum_t W_CheckNumForName(const char* name)
{
    return W_CheckNumForName2(name, false);
}

lumpnum_t W_GetNumForName(const char* name)
{
    lumpnum_t           i;

    i = W_CheckNumForName(name);
    if(i != -1)
    {
        return i;
    }

    Con_Error("Error:W_GetNumForName: Lump \"%s\" not found.", name);
    return -1;
}

size_t W_LumpLength(lumpnum_t lump)
{
    lump = W_Select(lump);

    if(lump >= numLumps)
    {
        Con_Error("Error:W_LumpLength: Lumpnum %i >= numLumps.", lump);
    }

    return lumpInfo[lump].size;
}

const char* W_LumpName(lumpnum_t lump)
{
    lump = W_Select(lump);

    if(lump >= numLumps || lump < 0)
    {
        return NULL; // The caller must be able to handle this...
        //Con_Error("Error:W_LumpName: Lumpnum %i >= numLumps.", lump);
    }

    return lumpInfo[lump].name;
}

void W_ReadLump(lumpnum_t lump, void *dest)
{
    size_t              c;
    lumpinfo_t         *l;

    if(lump >= numLumps)
    {
        Con_Error("Error:W_ReadLump: Lumpnum %i >= numLumps.", lump);
    }

    l = &lumpInfo[lump];
    F_Seek(l->handle, l->position, SEEK_SET);
    c = F_Read(dest, l->size, l->handle);
    if(c < l->size)
    {
        Con_Error("Error:W_ReadLump: Only read %lu of %lu bytes of lump %i.",
                  (unsigned long) c, (unsigned long) l->size, lump);
    }
}

void W_ReadLumpSection(lumpnum_t lump, void *dest, size_t startOffset,
                       size_t length)
{
    size_t              c;
    lumpinfo_t         *l;

    lump = W_Select(lump);
    if(lump >= numLumps)
    {
        Con_Error("Error:W_ReadLumpSection: Lumpnum %i >= numLumps.", lump);
    }

    l = &lumpInfo[lump];
    F_Seek(l->handle, l->position + startOffset, SEEK_SET);
    c = F_Read(dest, length, l->handle);
    if(c < length)
    {
        Con_Error("Error:W_ReadLumpSection: Only read %lu of %lu bytes of lump %i.",
                  (unsigned long) c, (unsigned long) length, lump);
    }
}

boolean W_DumpLump(lumpnum_t lump, const char* fileName)
{
    FILE*               file;
    const byte*         lumpPtr;
    const char*         fname;
    char                buf[13]; // 8 (max lump chars) + 4 (ext) + 1.

    lump = W_Select(lump);
    if(lump >= numLumps)
        return false;

    lumpPtr = W_CacheLumpNum(lump, PU_APPSTATIC);

    if(fileName && fileName[0])
    {
        fname = fileName;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        dd_snprintf(buf, 13, "%s.dum", lumpInfo[lump].name);
        fname = buf;
    }

    if(!(file = fopen(fname, "wb")))
    {
        Con_Printf("Warning: Failed to open %s for writing (%s), aborting.\n", fname, strerror(errno));
        W_ChangeCacheTag(lump, PU_CACHE);
        return false;
    }

    fwrite(lumpPtr, 1, lumpInfo[lump].size, file);
    fclose(file);
    W_ChangeCacheTag(lump, PU_CACHE);

    Con_Printf("%s dumped to %s.\n", lumpInfo[lump].name, fname);
    return true;
}

void W_DumpLumpDir(void)
{
    char                buff[10];
    int                 p;

    printf("Lumps (%d total):\n", numLumps);
    for(p = 0; p < numLumps; p++)
    {
        strncpy(buff, W_LumpName(p), 8);
        buff[8] = 0;
        printf("%04i - %-8s (hndl: %p, pos: %i, size: %lu)\n", p, buff,
               lumpInfo[p].handle, lumpInfo[p].position,
               (unsigned long) lumpInfo[p].size);
    }
    Con_Error("---End of lumps---\n");
}

const void* W_CacheLumpNum(lumpnum_t absoluteLump, int tag)
{
    static char retName[9];

    lumpnum_t lump = W_Select(absoluteLump);
    byte* ptr;

    if((unsigned) lump >= (unsigned) numLumps)
    {
        Con_Error("Error:W_CacheLumpNum: Lumpnum %i >= numLumps.", lump);
    }

    // Return the name instead of data?
    if(tag == PU_GETNAME)
    {
        strncpy(retName, lumpInfo[lump].name, 8);
        retName[8] = 0;
        return retName;
    }

    if(!lumpCache[lump])
    {    // Need to read the lump in
        ptr = Z_Malloc(W_LumpLength(absoluteLump), tag, &lumpCache[lump]);
        W_ReadLump(lump, lumpCache[lump]);
    }
    else
    {
        Z_ChangeTag(lumpCache[lump], tag);
    }

    return lumpCache[lump];
}

const void* W_CacheLumpName(const char* name, int tag)
{
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}

void W_ChangeCacheTag(lumpnum_t lump, int tag)
{
    lump = W_Select(lump);
    if(lump < 0 || lump >= numLumps)
        Con_Error("Error:W_ChangeCacheTag: Bad lump number %i.", lump);
    if(!lumpCache[lump])
        return;
    //if(Z_GetBlock(lumpCache[lump])->id == 0x1d4a11)
    {
        Z_ChangeTag2(lumpCache[lump], tag);
    }
}

const char* W_LumpSourceFile(lumpnum_t lump)
{
    lump = W_Select(lump);
    if(lump < 0 || lump >= numLumps)
        Con_Error("Error:W_LumpSourceFile: Bad lump number %i.", lump);

    { filerecord_t* rec;
    if((rec = findFileRecordForLump(lump)))
    {
        return rec->fileName;
    }}
    return "";
}

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
static uint W_CRCNumberForRecord(int idx)
{
    lumpnum_t i;
    int k;
    unsigned int crc = 0;

    if(idx < 0 || idx >= numRecords)
        return 0;

    for(i = 0; i < numLumps; ++i)
    {
        if(lumpInfo[i].handle != records[idx].handle)
            continue;

        crc += (unsigned int) lumpInfo[i].size;
        for(k = 0; k < 8; ++k)
            crc += lumpInfo[i].name[k];
    }

    return crc;
}

uint W_CRCNumber(void)
{
    int i;
    // Find the IWAD's record.
    for(i = 0; i < numRecords; ++i)
        if(records[i].iwad)
            return W_CRCNumberForRecord(i);
    return 0;
}

void W_GetIWADFileName(char* buf, size_t bufSize)
{
    // Find the IWAD's record.
    int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(!rec->iwad)
            continue;

        { filename_t temp;
        Dir_FileName(temp, records[i].fileName, FILENAME_T_MAXLEN);
        strncpy(buf, temp, bufSize);
        }
        break;
    }
}

void W_GetPWADFileNames(char* buf, size_t bufSize, char separator)
{
    int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->iwad)
            continue;

        { filename_t temp;
        Dir_FileName(temp, rec->fileName, FILENAME_T_MAXLEN);
        if(stricmp(temp + strlen(temp) - 3, "lmp"))
            M_LimitedStrCat(buf, temp, 64, separator, bufSize);
        }
    }
}

boolean W_LumpFromIWAD(lumpnum_t lump)
{
    lump = W_Select(lump);
    if(lump < 0 || lump >= numLumps)
    {   // This lump doesn't exist.
        return false;
    }

    { int i;
    for(i = 0; i < numRecords; ++i)
    {
        filerecord_t* rec = &records[i];
        if(rec->handle == lumpInfo[lump].handle)
            return rec->iwad != 0;
    }
    }
    return false;
}

D_CMD(Dump)
{
    lumpnum_t lump;
    if((lump = W_CheckNumForName(argv[1])) == -1)
    {
        Con_Printf("No such lump.\n");
        return false;
    }
    if(!W_DumpLump(lump, NULL))
        return false;
    return true;
}

/**
 * Prints the resource path to the console.
 * This is a f_allresourcepaths_callback_t.
 */
int printResourcePath(const ddstring_t* fileName, pathdirectory_pathtype_t type,
    void* paramaters)
{
    assert(fileName && VALID_PATHDIRECTORY_PATHTYPE(type));
    {
    boolean makePretty = F_IsRelativeToBasePath(fileName);
    Con_Printf("  %s\n", makePretty? Str_Text(F_PrettyPath(fileName)) : Str_Text(fileName));
    return 0; // Continue the listing.
    }
}

static void printDirectory(const ddstring_t* path)
{
    ddstring_t dir;

    Str_Init(&dir); Str_Copy(&dir, path);
    Str_Strip(&dir);
    F_FixSlashes(&dir, &dir);
    // Make sure it ends in a directory separator character.
    if(Str_RAt(&dir, 0) != DIR_SEP_CHAR)
        Str_AppendChar(&dir, DIR_SEP_CHAR);
    if(!F_ExpandBasePath(&dir, &dir))
        F_PrependBasePath(&dir, &dir);

    Con_Printf("Directory: %s\n", Str_Text(F_PrettyPath(&dir)));

    // Make the pattern.
    Str_AppendChar(&dir, '*');
    F_AllResourcePaths(&dir, printResourcePath);

    Str_Free(&dir);
}

/**
 * Print contents of directories as Doomsday sees them.
 */
D_CMD(Dir)
{
    ddstring_t path;
    Str_Init(&path);
    if(argc > 1)
    {
        int i;
        for(i = 1; i < argc; ++i)
        {
            Str_Set(&path, argv[i]);
            printDirectory(&path);
        }
    }
    else
    {
        Str_Set(&path, "/");
        printDirectory(&path);
    }
    Str_Free(&path);
    return true;
}

D_CMD(ListWadFiles)
{
    int i;
    for(i = 0; i < numRecords; ++i)
    {
        Con_Printf("%s (%d lump%s%s)", records[i].fileName,
                   records[i].numLumps, records[i].numLumps != 1 ? "s" : "",
                   !(records[i].flags & FRF_RUNTIME) ? ", startup" : "");
        if(records[i].iwad)
            Con_Printf(" [%08x]", W_CRCNumberForRecord(i));
        Con_Printf("\n");
    }
    Con_Printf("Total: %d lumps in %d files.\n", numLumps, numRecords);
    return true;
}
