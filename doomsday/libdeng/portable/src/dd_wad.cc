/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * This version supports runtime (un)loading, replacement of flats and
 * sprites and IWAD checking.
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

#include "doomsday.h"
#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "r_extres.h"
#include "gl_draw.h" // for GL_SetFilter()

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    char            name[9]; // End in \0.
    DFILE*          handle;
    int             position;
    size_t          size;
    int             sent;
    char            group; // Lump grouping tag (LGT_*).
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
    char           *start, *end; // Start and end markers.
} grouping_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Dir);
D_CMD(Dump);
D_CMD(ListFiles);
D_CMD(LoadFile);
D_CMD(ResetLumps);
D_CMD(UnloadFile);

int     MarkerForGroup(char *name, boolean begin);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void W_CloseAuxiliary(void);
static void W_CloseAuxiliaryFile(void);
static void W_UsePrimary(void);
static boolean W_UseAuxiliary(void); // may not be available

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

lumpinfo_t *lumpInfo = 0;
int numLumps = 0;
void** lumpCache = 0;
int numCache = 0;

// The file records.
int numRecords = 0;
filerecord_t *records = 0;

char retName[9];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean loadingForStartup = true;
static boolean iwadLoaded = false;

static grouping_t lumpGroups[] = {
    {"", ""},
    {"F_START", "F_END"},       // Flats
    {"S_START", "S_END"}        // Sprites
};

static lumpinfo_t* PrimaryLumpInfo;
static int PrimaryNumLumps;
static void** PrimaryLumpCache;
static DFILE* AuxiliaryHandle;
static lumpinfo_t* AuxiliaryLumpInfo;
static int AuxiliaryNumLumps;
static void** AuxiliaryLumpCache;
boolean AuxiliaryOpened = false;

// CODE --------------------------------------------------------------------

void DD_RegisterVFS(void)
{
    C_CMD("dir", "s*", Dir);
    C_CMD("dump", "s", Dump);
    C_CMD("listfiles", "", ListFiles);
    C_CMD("load", "ss", LoadFile);
    C_CMD("ls", "s*", Dir);
    C_CMD("reset", "", ResetLumps);
    C_CMD("unload", "s*", UnloadFile);
}

