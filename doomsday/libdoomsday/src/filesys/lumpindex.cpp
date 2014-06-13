/** @file lumpindex.cpp  Index of lumps.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1999-2006 by Colin Phipps, Florian Schulze, Neil Stevens, Andrey Budko (PrBoom 2.2.6)
 * @authors Copyright © 1999-2001 by Jess Haas, Nicolas Kalkhof (PrBoom 2.2.6)
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 by id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/filesys/lumpindex.h"

#include <QBitArray>
#include <QList>
#include <QVector>

#include <de/Log>
#include <de/NativePath>
#include <de/mathutil.h>

namespace de {

/**
 * @ingroup lumpIndexFlags
 */
///@{
#define LIF_INTERNAL_MASK               0xff000000
#define LIF_NEED_REBUILD_HASH           0x80000000 ///< Path hash map must be rebuilt.
#define LIF_NEED_PRUNE_DUPLICATES       0x40000000 ///< Path duplicate records must be pruned.
///@}

/// Stores indexes into LumpIndex::Instance::records forming a chain of
/// PathTree::Node fragment hashes. For ultra-fast name lookups.
struct LumpIndexHashRecord
{
    lumpnum_t head, next;
};

DENG2_PIMPL(LumpIndex)
{
    int flags;    ///< @ref lumpIndexFlags
    Lumps lumps;

    typedef QVector<LumpIndexHashRecord> HashMap;
    QScopedPointer<HashMap> hashMap;

    Instance(Public *i, int _flags)
        : Base   (i)
        , flags  (_flags & ~LIF_INTERNAL_MASK)
        , hashMap(0)
    {}

    ~Instance()
    {
        self.clear();
    }

    void buildHashMap()
    {
        if(!(flags & LIF_NEED_REBUILD_HASH)) return;

        int const numElements = lumps.size();
        if(hashMap.isNull())
        {
            hashMap.reset(new HashMap(numElements));
        }
        else
        {
            hashMap->resize(numElements);
        }

        // Clear the chains.
        DENG2_FOR_EACH(HashMap, i, *hashMap)
        {
            i->head = -1;
        }

        // Prepend nodes to each chain, in first-to-last load order, so that
        // the last lump with a given name appears first in the chain.
        for(int i = 0; i < numElements; ++i)
        {
            File1 const &lump = *(lumps[i]);
            PathTree::Node const &node = lump.directoryNode();
            ushort k = node.hash() % (unsigned)numElements;

            (*hashMap)[i].next = (*hashMap)[k].head; // Prepend to the chain.
            (*hashMap)[k].head = i;
        }

        flags &= ~LIF_NEED_REBUILD_HASH;

        LOG_RES_XVERBOSE("Rebuilt hashMap for LumpIndex %p") << &self;
    }

    /**
     * @param pruneFlags  Passed by reference to avoid deep copy on value-write.
     * @param file        Flag only those lumps contained by this file.
     *
     * @return Number of lumps newly flagged during this op.
     */
    int flagContainedLumps(QBitArray &pruneFlags, File1 &file)
    {
        DENG2_ASSERT(pruneFlags.size() == lumps.size());

        int const numRecords = lumps.size();
        int numFlagged = 0;
        for(int i = 0; i < numRecords; ++i)
        {
            if(pruneFlags.testBit(i)) continue;
            if(reinterpret_cast<File1 *>(&lumps[i]->container()) != &file) continue;
            pruneFlags.setBit(i, true);
            numFlagged += 1;
        }
        return numFlagged;
    }

    struct LumpSortInfo
    {
        File1 const *lump;
        String *path;
        int origIndex;
    };
    static int lumpSorter(void const *a, void const *b)
    {
        LumpSortInfo const *infoA = (LumpSortInfo const*)a;
        LumpSortInfo const *infoB = (LumpSortInfo const*)b;

        if(int delta = infoA->path->compare(infoB->path, Qt::CaseInsensitive))
            return delta;

        // Still matched; try the file load order indexes.
        if(int delta = (infoA->lump->container().loadOrderIndex() -
                        infoB->lump->container().loadOrderIndex()))
            return delta;

        // Still matched (i.e., present in the same package); use the original indexes.
        return (infoB->origIndex - infoA->origIndex);
    }

    /**
     * @param pruneFlags  Passed by reference to avoid deep copy on value-write.
     * @return Number of lumps newly flagged during this op.
     */
    int flagDuplicateLumps(QBitArray &pruneFlags)
    {
        DENG_ASSERT(pruneFlags.size() == lumps.size());

        // Any work to do?
        if(!(flags & LIF_UNIQUE_PATHS)) return 0;
        if(!(flags & LIF_NEED_PRUNE_DUPLICATES)) return 0;

        int const numRecords = lumps.size();
        if(numRecords <= 1) return 0;

        // Sort in descending load order for pruning.
        LumpSortInfo *sortInfos = new LumpSortInfo[numRecords];
        for(int i = 0; i < numRecords; ++i)
        {
            LumpSortInfo &sortInfo = sortInfos[i];
            File1 const *lump      = lumps[i];

            sortInfo.lump      = lump;
            sortInfo.origIndex = i;
            sortInfo.path      = new String(lump->composePath());
        }
        qsort(sortInfos, numRecords, sizeof(*sortInfos), lumpSorter);

        // Flag the lumps we'll be pruning.
        int numFlagged = 0;
        for(int i = 1; i < numRecords; ++i)
        {
            if(pruneFlags.testBit(i)) continue;
            if(sortInfos[i - 1].path->compare(sortInfos[i].path, Qt::CaseInsensitive)) continue;
            pruneFlags.setBit(sortInfos[i].origIndex, true);
            numFlagged += 1;
        }

        // Free temporary sort data.
        for(int i = 0; i < numRecords; ++i)
        {
            LumpSortInfo &sortInfo = sortInfos[i];
            delete sortInfo.path;
        }
        delete[] sortInfos;

        return numFlagged;
    }

    /// @return Number of pruned lumps.
    int pruneFlaggedLumps(QBitArray flaggedLumps)
    {
        DENG_ASSERT(flaggedLumps.size() == lumps.size());

        // Have we lumps to prune?
        int const numFlaggedForPrune = flaggedLumps.count(true);
        if(numFlaggedForPrune)
        {
            // We'll need to rebuild the hash after this.
            flags |= LIF_NEED_REBUILD_HASH;

            int numRecords = lumps.size();
            if(numRecords == numFlaggedForPrune)
            {
                lumps.clear();
            }
            else
            {
                // Do this one lump at a time, respecting the possibly-sorted order.
                for(int i = 0, newIdx = 0; i < numRecords; ++i)
                {
                    if(!flaggedLumps.testBit(i))
                    {
                        ++newIdx;
                        continue;
                    }

                    // Move the info for the lump to be pruned to the end.
                    lumps.move(newIdx, lumps.size() - 1);
                }

                // Erase the pruned lumps from the end of the list.
                int firstPruned = lumps.size() - numFlaggedForPrune;
                lumps.erase(lumps.begin() + firstPruned, lumps.end());
            }
        }
        return numFlaggedForPrune;
    }

    void pruneDuplicatesIfNeeded()
    {
        if(!(flags & LIF_NEED_PRUNE_DUPLICATES)) return;

        int const numRecords = lumps.size();
        if(numRecords <= 1) return;

        QBitArray pruneFlags(numRecords);
        flagDuplicateLumps(pruneFlags);
        pruneFlaggedLumps(pruneFlags);

        flags &= ~LIF_NEED_PRUNE_DUPLICATES;
    }
};

