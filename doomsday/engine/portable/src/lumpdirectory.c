/**\file lumpdirectory.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"

#include "lumpdirectory.h"

/**
 * @ingroup lumpDirectoryFlags
 * @{
 */
#define LDF_INTERNAL_MASK               0xff000000
#define LDF_RECORDS_HASHDIRTY           0x80000000 // Hash needs a rebuild.
/**@}*/

typedef struct {
    // killough 1/31/98: hash table fields, used for ultra-fast hash table lookup
    lumpnum_t hashRoot, hashNext;
    lumpnum_t presortIndex;
    abstractfile_t* file;
    int lumpIdx;
} lumpdirectory_lumprecord_t;

struct lumpdirectory_s {
    int flags; /// @see lumpDirectoryFlags
    int numRecords;
    lumpdirectory_lumprecord_t* records;
};

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
static uint hashLumpShortName(const lumpname_t lumpName)
{
    const char* s = lumpName;
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

LumpDirectory* LumpDirectory_New(void)
{
    LumpDirectory* ld = (LumpDirectory*)malloc(sizeof(LumpDirectory));
    ld->numRecords = 0;
    ld->records = NULL;
    ld->flags = 0;
    return ld;
}

void LumpDirectory_Delete(LumpDirectory* ld)
{
    assert(ld);
    LumpDirectory_Clear(ld);
    free(ld);
}

boolean LumpDirectory_IsValidIndex(LumpDirectory* ld, lumpnum_t lumpNum)
{
    assert(ld);
    return (lumpNum >= 0 && lumpNum < ld->numRecords);
}

static const lumpdirectory_lumprecord_t* LumpDirectory_Record(LumpDirectory* ld, lumpnum_t lumpNum)
{
    assert(ld);
    if(LumpDirectory_IsValidIndex(ld, lumpNum))
    {
        return ld->records + lumpNum;
    }
    Con_Error("LumpDirectory::Record: Invalid lumpNum #%i (valid range: [0...%i).", lumpNum, ld->numRecords);
    exit(1); // Unreachable.
}

const LumpInfo* LumpDirectory_LumpInfo(LumpDirectory* ld, lumpnum_t lumpNum)
{
    const lumpdirectory_lumprecord_t* rec = LumpDirectory_Record(ld, lumpNum);
    return F_LumpInfo(rec->file, rec->lumpIdx);
}

int LumpDirectory_Size(LumpDirectory* ld)
{
    assert(ld);
    return ld->numRecords;
}

/**
 * Moves @a count lumps starting beginning at @a from.
 * \assume LumpDirectory::records is large enough for this operation!
 */
static void LumpDirectory_Move(LumpDirectory* ld, uint from, uint count, int offset)
{
    assert(ld);
    // Check that our information is valid.
    if(offset == 0 || count == 0 || from >= (unsigned)ld->numRecords-1)
        return;
    memmove(ld->records + from + offset, ld->records + from, sizeof(lumpdirectory_lumprecord_t) * count);
    // We'll need to rebuild the hash.
    ld->flags |= LDF_RECORDS_HASHDIRTY;
}

static void LumpDirectory_Resize(LumpDirectory* ld, int numItems)
{
    assert(ld);
    if(numItems < 0) numItems = 0;
    if(0 != numItems)
    {
        ld->records = (lumpdirectory_lumprecord_t*) realloc(ld->records, sizeof(*ld->records) * numItems);
        if(NULL == ld->records)
            Con_Error("LumpDirectory::Resize: Failed on (re)allocation of %lu bytes for "
                "LumpInfo record list.", (unsigned long) (sizeof(*ld->records) * numItems));
    }
    else if(ld->records)
    {
        free(ld->records), ld->records = NULL;
    }
}

int LumpDirectory_PruneByFile(LumpDirectory* ld, abstractfile_t* file)
{
    assert(ld);
    {
    int i, origNumLumps = ld->numRecords;

    if(!file || 0 == ld->numRecords) return 0;

    // Do this one lump at a time, respecting the possibly-sorted order.
    for(i = 1; i < ld->numRecords+1; ++i)
    {
        if(ld->records[i-1].file != file)
            continue;

        // Move the data in the lump storage.
        LumpDirectory_Move(ld, i, ld->numRecords-i, -1);
        --ld->numRecords;
        --i;
        // We'll need to rebuild the info short name hash chains.
        ld->flags |= LDF_RECORDS_HASHDIRTY;
    }
    // Resize the lump storage to match numRecords.
    LumpDirectory_Resize(ld, ld->numRecords);
    return origNumLumps - ld->numRecords;
    }
}

void LumpDirectory_Append(LumpDirectory* ld, abstractfile_t* file,
    int lumpIdxBase, int lumpIdxCount)
{
    assert(ld && file);
    {
    int maxRecords = ld->numRecords + lumpIdxCount; // This must be enough.
    int newRecordBase = ld->numRecords;
    int i;

    if(0 == lumpIdxCount)
        return;

    // Allocate more memory for the new records.
    LumpDirectory_Resize(ld, maxRecords);

    for(i = 0; i < lumpIdxCount; ++i)
    {
        lumpdirectory_lumprecord_t* record = ld->records + newRecordBase + i;
        record->file = file;
        record->lumpIdx = lumpIdxBase + i;
    }

    ld->numRecords += lumpIdxCount;

    // It may be that all lumps weren't added. Make sure we don't have
    // excess memory allocated (=synchronize the storage size with the
    // real numRecords).
    LumpDirectory_Resize(ld, ld->numRecords);

    // We'll need to rebuild the info short name hash chains.
    ld->flags |= LDF_RECORDS_HASHDIRTY;
    }
}

static void LumpDirectory_BuildHash(LumpDirectory* ld)
{
    int i;
    assert(ld);

    if(!(ld->flags & LDF_RECORDS_HASHDIRTY)) return;

    // First mark slots empty.
    for(i = 0; i < ld->numRecords; ++i)
    {
        ld->records[i].hashRoot = -1;
    }

    // Insert nodes to the beginning of each chain, in first-to-last
    // lump order, so that the last lump of a given name appears first
    // in any chain, observing pwad ordering rules. killough
    for(i = 0; i < ld->numRecords; ++i)
    {
        const LumpInfo* info = F_LumpInfo(ld->records[i].file, ld->records[i].lumpIdx);
        uint j = hashLumpShortName(info->name) % ld->numRecords;
        ld->records[i].hashNext = ld->records[j].hashRoot; // Prepend to list
        ld->records[j].hashRoot = i;
    }

    ld->flags &= ~LDF_RECORDS_HASHDIRTY;
#if _DEBUG
    VERBOSE2( Con_Message("Rebuilt record hash for LumpDirectory %p.\n", ld) )
#endif
}

void LumpDirectory_Clear(LumpDirectory* ld)
{
    assert(ld);
    if(ld->numRecords)
    {
        free(ld->records), ld->records = NULL;
    }
    ld->numRecords = 0;
    ld->flags &= ~LDF_RECORDS_HASHDIRTY;
}

static int cataloguesFileWorker(const LumpInfo* info, void* paramaters)
{
    return 1; // Stop iteration we need go no further.
}

boolean LumpDirectory_Catalogues(LumpDirectory* ld, abstractfile_t* file)
{
    if(!file) return false;
    return LumpDirectory_Iterate(ld, file, cataloguesFileWorker);
}

int LumpDirectory_Iterate2(LumpDirectory* ld, abstractfile_t* file,
    int (*callback) (const LumpInfo*, void*), void* paramaters)
{
    int result = 0;
    assert(ld);
    if(callback)
    {
        lumpdirectory_lumprecord_t* record;
        const LumpInfo* info;
        int i;
        for(i = 0; i < ld->numRecords; ++i)
        {
            record = ld->records + i;

            // Are we only interested in the lumps from a particular file?
            if(file && record->file != file) continue;

            info = F_LumpInfo(record->file, record->lumpIdx);
            result = callback(info, paramaters);
            if(result) break;
        }
    }
    return result;
}

int LumpDirectory_Iterate(LumpDirectory* ld, abstractfile_t* file,
    int (*callback) (const LumpInfo*, void*))
{
    return LumpDirectory_Iterate2(ld, file, callback, 0);
}

static lumpnum_t LumpDirectory_IndexForName2(LumpDirectory* ld, const char* name,
    boolean matchLumpName)
{
    assert(ld);
    if(!(!name || !name[0]) && ld->numRecords != 0)
    {
        PathMap searchPattern;
        register int idx;

        // Do we need to rebuild the name hash chains?
        LumpDirectory_BuildHash(ld);

        // Can we use the lump name hash?
        if(matchLumpName)
        {
            const int hash = hashLumpShortName(name) % ld->numRecords;
            idx = ld->records[hash].hashRoot;
            while(idx != -1 && strnicmp(F_LumpInfo(ld->records[idx].file, ld->records[idx].lumpIdx)->name, name, LUMPNAME_T_LASTINDEX))
                idx = ld->records[idx].hashNext;
            return idx;
        }

        /// \todo Do not resort to a linear search.
        PathMap_Initialize(&searchPattern, name);
        for(idx = ld->numRecords; idx-- > 0; )
        {
            const lumpdirectory_lumprecord_t* rec = ld->records + idx;
            PathDirectoryNode* node = F_LumpDirectoryNode(rec->file, rec->lumpIdx);

            if(PathDirectoryNode_MatchDirectory(node, 0, &searchPattern, NULL/*no paramaters*/))
            {
                // This is the lump we are looking for.
                break;
            }
        }
        PathMap_Destroy(&searchPattern);
        return idx;
    }
    return -1;
}

lumpnum_t LumpDirectory_IndexForPath(LumpDirectory* ld, const char* name)
{
    return LumpDirectory_IndexForName2(ld, name, false);
}

lumpnum_t LumpDirectory_IndexForName(LumpDirectory* ld, const char* name)
{
    return LumpDirectory_IndexForName2(ld, name, true);
}

int C_DECL LumpDirectory_CompareRecordName(const void* a, const void* b)
{
    const lumpdirectory_lumprecord_t* recordA = (const lumpdirectory_lumprecord_t*)a;
    const lumpdirectory_lumprecord_t* recordB = (const lumpdirectory_lumprecord_t*)b;
    const LumpInfo* infoA = F_LumpInfo(recordA->file, recordA->lumpIdx);
    const LumpInfo* infoB = F_LumpInfo(recordB->file, recordB->lumpIdx);
    int result = strnicmp(infoA->name, infoB->name, LUMPNAME_T_LASTINDEX);
    if(0 != result) return result;

    // Still matched; try the file load order indexes.
    result = (AbstractFile_LoadOrderIndex(recordA->file) - AbstractFile_LoadOrderIndex(recordB->file));
    if(0 != result) return result;

    // Still matched (i.e., present in the same package); use the pre-sort indexes.
    return (recordB->presortIndex > recordA->presortIndex);
}

int C_DECL LumpDirectory_CompareRecordPath(const void* a, const void* b)
{
    const lumpdirectory_lumprecord_t* recordA = (const lumpdirectory_lumprecord_t*)a;
    const lumpdirectory_lumprecord_t* recordB = (const lumpdirectory_lumprecord_t*)b;
    ddstring_t* pathA = F_ComposeLumpPath(recordA->file, recordA->lumpIdx);
    ddstring_t* pathB = F_ComposeLumpPath(recordB->file, recordB->lumpIdx);
    int result;

    result = Str_CompareIgnoreCase(pathA, Str_Text(pathB));

    Str_Delete(pathA);
    Str_Delete(pathB);
    if(0 != result) return result;

    // Still matched; try the file load order indexes.
    result = (AbstractFile_LoadOrderIndex(recordA->file) - AbstractFile_LoadOrderIndex(recordB->file));
    if(0 != result) return result;

    // Still matched (i.e., present in the same package); use the pre-sort indexes.
    return (recordB->presortIndex > recordA->presortIndex);
}

static int LumpDirectory_CompareRecords(const lumpdirectory_lumprecord_t* recordA,
    const lumpdirectory_lumprecord_t* recordB, boolean matchLumpName)
{
    ddstring_t* pathA, *pathB;
    int result;
    assert(recordA && recordB);

    if(matchLumpName)
    {
        return strnicmp(F_LumpInfo(recordA->file, recordA->lumpIdx)->name,
                        F_LumpInfo(recordB->file, recordB->lumpIdx)->name, LUMPNAME_T_MAXLEN);
    }

    pathA = F_ComposeLumpPath(recordA->file, recordA->lumpIdx);
    pathB = F_ComposeLumpPath(recordB->file, recordB->lumpIdx);

    result = Str_CompareIgnoreCase(pathA, Str_Text(pathB));

    Str_Delete(pathA);
    Str_Delete(pathB);
    return result;
}

void LumpDirectory_PruneDuplicateRecords(LumpDirectory* ld, boolean matchLumpName)
{
    int i, j, sortedNumRecords;
    assert(ld);

    if(ld->numRecords <= 1)
        return; // Obviously no duplicates.

    // Mark with pre-sort indices; so we can determine if qsort changed element ordinals.
    for(i = 0; i < ld->numRecords; ++i)
        ld->records[i].presortIndex = i;

    // Sort entries in descending load order for pruning.
    qsort(ld->records, ld->numRecords, sizeof(*ld->records),
        (matchLumpName? LumpDirectory_CompareRecordName : LumpDirectory_CompareRecordPath));

    // Peform the prune. One scan through the directory is enough (left relative).
    sortedNumRecords = ld->numRecords;
    for(i = 1; i < sortedNumRecords; ++i)
    {
        // Is this entry equal to one or more of the next?
        j = i;
        while(j < sortedNumRecords &&
              !LumpDirectory_CompareRecords(ld->records + (j-1),
                                            ld->records + (j), matchLumpName))
        { ++j; }
        if(j == i) continue; // No.

        // Shift equal-range -1 out of the set.
        if(i != sortedNumRecords-1)
            memmove(&ld->records[i], &ld->records[j],
                sizeof(lumpdirectory_lumprecord_t) * (sortedNumRecords-j));
        sortedNumRecords -= j-i;

        // We'll need to rebuild the hash.
        ld->flags |= LDF_RECORDS_HASHDIRTY;

        // We want to re-test with the new candidate.
        --i;
    }

    // Do we need to invalidate the hash?
    if(!(ld->flags & LDF_RECORDS_HASHDIRTY))
    {
        // As yet undecided. Compare the original ordinals.
        for(i = 0; i < sortedNumRecords; ++i)
        {
            if(i != ld->records[i-1].presortIndex)
                ld->flags |= LDF_RECORDS_HASHDIRTY;
        }
    }

    // Resize record storage to match?
    if(sortedNumRecords != ld->numRecords)
        LumpDirectory_Resize(ld, ld->numRecords = sortedNumRecords);
}

void LumpDirectory_Print(LumpDirectory* ld)
{
    int i;
    assert(ld);

    printf("LumpDirectory %p (%i records):\n", ld, ld->numRecords);

    for(i = 0; i < ld->numRecords; ++i)
    {
        lumpdirectory_lumprecord_t* rec = ld->records + i;
        const LumpInfo* info = F_LumpInfo(rec->file, rec->lumpIdx);
        ddstring_t* path = F_ComposeLumpPath(rec->file, rec->lumpIdx);
        printf("%04i - \"%s:%s\" (size: %lu bytes%s)\n", i,
               F_PrettyPath(Str_Text(AbstractFile_Path(rec->file))),
               F_PrettyPath(Str_Text(path)),
               (unsigned long) info->size, (info->compressedSize != info->size? " compressed" : ""));
        Str_Delete(path);
    }
    printf("---End of lumps---\n");
}
