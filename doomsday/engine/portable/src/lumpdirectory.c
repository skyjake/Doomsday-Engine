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
 */
///{
#define LDF_INTERNAL_MASK               0xff000000
#define LDF_NEED_REBUILD_HASH           0x80000000 ///< Path hash must be rebuilt.
#define LDF_NEED_PRUNE                  0x40000000 ///< Path duplicate records must be pruned.
///}

typedef struct {
    /// Info record for this lump in it's owning file.
    const LumpInfo* lumpInfo;

    /// @todo refactory away.
    int flags;

    /// Indexes into LumpDirectory::records forming a chain of PathDirectoryNode
    /// fragment hashes. For ultra-fast lookup.
    lumpnum_t hashRoot, hashNext;
} lumpdirectory_lumprecord_t;

struct lumpdirectory_s {
    int flags; /// @see lumpDirectoryFlags
    int numRecords;
    lumpdirectory_lumprecord_t* records;
};

static void LumpDirectory_Prune(LumpDirectory* ld);

LumpDirectory* LumpDirectory_NewWithFlags(int flags)
{
    LumpDirectory* ld = (LumpDirectory*)malloc(sizeof(LumpDirectory));
    ld->numRecords = 0;
    ld->records = NULL;
    ld->flags = flags & ~LDF_INTERNAL_MASK;
    return ld;
}

LumpDirectory* LumpDirectory_New(void)
{
    return LumpDirectory_NewWithFlags(0);
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

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    return (lumpNum >= 0 && lumpNum < ld->numRecords);
}

static __inline const lumpdirectory_lumprecord_t* LumpDirectory_Record(LumpDirectory* ld, lumpnum_t lumpNum)
{
    assert(ld);
    if(!LumpDirectory_IsValidIndex(ld, lumpNum)) return NULL;
    return ld->records + lumpNum;
}

const LumpInfo* LumpDirectory_LumpInfo(LumpDirectory* ld, lumpnum_t lumpNum)
{
    const lumpdirectory_lumprecord_t* rec = LumpDirectory_Record(ld, lumpNum);
    if(!rec) return NULL; // Invalid index?
    return rec->lumpInfo;
}

int LumpDirectory_Size(LumpDirectory* ld)
{
    assert(ld);

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    return ld->numRecords;
}

/**
 * Moves @a count lumps starting beginning at @a from.
 * @pre LumpDirectory::records is large enough for this operation!
 */
static void LumpDirectory_Move(LumpDirectory* ld, uint from, uint count, int offset)
{
    assert(ld);
    // Check that our information is valid.
    if(offset == 0 || count == 0 || from >= (unsigned)ld->numRecords-1) return;
    memmove(ld->records + from + offset, ld->records + from, sizeof(lumpdirectory_lumprecord_t) * count);
    // We'll need to rebuild the hash.
    ld->flags |= LDF_NEED_REBUILD_HASH;
}

static void LumpDirectory_Resize(LumpDirectory* ld, int numItems)
{
    assert(ld);
    if(numItems < 0) numItems = 0;
    if(numItems == ld->numRecords) return;
    if(0 != numItems)
    {
        ld->records = (lumpdirectory_lumprecord_t*) realloc(ld->records, sizeof(*ld->records) * numItems);
        if(!ld->records)
            Con_Error("LumpDirectory::Resize: Failed on (re)allocation of %lu bytes for "
                "LumpInfo record list.", (unsigned long) (sizeof(*ld->records) * numItems));
    }
    else if(ld->records)
    {
        free(ld->records);
        ld->records = NULL;
    }
}

int LumpDirectory_PruneByFile(LumpDirectory* ld, AbstractFile* file)
{
    int i, origNumLumps;
    assert(ld);

    if(!file || 0 == ld->numRecords) return 0;

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    // Do this one lump at a time, respecting the possibly-sorted order.
    origNumLumps = ld->numRecords;
    for(i = 1; i < ld->numRecords+1; ++i)
    {
        if(ld->records[i-1].lumpInfo->container != file) continue;

        // Move the data in the lump storage.
        LumpDirectory_Move(ld, i, ld->numRecords-i, -1);
        --ld->numRecords;
        --i;

        // We'll need to rebuild the path hash chains.
        ld->flags |= LDF_NEED_REBUILD_HASH;
    }

    // Resize the lump storage to match numRecords.
    LumpDirectory_Resize(ld, ld->numRecords);

    return origNumLumps - ld->numRecords;
}

boolean LumpDirectory_PruneLump(LumpDirectory* ld, LumpInfo* lumpInfo)
{
    int i;
    assert(ld);

    if(!lumpInfo || 0 == ld->numRecords) return 0;

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    for(i = 1; i < ld->numRecords+1; ++i)
    {
        if(ld->records[i-1].lumpInfo != lumpInfo) continue;

        // Move the data in the lump storage.
        LumpDirectory_Move(ld, i, ld->numRecords-i, -1);
        --ld->numRecords;

        // Resize the lump storage to match numRecords.
        LumpDirectory_Resize(ld, ld->numRecords);

        // We'll need to rebuild the path hash chains.
        ld->flags |= LDF_NEED_REBUILD_HASH;

        return true;
    }

    return false;
}