LumpIndex::LumpIndex(int flags) : d(new Instance(this, flags))
{}

bool LumpIndex::isValidIndex(lumpnum_t lumpNum) const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();
    return (lumpNum >= 0 && lumpNum < d->lumps.size());
}

static String invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    String msg = String("Invalid lump index %1").arg(invalidIdx);
    if(lastValidIdx < 0) msg += " (file is empty)";
    else                 msg += String(", valid range: [0..%2)").arg(lastValidIdx);
    return msg;
}

File1 &LumpIndex::lump(lumpnum_t lumpNum) const
{
    if(!isValidIndex(lumpNum)) throw NotFoundError("LumpIndex::lump", invalidIndexMessage(lumpNum, size() - 1));
    return *d->lumps[lumpNum];
}

LumpIndex::Lumps const &LumpIndex::lumps() const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();
    return d->lumps;
}

int LumpIndex::size() const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();
    return d->lumps.size();
}

int LumpIndex::pruneByFile(File1 &file)
{
    if(d->lumps.empty()) return 0;

    int const numRecords = d->lumps.size();
    QBitArray pruneFlags(numRecords);

    // We may need to prune path-duplicate lumps. We'll fold those into this
    // op as pruning may result in reallocations.
    d->flagDuplicateLumps(pruneFlags);

    // Flag the lumps we'll be pruning.
    int numFlaggedForFile = d->flagContainedLumps(pruneFlags, file);

    // Perform the prune.
    d->pruneFlaggedLumps(pruneFlags);

    d->flags &= ~LIF_NEED_PRUNE_DUPLICATES;

    return numFlaggedForFile;
}

