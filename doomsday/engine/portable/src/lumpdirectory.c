/**\file lumpdirectory.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1999-2006 by Colin Phipps, Florian Schulze, Neil Stevens, Andrey Budko (PrBoom 2.2.6)
 *\author Copyright © 1999-2001 by Jess Haas, Nicolas Kalkhof (PrBoom 2.2.6)
 *\author Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
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

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"

#include "lumpdirectory.h"

typedef struct lumpdirectory_lumprecord_s {
    // killough 1/31/98: hash table fields, used for ultra-fast hash table lookup
    int index, next;
    wadfile_t* file;
    const wadfile_lumpinfo_t* info;
} lumpdirectory_lumprecord_t;

/**
 * This is a hash function. It uses the eight-character lump name to generate
 * a somewhat-random number suitable for use as a hash key.
 *
 * Originally DOOM used a sequential search for locating lumps by name. Large
 * wads with > 1000 lumps meant an average of over 500 were probed during every
 * search. Rewritten by Lee Killough to use a hash table for performance and
 * now the average is under 2 probes per search.
 *
 * @param  s  Lump name to be hashed.
 * @return  The generated hash key.
 */
static uint hashLumpName(const char* s)
{
    uint hash;
    (void) ((hash =          toupper(s[0]), s[1]) &&
            (hash = hash*3 + toupper(s[1]), s[2]) &&
            (hash = hash*2 + toupper(s[2]), s[3]) &&
            (hash = hash*2 + toupper(s[3]), s[4]) &&
            (hash = hash*2 + toupper(s[4]), s[5]) &&
            (hash = hash*2 + toupper(s[5]), s[6]) &&
            (hash = hash*2 + toupper(s[6]),
             hash = hash*2 + toupper(s[7]))
           );
    return hash;
}

lumpdirectory_t* LumpDirectory_Construct(void)
{
    return (lumpdirectory_t*)calloc(1, sizeof(lumpdirectory_t));
}

void LumpDirectory_Destruct(lumpdirectory_t* ld)
{
    assert(NULL != ld);
    LumpDirectory_Clear(ld);
    free(ld);
}

boolean LumpDirectory_IsValidIndex(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    assert(NULL != ld);
    return !(lumpNum < 0 || (unsigned)lumpNum >= ld->_numRecords);
}

static const lumpdirectory_lumprecord_t* LumpDirectory_Record(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    assert(NULL != ld);
    if(LumpDirectory_IsValidIndex(ld, lumpNum))
    {
        return ld->_records + lumpNum;
    }
    Con_Error("LumpDirectory::Record: Lump #%i >= numRecords.", lumpNum);
    exit(1); // Unreachable.
}

const wadfile_lumpinfo_t* LumpDirectory_LumpInfo(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    return LumpDirectory_Record(ld, lumpNum)->info;
}

wadfile_t* LumpDirectory_LumpSourceFile(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    return LumpDirectory_Record(ld, lumpNum)->file;
}

size_t LumpDirectory_NumLumps(lumpdirectory_t* ld)
{
    assert(NULL != ld);
    return ld->_numRecords;
}

/**
 * Moves @a count lumps starting beginning at @a from.
 * \assume LumpDirectory::records is large enough for this operation!
 */
static void LumpDirectory_Move(lumpdirectory_t* ld, size_t from, size_t count, size_t offset)
{
    assert(NULL != ld);
    // Check that our information is valid.
    if(offset == 0 || count == 0 || from > ld->_numRecords - 1)
        return;
    memmove(ld->_records + from + offset, ld->_records + from, sizeof(lumpdirectory_lumprecord_t) * count);
}

static void LumpDirectory_Resize(lumpdirectory_t* ld, size_t numItems)
{
    assert(NULL != ld);
    if(0 != numItems)
    {
        ld->_records = (lumpdirectory_lumprecord_t*)
            Z_Realloc(ld->_records, sizeof(*ld->_records) * numItems, PU_APPSTATIC);
        if(NULL == ld->_records)
            Con_Error("LumpDirectory::Resize: Failed on (re)allocation of %lu bytes for "
                "LumpInfo record list.", (unsigned long) (sizeof(*ld->_records) * numItems));
    }
    else if(NULL != ld->_records)
    {
        Z_Free(ld->_records), ld->_records = NULL;
    }
}

size_t LumpDirectory_PruneByFile(lumpdirectory_t* ld, wadfile_t* fileRecord)
{
    assert(NULL != ld);
    {
    size_t i, origNumLumps = ld->_numRecords;

    if(NULL == fileRecord) return 0;

    // Do this one lump at a time...
    for(i = 0; i < ld->_numRecords; ++i)
    {
        if(ld->_records[i].file != fileRecord)
            continue;
        // Move the data in the lump storage.
        LumpDirectory_Move(ld, i+1, 1, -1);
        --ld->_numRecords;
        --i;
    }

    // Resize the lump storage to match numRecords.
    LumpDirectory_Resize(ld, ld->_numRecords);
    if(ld->_numRecords != origNumLumps) ld->_hashDirty = true;

    return origNumLumps - ld->_numRecords;
    }
}