void LumpDirectory_CatalogLumps(LumpDirectory* ld, AbstractFile* file,
    int lumpIdxBase, int numLumps)
{
    lumpdirectory_lumprecord_t* record;
    int i, newRecordBase;
    assert(ld);

    if(!file || numLumps <= 0) return;

    newRecordBase = ld->numRecords;

    // Allocate more memory for the new records.
    LumpDirectory_Resize(ld, ld->numRecords + numLumps);

    record = ld->records + newRecordBase;
    for(i = 0; i < numLumps; ++i, record++)
    {
        record->lumpInfo = F_LumpInfo(file, lumpIdxBase + i);
        record->flags = 0;
    }
    ld->numRecords += numLumps;

    // It may be that all lumps weren't added. Make sure we don't have excess
    // memory allocated (=synchronize the storage size with the real numRecords).
    LumpDirectory_Resize(ld, ld->numRecords);

    // We'll need to rebuild the name hash chains.
    ld->flags |= LDF_NEED_REBUILD_HASH;

    if(ld->flags & LDF_UNIQUE_PATHS)
    {
        // We may need to prune duplicate paths.
        if(ld->numRecords > 1)
            ld->flags |= LDF_NEED_PRUNE;
    }
}

static void LumpDirectory_BuildHash(LumpDirectory* ld)
{
    int i;
    assert(ld);

    if(!(ld->flags & LDF_NEED_REBUILD_HASH)) return;

    // Clear the chains.
    for(i = 0; i < ld->numRecords; ++i)
    {
        ld->records[i].hashRoot = -1;
    }

    // Insert nodes to the beginning of each chain, in first-to-last lump order,
    // so that the last lump of a given name appears first in any chain,
    // observing pwad ordering rules.
    for(i = 0; i < ld->numRecords; ++i)
    {
        const LumpInfo* lumpInfo = ld->records[i].lumpInfo;
        const PathDirectoryNode* node = F_LumpDirectoryNode(lumpInfo->container,
                                                            lumpInfo->lumpIdx);
        ushort j = PathDirectoryNode_Hash(node) % (unsigned)ld->numRecords;

        ld->records[i].hashNext = ld->records[j].hashRoot; // Prepend to list
        ld->records[j].hashRoot = i;
    }

    ld->flags &= ~LDF_NEED_REBUILD_HASH;
#if _DEBUG
    VERBOSE2( Con_Message("Rebuilt record hash for LumpDirectory %p.\n", ld) )
#endif
}

void LumpDirectory_Clear(LumpDirectory* ld)
{
    assert(ld);
    if(ld->numRecords)
    {
        free(ld->records);
        ld->records = NULL;
    }
    ld->numRecords = 0;
    ld->flags &= ~(LDF_NEED_REBUILD_HASH|LDF_NEED_PRUNE);
}

static int findFirstLumpWorker(const LumpInfo* info, void* paramaters)
{
    return 1; // Stop iteration we need go no further.
}

boolean LumpDirectory_Catalogues(LumpDirectory* ld, AbstractFile* file)
{
    if(!file) return false;
    return LumpDirectory_Iterate(ld, file, findFirstLumpWorker);
}

int LumpDirectory_Iterate2(LumpDirectory* ld, AbstractFile* file,
    int (*callback) (const LumpInfo*, void*), void* paramaters)
{
    int result = 0;
    assert(ld);
    if(callback)
    {
        int i;
        lumpdirectory_lumprecord_t* record;

        // Do we need to prune path-duplicate lumps?
        LumpDirectory_Prune(ld);

        record = ld->records;
        for(i = 0; i < ld->numRecords; ++i, record++)
        {
            // Are we only interested in the lumps from a particular file?
            if(file && record->lumpInfo->container != file) continue;

            result = callback(record->lumpInfo, paramaters);
            if(result) break;
        }
    }
    return result;
}

int LumpDirectory_Iterate(LumpDirectory* ld, AbstractFile* file,
    int (*callback) (const LumpInfo*, void*))
{
    return LumpDirectory_Iterate2(ld, file, callback, 0);
}