static void convertSlashes(char* modifiableBuffer, size_t len)
{
    size_t          i;

    for(i = 0; i < len; ++i)
        if(modifiableBuffer[i] == '\\')
            modifiableBuffer[i] = '/';
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

/*
 * File Record Handling
 * (all functions named W_Record*)
 */

/**
 * Allocate a new file record.
 */
filerecord_t* W_RecordNew(void)
{
    filerecord_t*       ptr;

    records = (filerecord_t*) M_Realloc(records, sizeof(filerecord_t) * (++numRecords));
    ptr = records + numRecords - 1;
    memset(ptr, 0, sizeof(*ptr));
    return ptr;
}

int W_RecordGetIdx(const char* fileName)
{
    int                 i;
    filename_t          buf;

    // We have to make sure the slashes are correct.
    strncpy(buf, fileName, FILENAME_T_MAXLEN);
    convertSlashes(buf, FILENAME_T_MAXLEN);

    for(i = 0; i < numRecords; ++i)
        if(!strnicmp(records[i].fileName, buf, FILENAME_T_MAXLEN))
            return i;

    return -1;
}

/**
 * Destroy the specified record. Returns true if successful.
 */
boolean W_RecordDestroy(int idx)
{
    if(idx < 0 || idx > numRecords - 1)
        return false; // Huh?

    // First unallocate the memory of the record.
    //  M_Free(records[idx].indices);

    // Collapse the record array.
    if(idx != numRecords - 1)
        memmove(records + idx, records + idx + 1,
                sizeof(filerecord_t) * (numRecords - idx - 1));

    // Reallocate the records memory.
    numRecords--; // One less record.
    records = (filerecord_t*) M_Realloc(records, sizeof(filerecord_t) * numRecords);

    return true;
}

/**
 * Look for the named lump, starting from the specified index.
 */
lumpnum_t W_ScanForName(char *lumpname, int startfrom)
{
    char                name8[9];
    int                 v1, v2;
    int                 i;
    lumpinfo_t         *lump_p;

    if(startfrom < 0 || startfrom > numLumps - 1)
        return -1;

    memset(name8, 0, sizeof(name8));
    strncpy(name8, lumpname, 8);
    v1 = *(int *) name8;
    v2 = *(int *) (name8 + 4);

    // Start from the beginning.
    for(i = startfrom, lump_p = lumpInfo + startfrom; i < numLumps;
        i++, lump_p++)
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
void W_FillLumpInfo(int liIndex, filelump_t* flump, filerecord_t* rec,
                    int groupTag)
{
    lumpinfo_t*         lump = &lumpInfo[liIndex];

    lump->handle = rec->handle;
    lump->position = LONG(flump->filePos);
    lump->size = LONG(flump->size);
    memset(lump->name, 0, sizeof(lump->name));
    strncpy(lump->name, flump->name, 8);
    lump->group = groupTag;
}

/**
 * Moves 'count' lumpinfos, starting from 'from'. Updates the lumpcache.
 * Offset can only be positive.
 *
 * Lumpinfo and lumpcache are assumed to have enough memory for the
 * operation!
 */
void W_MoveLumps(int from, int count, int offset)
{
    int                 i;

    // Check that our information is valid.
    if(!offset || count <= 0 || from < 0 || from > numLumps - 1)
        return;

    // First update the lumpcache.
    memmove(lumpCache + from + offset, lumpCache + from,
            sizeof(*lumpCache) * count);
    for(i = from + offset; i < from + count + offset; ++i)
        if(lumpCache[i])
            Z_ChangeUser(lumpCache[i], lumpCache + i); // Update the user.

    // Clear the 'revealed' memory.
    if(offset > 0)              // Revealed from the beginning.
        memset(lumpCache + from, 0, offset * sizeof(*lumpCache));
    else                        // Revealed from the end.
        memset(lumpCache + from + count + offset, 0,
               -offset * sizeof(*lumpCache));

    // Lumpinfo.
    memmove(lumpInfo + from + offset, lumpInfo + from,
            sizeof(lumpinfo_t) * count);
}

/**
 * Moves the rest of the lumps forward.
 */
void W_InsertAndFillLumpRange(int toIndex, filelump_t *lumps, int num,
                              filerecord_t *rec, int groupTag)
{
    int                 i;

    // Move lumps if needed.
    if(toIndex < numLumps)
        W_MoveLumps(toIndex, numLumps - toIndex, num);

    // Now we can just fill in the lumps.
    for(i = 0; i < num; ++i)
        W_FillLumpInfo(toIndex + i, lumps + i, rec, groupTag);

    // Update the number of lumps.
    numLumps += num;
}

void W_RemoveLumpsWithHandle(DFILE *handle)
{
    int                 i, k, first, len;

    for(i = 0, first = -1; i < numLumps; ++i)
    {
        if(first < 0 && lumpInfo[i].handle == handle)
        {
            // Start a region.
            first = i;
            continue;
        }

        // Does a region end?
        if(first >= 0)
            if(lumpInfo[i].handle != handle || i == numLumps - 1 ||
               MarkerForGroup(lumpInfo[i].name, true) ||
               MarkerForGroup(lumpInfo[i].name, false))
            {
                if(lumpInfo[i].handle == handle && i == numLumps - 1)
                    i++;        // Also free the last one.

                // The length of the region.
                len = i - first;
                // Free the memory allocated for the region.
                for(k = first; k < i; ++k)
                {
                    //  Con_Message("removing lump: %s (%d)\n", lumpInfo[k].name, lumpInfo[k].handle);
                    if(lumpCache[k])
                    {

                        // If the block has a user, it must be explicitly freed.
                        /*
                        if((unsigned int)Z_GetUser(lumpCache[k]) > 0x100)
                           Z_Free(lumpCache[k]);
                        else if(Z_GetTag(lumpCache[k]) < PU_MAP)
                           Z_ChangeTag(lumpCache[k], PU_MAP);
                        Z_ChangeTag(lumpCache[k], PU_CACHE);
                        Z_ChangeUser(lumpCache[k], NULL);
                        */
                        if(Z_GetTag(lumpCache[k]) < PU_MAP)
                            Z_ChangeTag(lumpCache[k], PU_MAP);
                        // Mark the memory pointer in use, but unowned.
                        Z_ChangeUser(lumpCache[k], (void *) 0x2);
                    }
                }

                // Move the data in the lump storage.
                W_MoveLumps(i, numLumps - i, -len);
                numLumps -= len;
                i -= len;
                // Make it possible to begin a new region.
                first = -1;
            }
    }
}

/**
 * Reallocates lumpInfo and lumpcache.
 */
void W_ResizeLumpStorage(int numItems)
{
    lumpInfo = (lumpinfo_t*) M_Realloc(lumpInfo, sizeof(lumpinfo_t) * numItems);

    // Updating the cache is a bit more difficult. We need to make sure
    // the user pointers in the memory zone remain valid.
    if(numCache != numItems)
    {
        int                 i, numToMod;
        void              **newCache; // This is the new cache.
        size_t              newCacheBytes = numItems * sizeof(*newCache); // The new size of the cache (bytes).

        newCache = (void**) M_Calloc(newCacheBytes);
        if(lumpCache)
        {
            // Copy the old cache.
            if(numCache < numItems)
                numToMod = numCache;
            else
                numToMod = numItems;

            memcpy(newCache, lumpCache, numToMod * sizeof(*lumpCache));

            // Update the user information in the memory zone.
            for(i = 0; i < numToMod; ++i)
                if(newCache[i])
                    Z_ChangeUser(newCache[i], newCache + i);

            // Get rid of the old cache.
            M_Free(lumpCache);
        }

        lumpCache = newCache;
        numCache = numItems;
    }
}

/**
 * @return              One of the grouping tags.
 */
int MarkerForGroup(char *name, boolean begin)
{
    int                 i;

    for(i = 1; i < NUM_LGTAGS; ++i)
        if(!strnicmp(name, begin ? lumpGroups[i].start : lumpGroups[i].end, 8) ||
           !strnicmp(name + 1, begin ? lumpGroups[i].start : lumpGroups[i].end, 7))
            return i;

    // No matches...
    return LGT_NONE;
}

/**
 * Inserts the lumps in the fileinfo/record to their correct places in the
 * lumpInfo. Also maintains lumpInfo/records that all data is valid.
 *
 * The current placing of the lumps is that flats and sprites are added to
 * previously existing flat and sprite groups. All other lumps are just
 * appended to the end of the list.
 */
void W_InsertLumps(filelump_t* fileinfo, filerecord_t* rec)
{
    int                 i, to, num;
    filelump_t*         flump = fileinfo;
    int                 inside = LGT_NONE; // Not inside any group.
    int                 groupFirst = 0; // First lump in the current group.
    int                 maxNumLumps = numLumps + rec->numLumps; // This must be enough.

    // Allocate more memory for the lumpInfo.
    W_ResizeLumpStorage(maxNumLumps);

    for(i = 0; i < rec->numLumps; ++i, flump++)
    {
        /**
         * The Hexen demo on Mac uses the 0x80 on some lumps, maybe has
         * significance?
         * \todo: Ensure that this doesn't break other IWADs. The 0x80-0xff
         * range isn't normally used in lump names, right??
         */
        for(to = 0; to < 8; to++)
        {
            flump->name[to] = flump->name[to] & 0x7f;
        }

        if(inside == LGT_NONE)
        {
            // We are currently not inside any group.
            if((inside = MarkerForGroup(flump->name, true)) != LGT_NONE)
            {
                // We have entered a group! Go to the next lump.
                groupFirst = i + 1;
                continue;
            }

            // This lump is very ordinary. Just append it to the lumpInfo.
            //rec->indices[i] = numLumps;
            W_FillLumpInfo(numLumps++, flump, rec, inside);
        }
        else
        {
            if(MarkerForGroup(flump->name, false) == inside) // Group ends?
            {
                // This is how many lumps we'll add.
                num = i - groupFirst;

                // Find the existing group.
                to = W_ScanForName(lumpGroups[inside].end, 0);
                if(to < 0)
                {
                    // There is no existing group. Include the start and
                    // end markers in the range of lumps to add.
                    groupFirst--;
                    num += 2;
                    to = numLumps;
                }
                W_InsertAndFillLumpRange(to, &fileinfo[groupFirst], num,
                                         rec, inside);

                // We exit this group.
                inside = LGT_NONE;
            }
        }
    }

    // It may be that all lumps weren't added. Make sure we don't have
    // excess memory allocated (=synchronize the storage size with the
    // real numLumps).
    W_ResizeLumpStorage(numLumps);

    // Update the record with the number of lumps that were loaded.
    rec->numLumps -= maxNumLumps - numLumps;
}

/**
 * Files with a .wad extension are wadlink files with multiple lumps,
 * other files are single lumps with the base filename for the lump name.
 *
 * @return              @c true, if the operation is successful.
 */
boolean W_AddFile(const char* fileName, boolean allowDuplicate)
{
    filename_t          alterFileName;
    wadinfo_t           header;
    DFILE*              handle;
    unsigned int        length;
    filelump_t          singleInfo, *fileInfo, *freeFileInfo;
    filerecord_t*       rec;
    const char*         extension;

    // Filename given?
    if(!fileName || !fileName[0])
        return true;

    if((handle = F_Open(fileName, "rb")) == NULL)
    {
        // Didn't find file. Try reading from the data path.
        R_PrependDataPath(alterFileName, fileName, FILENAME_T_MAXLEN);
        if((handle = F_Open(alterFileName, "rb")) == NULL)
        {
            Con_Message("W_AddFile: ERROR: %s not found!\n", fileName);
            return false;
        }
        // We'll use this instead.
        fileName = alterFileName;
    }

    // Do not read files twice.
    if(!allowDuplicate && !M_CheckFileID(fileName))
    {
        F_Close(handle);        // The file is not used.
        return false;
    }

    Con_Message("W_AddFile: %s\n", M_PrettyPath(fileName));

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
    convertSlashes(rec->fileName, FILENAME_T_MAXLEN);
    rec->handle = handle;

    // If we're not loading for startup, flag the record to be a Runtime one.
    if(!loadingForStartup)
        rec->flags = FRF_RUNTIME;

    if(stricmp(extension, "wad")) // lmp?
    {
        int                 offset = 0;
        char*               slash = NULL;

        // Single lump file.
        fileInfo = &singleInfo;
        freeFileInfo = NULL;
        singleInfo.filePos = 0;
        singleInfo.size = LONG(F_Length(handle));

        // Is there a prefix to be omitted in the name?
        slash = strrchr(fileName, DIR_SEP_CHAR);
        // The slash mustn't be too early in the string.
        if(slash && slash >= fileName + 2)
        {
            // Good old negative indices.
            if(slash[-2] == '.' && slash[-1] >= '1' &&
               slash[-1] <= '9')
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
        if(!strncmp(header.identification, "JWAD", 4))
        {
            // This is treated like an IWAD, but we won't set the
            // iwadLoaded flag.
            rec->iwad = true;
        }
        else if(strncmp(header.identification, "IWAD", 4))
        {
            if(strncmp(header.identification, "PWAD", 4))
            {                   // Bad file id
                Con_Error("Wad file %s doesn't have IWAD or PWAD id\n",
                          fileName);
            }
        }
        else
        {
            // Found an IWAD.
            iwadLoaded = true;
            if(!stricmp(extension, "wad"))
                rec->iwad = true;
        }

        header.numLumps = LONG(header.numLumps);
        header.infoTableOfs = LONG(header.infoTableOfs);
        length = header.numLumps * sizeof(filelump_t);
        if(!(fileInfo = (filelump_t*) M_Malloc(length)))
        {
            Con_Error("W_AddFile: fileinfo malloc failed\n");
        }

        freeFileInfo = fileInfo;
        F_Seek(handle, header.infoTableOfs, SEEK_SET);
        F_Read(fileInfo, length, handle);
        rec->numLumps = header.numLumps;
    }

    // Insert the lumps to lumpInfo, into their rightful places.
    W_InsertLumps(fileInfo, rec);

    if(freeFileInfo)
        M_Free(freeFileInfo);

    PrimaryLumpInfo = lumpInfo;
    PrimaryLumpCache = lumpCache;
    PrimaryNumLumps = numLumps;

    // Print the 'CRC' number of the IWAD, so it can be identified.
    if(rec->iwad)
        Con_Message("  IWAD identification: %08x\n",
                    W_CRCNumberForRecord(rec - records));

    return true;
}

boolean W_RemoveFile(const char *fileName)
{
    int                 idx = W_RecordGetIdx(fileName);
    filerecord_t*       rec;

    if(idx == -1)
        return false; // No such file loaded.
    rec = records + idx;

    // We must remove all the data of this file from the lump storage
    // (lumpInfo + lumpcache).
    W_RemoveLumpsWithHandle(rec->handle);

    // Resize the lump storage to match numLumps.
    W_ResizeLumpStorage(numLumps);

    // Close the file, we don't need it any more.
    F_Close(rec->handle);

    // Destroy the file record.
    W_RecordDestroy(idx);

    PrimaryLumpInfo = lumpInfo;
    PrimaryLumpCache = lumpCache;
    PrimaryNumLumps = numLumps;

    // Success!
    return true;
}

/**
 * Remove all records flagged Runtime.
 */
void W_Reset(void)
{
    int                 i;

    for(i = 0; i < numRecords; ++i)
        if(records[i].flags & FRF_RUNTIME)
            W_RemoveFile(records[i].fileName);
}

/**
 * @return              @c true, iff the given filename exists and
 *                      is an IWAD.
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

/**
 * Determines if a file name refers to a PK3 package.
 *
 * @param fn            Path of the file.
 *
 * @return              @c true, if the file is a PK3 package.
 */
boolean W_IsPK3(const char* fn)
{
    size_t              len = strlen(fn);

    return (len > 4 && (!strnicmp(fn + len - 4, ".pk3", 4) ||
                        !strnicmp(fn + len - 4, ".zip", 4)));
}

/**
 * Pass a null terminated list of files to use. All files are optional, but
 * at least one file must be found. Lump names can appear multiple times.
 * The name searcher looks backwards, so a later file can override an
 * earlier one.
 */
void W_InitMultipleFiles(char** fileNames)
{
    char**              ptr;
    int                 numLoaded = 0;
    byte*               loaded = 0;

    // Count number of files.
    for(ptr = fileNames; *ptr; ptr++, numLoaded++);

    loaded = (byte*) M_Calloc(numLoaded);

    iwadLoaded = false;

    // Open all the files, load headers, and count lumps
    numLumps = 0;
    lumpInfo = (lumpinfo_t*) M_Malloc(1); // Will be realloced as lumps are added

    // This'll force the loader NOT the flag new records Runtime. (?)
    loadingForStartup = true;

    // Before anything else, load PK3s, because they may contain virtual
    // WAD files. (Should load WADs that have a DD_DIREC as well...)
    for(ptr = fileNames; *ptr; ptr++)
        if(W_IsPK3(*ptr))
        {
            loaded[ptr - fileNames] = true;
            W_AddFile(*ptr, false);
        }

    // IWAD(s) must be loaded before other WADs. Let's see if one has been
    // specified with -iwad or -file options.
    for(ptr = fileNames; *ptr; ptr++)
    {
        if(W_IsIWAD(*ptr))
        {
            loaded[ptr - fileNames] = true;
            W_AddFile(*ptr, false);
        }
    }

    // Do a full autoload round before loading any other WAD files.
    DD_AutoLoad();

    // Make sure an IWAD gets loaded; if not, display a warning.
    W_CheckIWAD();

    // Load the rest of the WADs.
    for(ptr = fileNames; *ptr; ptr++)
        if(!loaded[ptr - fileNames])
            W_AddFile(*ptr, false);

    // Bookkeeping no longer needed.
    M_Free(loaded);
    loaded = NULL;

    if(!numLumps)
    {
        Con_Error("W_InitMultipleFiles: no files found.\n");
    }

}

void W_EndStartup(void)
{
    // No more WADs will be loaded in startup mode.
    loadingForStartup = false;
}

/**
 * Reallocate the lump cache so that it has the right amount of memory.
 */
void W_UpdateCache(void)
{
    /*  unsigned int         i, numCopy;
       int                  size;
       void               **newCache;

       size = numLumps * sizeof(*lumpCache);
       newCache = M_Malloc(size);
       memset(newcache, 0, size);
       // The cache is a list of pointers to the memory zone. We must
       // update the zone user pointers.
       numCopy = cachesize / sizeof(*lumpCache); // Copy this many.
       // If the new cache is smaller, don't copy too much.
       if((unsigned)numLumps < numCopy)
       {
       //       for(i = numLumps; i < numCopy; ++i)
       //           if(lumpCache[i])
       //           {
       //               // The blocks aren't used by anyone any more.
       //               Z_Free(lumpCache[i]);
       //           }
       //       numCopy = numLumps;
       Con_Error("W_UpdateCache: Trying to copy more lumps than there are.\n");
       }
       for(i = 0; i < numCopy; i++)
       if(lumpCache[i])
       Z_ChangeUser(lumpCache[i], newCache+i);
       // Copy the old cache contents.
       memcpy(newCache, lumpCache, numCopy * sizeof(*lumpCache));

       // Get rid of the old cache.
       M_Free(lumpCache);
       lumpCache = newCache;
       cacheSize = size;

       PrimaryLumpInfo = lumpInfo;
       PrimaryLumpCache = lumpCache;
       PrimaryNumLumps = numLumps; */
}

/**
 * Initialize the primary from a single file.
 */
void W_InitFile(char *fileName)
{
    char               *names[2];

    names[0] = fileName;
    names[1] = NULL;
    W_InitMultipleFiles(names);
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
        Con_Error("W_OpenAuxiliary: %s not found.", fileName);
        return -1;
    }

    AuxiliaryHandle = handle;
    F_Read(&header, sizeof(header), handle);
    if(strncmp(header.identification, "IWAD", 4))
    {
        if(strncmp(header.identification, "PWAD", 4))
        {                       // Bad file id
            Con_Error("Wad file %s doesn't have IWAD or PWAD id\n", fileName);
        }
    }

    header.numLumps = LONG(header.numLumps);
    header.infoTableOfs = LONG(header.infoTableOfs);
    length = header.numLumps * sizeof(filelump_t);
    fileInfo = (filelump_t*) M_Malloc(length);
    F_Seek(handle, header.infoTableOfs, SEEK_SET);
    F_Read(fileInfo, length, handle);
    numLumps = header.numLumps;

    // Init the auxiliary lumpInfo array
    lumpInfo = (lumpinfo_t*) Z_Malloc(numLumps * sizeof(lumpinfo_t), PU_STATIC, 0);
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
    lumpCache = (void**) Z_Malloc(size, PU_STATIC, 0);
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

int W_NumLumps(void)
{
    return numLumps;
}

/**
 * Scan backwards so patch lump files take precedence.
 */
static lumpnum_t W_ScanLumpInfo(int v[2])
{
    lumpinfo_t*         lump_p = lumpInfo + numLumps;

    while(lump_p-- != lumpInfo)
    {
        if(*(int *) lump_p->name == v[0] &&
           *(int *) &lump_p->name[4] == v[1])
        {
            // W_Index handles the conversion to a logical index that is
            // independent of the
            return W_Index(lump_p - lumpInfo);
        }
    }
    return -1;
}

/**
 * @return              @c -1, if name not found, else lump num.
 */
lumpnum_t W_CheckNumForName(const char* name)
{
    char                name8[9];
    int                 v[2];
    lumpnum_t           idx = -1;

    // If the name string is empty, don't bother to search.
    if(!name || !name[0])
    {
        VERBOSE2(Con_Message("W_CheckNumForName: Empty name.\n"));
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

    VERBOSE2(Con_Message("W_CheckNumForName: \"%s\" not found.\n", name8));
    return -1;
}

/**
 * Calls W_CheckNumForName, but bombs out if not found.
 */
lumpnum_t W_GetNumForName(const char *name)
{
    lumpnum_t           i;

    i = W_CheckNumForName(name);
    if(i != -1)
    {
        return i;
    }

    Con_Error("W_GetNumForName: %s not found!", name);
    return -1;
}

/**
 * @return              The buffer size needed to load the given lump.
 */
size_t W_LumpLength(lumpnum_t lump)
{
    lump = W_Select(lump);

    if(lump >= numLumps)
    {
        Con_Error("W_LumpLength: %i >= numLumps", lump);
    }

    return lumpInfo[lump].size;
}

/**
 * Get the name of the given lump.
 */
const char *W_LumpName(lumpnum_t lump)
{
    lump = W_Select(lump);

    if(lump >= numLumps || lump < 0)
    {
        return NULL; // The caller must be able to handle this...
        //Con_Error("W_LumpName: %i >= numLumps", lump);
    }

    return lumpInfo[lump].name;
}

/**
 * Loads the lump into the given buffer, which must be >= W_LumpLength().
 */
void W_ReadLump(lumpnum_t lump, void *dest)
{
    size_t              c;
    lumpinfo_t         *l;

    if(lump >= numLumps)
    {
        Con_Error("W_ReadLump: %i >= numLumps", lump);
    }

    l = &lumpInfo[lump];
    F_Seek(l->handle, l->position, SEEK_SET);
    c = F_Read(dest, l->size, l->handle);
    if(c < l->size)
    {
        Con_Error("W_ReadLump: only read %lu of %lu on lump %i",
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
        Con_Error("W_ReadLumpSection: %i >= numLumps", lump);
    }

    l = &lumpInfo[lump];
    F_Seek(l->handle, l->position + startOffset, SEEK_SET);
    c = F_Read(dest, length, l->handle);
    if(c < length)
    {
        Con_Error("W_ReadLumpSection: only read %lu of %lu on lump %i",
                  (unsigned long) c, (unsigned long) length, lump);
    }
}

/**
 * Writes the specifed lump to file with the given name.
 *
 * @param lump
 */
boolean W_DumpLump(lumpnum_t lump, const char* fileName)
{
    FILE*               file;
    const byte*         lumpPtr;
    const char*         fname;
    char                buf[13]; // 8 (max lump chars) + 4 (ext) + 1.

    lump = W_Select(lump);
    if(lump >= numLumps)
        return false;

    lumpPtr = (const byte*) W_CacheLumpNum(lump, PU_STATIC);

    if(fileName && fileName[0])
    {
        fname = fileName;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 12, "%s.dum", lumpInfo[lump].name);
        fname = buf;
    }

    if(!(file = fopen(fname, "wb")))
    {
        Con_Printf("Couldn't open %s for writing. %s\n", fname,
                   strerror(errno));
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

/**
 * If called with the special purgelevel PU_GETNAME, returns a pointer
 * to the name of the lump.
 */
const void* W_CacheLumpNum(lumpnum_t absoluteLump, int tag)
{
    byte               *ptr;
    lumpnum_t           lump = W_Select(absoluteLump);

    if((unsigned) lump >= (unsigned) numLumps)
    {
        Con_Error("W_CacheLumpNum: %i >= numLumps", lump);
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
        ptr = (byte*) Z_Malloc(W_LumpLength(absoluteLump), tag, &lumpCache[lump]);
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
    if(lumpCache[lump])
        //if(Z_GetBlock(lumpCache[lump])->id == 0x1d4a11)
        {
            Z_ChangeTag2(lumpCache[lump], tag);
        }
}

/**
 * Checks if an IWAD has been loaded. If not, tries to load one of the
 * default ones.
 */
void W_CheckIWAD(void)
{
    extern char*        iwadList[];

    int                 i;

    if(iwadLoaded)
        return;

    // Try one of the default IWADs.
    for(i = 0; iwadList[i]; ++i)
    {
        if(M_FileExists(iwadList[i]))
            W_AddFile(iwadList[i], false);
        // We can leave as soon as an IWAD is found.
        if(iwadLoaded)
            return;
    }

    if(!Sys_CriticalMessage
       ("No IWAD has been specified! "
        "Important data might be missing. Are you sure you "
        "want to continue?"))
        Con_Error("W_CheckIWAD: Init aborted.\n");
}

/**
 * @return              The name of the WAD file where the given lump resides.
 *                      Always returns a valid filename (or an empty string).
 */
const char *W_LumpSourceFile(lumpnum_t lump)
{
    int                 i;
    lumpinfo_t         *l;

    lump = W_Select(lump);
    if(lump < 0 || lump >= numLumps)
        Con_Error("W_LumpSourceFile: Bad lump number: %i.", lump);

    l = lumpInfo + lump;
    for(i = 0; i < numRecords; ++i)
        if(records[i].handle == l->handle)
            return records[i].fileName;

    return "";
}

/**
 * An extremely simple formula. Does not conform to any CRC standard.
 * (So why's it called CRC, then?)
 */
unsigned int W_CRCNumberForRecord(int idx)
{
    lumpnum_t           i;
    int                 k;
    unsigned int        crc = 0;

    if(idx < 0 || idx >= numRecords)
        return 0;

    for(i = 0; i < numLumps; ++i)
    {
        if(lumpInfo[i].handle != records[idx].handle)
            continue;

        crc += lumpInfo[i].size;
        for(k = 0; k < 8; ++k)
            crc += lumpInfo[i].name[k];
    }

    return crc;
}

/**
 * Calculated using the lumps of the main IWAD.
 */
unsigned int W_CRCNumber(void)
{
    int                 i;

    // Find the IWAD's record.
    for(i = 0; i < numRecords; ++i)
        if(records[i].iwad)
            return W_CRCNumberForRecord(i);

    return 0;
}

/**
 * Copies the file name of the IWAD to the given buffer.
 */
void W_GetIWADFileName(char* buf, size_t bufSize)
{
    int                 i;

    // Find the IWAD's record.
    for(i = 0; i < numRecords; ++i)
        if(records[i].iwad)
        {
            filename_t          temp;

            Dir_FileName(temp, records[i].fileName, FILENAME_T_MAXLEN);
            strupr(temp);
            strncpy(buf, temp, bufSize);
            break;
        }
}

/**
 * Compiles a list of PWAD file names, separated by the specified character.
 */
void W_GetPWADFileNames(char* buf, size_t bufSize, char separator)
{
    int                 i;

    for(i = 0; i < numRecords; ++i)
        if(!records[i].iwad)
        {
            filename_t          temp;

            Dir_FileName(temp, records[i].fileName, FILENAME_T_MAXLEN);
            if(!stricmp(temp + strlen(temp) - 3, "lmp"))
                continue;

            M_LimitedStrCat(buf, temp, 64, separator, bufSize);
        }
}

/**
 * @return              @ true, if the specified lump is in an IWAD.
 *                      Otherwise it's from a PWAD.
 */
boolean W_IsFromIWAD(lumpnum_t lump)
{
    int                 i;

    lump = W_Select(lump);
    if(lump < 0 || lump >= numLumps)
    {
        // This lump doesn't exist.
        return false;
    }

    for(i = 0; i < numRecords; ++i)
        if(records[i].handle == lumpInfo[lump].handle)
            return records[i].iwad != 0;

    return false;
}

#if 0
/**
 * \fixme What about GL data?
 *
 * Change the map identifiers of all the maps in a PWAD.
 * 'episode' and 'map' are used with the first map in the PWAD, the rest
 * get incremented.
 */
boolean W_RemapPWADMaps(const char* pwadName, int episode, int map)
{
    int                 i;
    filerecord_t*       rec;
    filename_t          baseName, buf;

    // Try matching the full name first.
    if((i = W_RecordGetIdx(pwadName)) < 0)
    {
        // Then the base name only.
        M_ExtractFileBase(baseName, pwadName, FILENAME_T_MAXLEN);

        for(i = 0; i < numRecords; ++i)
        {
            M_ExtractFileBase(buf, records[i].fileName, FILENAME_T_MAXLEN);
            if(!stricmp(baseName, buf))
                break;
        }

        if(i == numRecords)
        {
            Con_Printf("%s has not been loaded.\n", pwadName);
            return false;
        }
    }

    rec = records + i;
    if(rec->iwad)
    {
        Con_Printf("%s is an IWAD!\n", rec->fileName);
        return false;
    }
    Con_Printf("Renumbering maps in %s.\n", rec->fileName);

    for(i = 0; i < numLumps; ++i)
    {
        // We'll only modify the maps
        if(lumpInfo[i].handle != rec->handle)
            continue;
    }
    return true;
}
#endif

D_CMD(LoadFile)
{
    int                 i, succeeded = false;

    for(i = 1; i < argc; ++i)
    {
        Con_Message("Loading %s...\n", argv[i]);
        if(W_AddFile(argv[i], true))
        {
            Con_Message("OK\n");
            succeeded = true;   // At least one has been loaded.
        }
        else
            Con_Message("Failed!\n");
    }

    // We only need to update if something was actually loaded.
    if(succeeded)
    {
        // Update the lumpcache.
        //W_UpdateCache();
        // The new wad may contain lumps that alter the current ones
        // in use.
        DD_UpdateEngineState();
    }
    return true;
}

D_CMD(UnloadFile)
{
    int                 i, succeeded = false;

    for(i = 1; i < argc; ++i)
    {
        filename_t          file;

        strncpy(file, argv[i], FILENAME_T_MAXLEN);
        Con_Message("Unloading %s...\n", file);
        if(W_RemoveFile(file))
        {
            Con_Message("OK\n");
            succeeded = true;
        }
        else
            Con_Message("Failed!\n");
    }

    if(succeeded)
        DD_UpdateEngineState();
    return true;
}

D_CMD(Dump)
{
    lumpnum_t           lump;

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
 * Print contents of directories as Doomsday sees them.
 */
D_CMD(Dir)
{
    filename_t          dir, pattern;
    int                 i;

    for(i = 1; i < argc; ++i)
    {
        M_PrependBasePath(dir, argv[i], FILENAME_T_MAXLEN);
        Dir_ValidDir(dir, FILENAME_T_MAXLEN);
        Dir_MakeAbsolute(dir, FILENAME_T_MAXLEN);
        Con_Printf("Directory: %s\n", dir);

        // Make the pattern.
        snprintf(pattern, FILENAME_T_MAXLEN, "%s*", dir);
        F_ForAll(pattern, dir, Con_PrintFileName);
    }

    return true;
}

D_CMD(ListFiles)
{
    int                 i;

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

D_CMD(ResetLumps)
{
    GL_SetFilter(false);
    W_Reset();
    Con_Message("Only startup files remain.\n");
    DD_UpdateEngineState();
    return true;
}