bool LumpIndex::pruneLump(File1 &lump)
{
    if(d->lumps.empty()) return 0;

    d->pruneDuplicatesIfNeeded();

    // Prune this lump.
    if(!d->lumps.removeOne(&lump)) return false;

    // We'll need to rebuild the path hash chains.
    d->flags |= LIF_NEED_REBUILD_HASH;
    return true;
}

void LumpIndex::catalogLump(File1 &lump)
{
    d->lumps.push_back(&lump);

    // We'll need to rebuild the name hash chains.
    d->flags |= LIF_NEED_REBUILD_HASH;

    if(d->flags & LIF_UNIQUE_PATHS)
    {
        // We may need to prune duplicate paths.
        d->flags |= LIF_NEED_PRUNE_DUPLICATES;
    }
}

void LumpIndex::clear()
{
    d->lumps.clear();
    d->flags &= ~(LIF_NEED_REBUILD_HASH | LIF_NEED_PRUNE_DUPLICATES);
}

bool LumpIndex::catalogues(File1 &file)
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();

    DENG2_FOR_EACH(Lumps, i, d->lumps)
    {
        File1 const &lump = **i;
        if(&lump.container() == &file) return true;
    }
    return false;
}

lumpnum_t LumpIndex::lastIndexForPath(Path const &path) const
{
    if(path.isEmpty() || d->lumps.empty()) return -1;

    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();

    // We may need to rebuild the path hash map.
    d->buildHashMap();
    DENG2_ASSERT(!d->hashMap.isNull());

    // Perform the search.
    ushort hash = path.lastSegment().hash() % d->hashMap->size();
    if((*d->hashMap)[hash].head == -1) return -1;

    for(int idx = (*d->hashMap)[hash].head; idx != -1; idx = (*d->hashMap)[idx].next)
    {
        File1 const &lump = *d->lumps[idx];
        PathTree::Node const &node = lump.directoryNode();

        if(node.comparePath(path, 0)) continue;

        // This is the lump we are looking for.
        return idx;
    }

    return -1;
}

/// @todo Make use of the hash!
lumpnum_t LumpIndex::firstIndexForPath(Path const &path) const
{
    if(path.isEmpty() || d->lumps.empty()) return -1;

    // We may need to prune path-duplicate lumps.
    d->pruneDuplicatesIfNeeded();

    // We may need to rebuild the path hash map.
    d->buildHashMap();
    DENG2_ASSERT(!d->hashMap.isNull());

    // Perform the search.
    for(lumpnum_t idx = 0; idx < d->lumps.size(); ++idx)
    {
        File1 const &lump = *d->lumps[idx];
        PathTree::Node const &node = lump.directoryNode();

        if(node.comparePath(path, 0)) continue;

        // This is the lump we are looking for.
        return idx;
    }
    return -1;
}

void LumpIndex::print(LumpIndex const &index)
{
    int const numRecords = index.size();
    int const numIndexDigits = de::max(3, M_NumDigits(numRecords));

    LOG_RES_MSG("LumpIndex %p (%i records):") << &index << numRecords;

    int idx = 0;
    DENG2_FOR_EACH_CONST(Lumps, i, index.lumps())
    {
        File1 const &lump    = **i;
        String containerPath = NativePath(lump.container().composePath()).pretty();
        String lumpPath      = NativePath(lump.composePath()).pretty();

        LOG_RES_MSG(QString("%1 - \"%2:%3\" (size: %4 bytes%5)")
                    .arg(idx++, numIndexDigits, 10, QChar('0'))
                    .arg(containerPath)
                    .arg(lumpPath)
                    .arg(lump.info().size)
                    .arg(lump.info().isCompressed()? " compressed" : ""));
    }
    LOG_RES_MSG("---End of lumps---");
}

} // namespace de