lumpnum_t LumpDirectory_IndexForPath(LumpDirectory* ld, const char* path)
{
    boolean builtSearchPattern = false;
    PathMap searchPattern;
    int idx, hash;

    assert(ld);
    if(!path || !path[0] || ld->numRecords <= 0) return -1;

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    // Do we need to rebuild the path hash chains?
    LumpDirectory_BuildHash(ld);

    // Perform the search.
    hash = PathDirectory_HashPathFragment2(path, strlen(path), '/') % ld->numRecords;
    for(idx = ld->records[hash].hashRoot; idx != -1; idx = ld->records[idx].hashNext)
    {
        const LumpInfo* lumpInfo = ld->records[idx].lumpInfo;
        PathDirectoryNode* node = F_LumpDirectoryNode(lumpInfo->container, lumpInfo->lumpIdx);

        // Time to build the pattern?
        if(!builtSearchPattern)
        {
            PathMap_Initialize(&searchPattern, PathDirectory_HashPathFragment2, path);
            builtSearchPattern = true;
        }

        if(PathDirectoryNode_MatchDirectory(node, 0, &searchPattern, NULL/*no paramaters*/))
        {
            // This is the lump we are looking for.
            break;
        }
    }

    if(builtSearchPattern)
        PathMap_Destroy(&searchPattern);
    return idx;
}

typedef struct {
    lumpdirectory_lumprecord_t* record;
    AutoStr* path;
    int origIndex;
} lumpsortinfo_t;

int C_DECL LumpDirectory_Sorter(const void* a, const void* b)
{
    const lumpsortinfo_t* infoA = (const lumpsortinfo_t*)a;
    const lumpsortinfo_t* infoB = (const lumpsortinfo_t*)b;
    int result = Str_CompareIgnoreCase(infoA->path, Str_Text(infoB->path));
    if(0 != result) return result;

    // Still matched; try the file load order indexes.
    result = (AbstractFile_LoadOrderIndex(infoA->record->lumpInfo->container) -
              AbstractFile_LoadOrderIndex(infoB->record->lumpInfo->container));
    if(0 != result) return result;

    // Still matched (i.e., present in the same package); use the original indexes.
    return (infoB->origIndex - infoA->origIndex);
}

static void LumpDirectory_Prune(LumpDirectory* ld)
{
    lumpsortinfo_t* sortInfoSet, *sortInfo;
    lumpdirectory_lumprecord_t* record;
    int i, origNumLumps;
    assert(ld);

    if(!(ld->flags & LDF_UNIQUE_PATHS)) return;

    // Any work to do?
    if(!(ld->flags & LDF_NEED_PRUNE)) return;
    if(ld->numRecords <= 1) return;

    sortInfoSet = malloc(sizeof(*sortInfoSet) * ld->numRecords);
    if(!sortInfoSet) Con_Error("LumpDirectory::PruneDuplicateRecords: Failed on allocation of %lu bytes for sort info.", (unsigned long) (sizeof(*sortInfoSet) * ld->numRecords));

    record = ld->records;
    sortInfo = sortInfoSet;
    for(i = 0; i < ld->numRecords; ++i, record++, sortInfo++)
    {
        sortInfo->record = record;
        sortInfo->path = F_ComposeLumpPath2(record->lumpInfo->container, record->lumpInfo->lumpIdx, '/');
        sortInfo->origIndex = i;
    }

    // Sort in descending load order for pruning.
    qsort(sortInfoSet, ld->numRecords, sizeof(*sortInfoSet), LumpDirectory_Sorter);

    // Flag records we'll be pruning.
    for(i = 1; i < ld->numRecords; ++i)
    {
        if(Str_CompareIgnoreCase(sortInfoSet[i-1].path, Str_Text(sortInfoSet[i].path))) continue;
        sortInfoSet[i].record->flags |= 0x1;
    }

    // Free temporary storage.
    free(sortInfoSet);

    // Peform the prune. Do this one lump at a time, respecting the possibly-sorted order.
    origNumLumps = ld->numRecords;
    for(i = 1; i < ld->numRecords+1; ++i)
    {
        if(!(ld->records[i-1].flags & 0x1)) continue;

        // Move the data in the lump storage.
        LumpDirectory_Move(ld, i, ld->numRecords-i, -1);
        --ld->numRecords;
        --i;
    }

    // Resize the lump storage to match numRecords.
    LumpDirectory_Resize(ld, ld->numRecords);

    ld->flags &= ~LDF_NEED_PRUNE;
}

void LumpDirectory_Print(LumpDirectory* ld)
{
    int i;
    assert(ld);

    Con_Printf("LumpDirectory %p (%i records):\n", ld, ld->numRecords);

    // Do we need to prune path-duplicate lumps?
    LumpDirectory_Prune(ld);

    for(i = 0; i < ld->numRecords; ++i)
    {
        const LumpInfo* lumpInfo = ld->records[i].lumpInfo;
        AutoStr* path = F_ComposeLumpPath(lumpInfo->container, lumpInfo->lumpIdx);
        Con_Printf("%04i - \"%s:%s\" (size: %lu bytes%s)\n", i,
                   F_PrettyPath(Str_Text(AbstractFile_Path(lumpInfo->container))),
                   F_PrettyPath(Str_Text(path)),
                   (unsigned long) lumpInfo->size,
                   (lumpInfo->compressedSize != lumpInfo->size? " compressed" : ""));
    }
    Con_Printf("---End of lumps---\n");
}