void LumpDirectory_Append(lumpdirectory_t* ld, const wadfile_lumpinfo_t* lumpInfo,
    size_t numLumpInfo, wadfile_t* fileRecord)
{
    assert(NULL != ld && NULL != fileRecord);
    {
    const wadfile_lumpinfo_t* info = lumpInfo;
    size_t maxRecords = ld->_numRecords + numLumpInfo; // This must be enough.
    size_t newRecordBase = ld->_numRecords;

    if(NULL == lumpInfo || 0 == numLumpInfo)
        return;

    // Allocate more memory for the new records.
    LumpDirectory_Resize(ld, maxRecords);

    { size_t i;
    for(i = 0; i < numLumpInfo; ++i, info++)
    {
        lumpdirectory_lumprecord_t* lump = ld->_records + newRecordBase + i;
        lump->file = fileRecord;
        lump->info = info;
    }}

    ld->_numRecords += numLumpInfo;

    // It may be that all lumps weren't added. Make sure we don't have
    // excess memory allocated (=synchronize the storage size with the
    // real numRecords).
    LumpDirectory_Resize(ld, ld->_numRecords);

    // We'll need to rebuild the info hash chains.
    ld->_hashDirty = true;
    }
}

static void LumpDirectory_BuildHash(lumpdirectory_t* ld)
{
    assert(NULL != ld);

    if(!ld->_hashDirty) return;

    // First mark slots empty.
    { size_t i;
    for(i = 0; i < ld->_numRecords; ++i)
    {
        ld->_records[i].index = -1;
    }}

    // Insert nodes to the beginning of each chain, in first-to-last
    // lump order, so that the last lump of a given name appears first
    // in any chain, observing pwad ordering rules. killough
    { size_t i;
    for(i = 0; i < ld->_numRecords; ++i)
    {
        int j = hashLumpName(ld->_records[i].info->name) % ld->_numRecords;
        ld->_records[i].next = ld->_records[j].index; // Prepend to list
        ld->_records[j].index = i;
    }}

    ld->_hashDirty = false;

#if _DEBUG
    VERBOSE2( Con_Message("LumpDirectory::Rebuilt hash.\n") )
#endif
}

void LumpDirectory_Clear(lumpdirectory_t* ld)
{
    assert(NULL != ld);
    if(ld->_numRecords)
    {
        Z_Free(ld->_records), ld->_records = NULL;
    }
    ld->_numRecords = 0;
    ld->_hashDirty = false;
}

lumpnum_t LumpDirectory_IndexForName(lumpdirectory_t* ld, const char* name)
{
    assert(NULL != ld);
    {
    register lumpnum_t idx = -1;
    if(!(!name || !name[0]) && ld->_numRecords != 0)
    {
        uint hash = hashLumpName(name);

        // Do we need to rebuild the name hash chains?
        LumpDirectory_BuildHash(ld);

        idx = ld->_records[hash % ld->_numRecords].index;
        while(idx >= 0 && strncasecmp(ld->_records[idx].info->name, name, LUMPNAME_T_LASTINDEX))
            idx = ld->_records[idx].next;
    }
    return idx;
    }
}

void LumpDirectory_Print(lumpdirectory_t* ld)
{
    assert(NULL != ld);
    printf("Lumps (%lu total):\n", (unsigned long)ld->_numRecords);
    { size_t i;
    for(i = 0; i < ld->_numRecords; i++)
    {
        lumpdirectory_lumprecord_t* rec = ld->_records + i;
        printf("%04i - %-8s (hndl: %p, pos: %lu, size: %lu)\n", i, rec->info->name,
               WadFile_Handle(rec->file), (unsigned long) rec->info->position, (unsigned long) rec->info->size);
    }}
    printf("---End of lumps---\n");
}

const char* LumpDirectory_LumpName(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    assert(NULL != ld);
    if(LumpDirectory_IsValidIndex(ld, lumpNum))
    {
        return ld->_records[lumpNum].info->name;
    }
    return NULL; // The caller must be able to handle this...
}

size_t LumpDirectory_LumpSize(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    const wadfile_lumpinfo_t* info = LumpDirectory_LumpInfo(ld, lumpNum);
    return info->size;
}

size_t LumpDirectory_LumpBaseOffset(lumpdirectory_t* ld, lumpnum_t lumpNum)
{
    const wadfile_lumpinfo_t* info = LumpDirectory_LumpInfo(ld, lumpNum);
    return info->position;
}
