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
#include <QVector>
#include <de/Log>

namespace de {
namespace internal
{
    struct LumpSortInfo
    {
        File1 const *lump;
        String path;
        int origIndex;
    };

    static int lumpSorter(void const *a, void const *b)
    {
        LumpSortInfo const *infoA = (LumpSortInfo const*)a;
        LumpSortInfo const *infoB = (LumpSortInfo const*)b;

        if(int delta = infoA->path.compare(infoB->path, Qt::CaseInsensitive))
            return delta;

        // Still matched; try the file load order indexes.
        if(int delta = (infoA->lump->container().loadOrderIndex() -
                        infoB->lump->container().loadOrderIndex()))
            return delta;

        // Still matched (i.e., present in the same package); use the original indexes.
        return (infoB->origIndex - infoA->origIndex);
    }

} // namespace internal

using namespace internal;

DENG2_PIMPL(LumpIndex)
{
    bool pathsAreUnique;

    Lumps lumps;
    bool needPruneDuplicateLumps;

    /// Stores indexes into records forming a chain of PathTree::Node fragment
    /// hashes. For ultra-fast lookup by path.
    struct PathHashRecord
    {
        lumpnum_t head, nextInLoadOrder;
    };
    typedef QVector<PathHashRecord> PathHash;
    QScopedPointer<PathHash> lumpsByPath;

    Instance(Public *i)
        : Base(i)
        , pathsAreUnique         (false)
        , needPruneDuplicateLumps(false)
    {}

    ~Instance() { self.clear(); }

    void buildLumpsByPathIfNeeded()
    {
        if(!lumpsByPath.isNull()) return;

        int const numElements = lumps.size();
        lumpsByPath.reset(new PathHash(numElements));

        // Clear the chains.
        DENG2_FOR_EACH(PathHash, i, *lumpsByPath)
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

            (*lumpsByPath)[i].nextInLoadOrder = (*lumpsByPath)[k].head;
            (*lumpsByPath)[k].head = i;
        }

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

    /**
     * @param pruneFlags  Passed by reference to avoid deep copy on value-write.
     * @return Number of lumps newly flagged during this op.
     */
    int flagDuplicateLumps(QBitArray &pruneFlags)
    {
        DENG2_ASSERT(pruneFlags.size() == lumps.size());

        // Any work to do?
        if(!pathsAreUnique) return 0;
        if(!needPruneDuplicateLumps) return 0;

        int const numRecords = lumps.size();
        if(numRecords <= 1) return 0;

        // Sort in descending load order for pruning.
        LumpSortInfo *sortInfos = new LumpSortInfo[numRecords];
        for(int i = 0; i < numRecords; ++i)
        {
            LumpSortInfo &sortInfo = sortInfos[i];
            File1 const *lump      = lumps[i];

            sortInfo.lump      = lump;
            sortInfo.path      = lump->composePath();
            sortInfo.origIndex = i;
        }
        qsort(sortInfos, numRecords, sizeof(*sortInfos), lumpSorter);

        // Flag the lumps we'll be pruning.
        int numFlagged = 0;
        for(int i = 1; i < numRecords; ++i)
        {
            if(pruneFlags.testBit(i)) continue;
            if(sortInfos[i - 1].path.compare(sortInfos[i].path, Qt::CaseInsensitive)) continue;
            pruneFlags.setBit(sortInfos[i].origIndex, true);
            numFlagged += 1;
        }

        // We're done with the sort info.
        delete[] sortInfos;

        return numFlagged;
    }

    /// @return Number of pruned lumps.
    int pruneFlaggedLumps(QBitArray flaggedLumps)
    {
        DENG2_ASSERT(flaggedLumps.size() == lumps.size());

        // Have we lumps to prune?
        int const numFlaggedForPrune = flaggedLumps.count(true);
        if(numFlaggedForPrune)
        {
            // We'll need to rebuild the hash after this.
            lumpsByPath.reset();

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
        if(!needPruneDuplicateLumps) return;
        needPruneDuplicateLumps = false;

        int const numRecords = lumps.size();
        if(numRecords <= 1) return;

        QBitArray pruneFlags(numRecords);
        flagDuplicateLumps(pruneFlags);
        pruneFlaggedLumps(pruneFlags);
    }
};

LumpIndex::LumpIndex(bool pathsAreUnique) : d(new Instance(this))
{
    d->pathsAreUnique = pathsAreUnique;
}

bool LumpIndex::hasLump(lumpnum_t lumpNum) const
{
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

File1 &LumpIndex::toLump(lumpnum_t lumpNum) const
{
    if(!hasLump(lumpNum)) throw NotFoundError("LumpIndex::lump", invalidIndexMessage(lumpNum, size() - 1));
    return *d->lumps[lumpNum];
}

LumpIndex::Lumps const &LumpIndex::allLumps() const
{
    d->pruneDuplicatesIfNeeded();
    return d->lumps;
}

int LumpIndex::size() const
{
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

    d->needPruneDuplicateLumps = false;

    return numFlaggedForFile;
}

bool LumpIndex::pruneLump(File1 &lump)
{
    if(d->lumps.empty()) return 0;

    d->pruneDuplicatesIfNeeded();

    // Prune this lump.
    if(!d->lumps.removeOne(&lump)) return false;

    // We'll need to rebuild the path hash chains.
    d->lumpsByPath.reset();

    return true;
}

void LumpIndex::catalogLump(File1 &lump)
{
    d->lumps.push_back(&lump);
    d->lumpsByPath.reset();    // We'll need to rebuild the path hash chains.

    if(d->pathsAreUnique)
    {
        // We may need to prune duplicate paths.
        d->needPruneDuplicateLumps = true;
    }
}

void LumpIndex::clear()
{
    d->lumps.clear();
    d->lumpsByPath.reset();
    d->needPruneDuplicateLumps = false;
}

bool LumpIndex::catalogues(File1 &file)
{
    d->pruneDuplicatesIfNeeded();

    DENG2_FOR_EACH(Lumps, i, d->lumps)
    {
        File1 const &lump = **i;
        if(&lump.container() == &file)
            return true;
    }

    return false;
}

int LumpIndex::findAll(Path const &path, FoundIndices &found) const
{
    LOG_AS("LumpIndex::findAll");

    found.clear();

    if(path.isEmpty() || d->lumps.empty()) return 0;

    d->pruneDuplicatesIfNeeded();
    d->buildLumpsByPathIfNeeded();

    // Perform the search.
    DENG2_ASSERT(!d->lumpsByPath.isNull());
    ushort hash = path.lastSegment().hash() % d->lumpsByPath->size();
    for(int idx = (*d->lumpsByPath)[hash].head; idx != -1;
        idx = (*d->lumpsByPath)[idx].nextInLoadOrder)
    {
        File1 const &lump          = *d->lumps[idx];
        PathTree::Node const &node = lump.directoryNode();

        if(!node.comparePath(path, 0))
        {
            found.push_front(idx);
        }
    }

    return int(found.size());
}

lumpnum_t LumpIndex::findLast(Path const &path) const
{
    if(path.isEmpty() || d->lumps.empty()) return -1;

    d->pruneDuplicatesIfNeeded();
    d->buildLumpsByPathIfNeeded();

    // Perform the search.
    DENG2_ASSERT(!d->lumpsByPath.isNull());
    ushort hash = path.lastSegment().hash() % d->lumpsByPath->size();
    for(int idx = (*d->lumpsByPath)[hash].head; idx != -1;
        idx = (*d->lumpsByPath)[idx].nextInLoadOrder)
    {
        File1 const &lump          = *d->lumps[idx];
        PathTree::Node const &node = lump.directoryNode();

        if(!node.comparePath(path, 0))
        {
            return idx; // This is the lump we are looking for.
        }
    }

    return -1; // Not found.
}

lumpnum_t LumpIndex::findFirst(Path const &path) const
{
    if(path.isEmpty() || d->lumps.empty()) return -1;

    d->pruneDuplicatesIfNeeded();
    d->buildLumpsByPathIfNeeded();

    lumpnum_t earliest = -1; // Not found.

    // Perform the search.
    DENG2_ASSERT(!d->lumpsByPath.isNull());
    ushort hash = path.lastSegment().hash() % d->lumpsByPath->size();
    for(int idx = (*d->lumpsByPath)[hash].head; idx != -1;
        idx = (*d->lumpsByPath)[idx].nextInLoadOrder)
    {
        File1 const &lump          = *d->lumps[idx];
        PathTree::Node const &node = lump.directoryNode();

        if(!node.comparePath(path, 0))
        {
            earliest = idx; // This is now the first lump loaded.
        }
    }

    return earliest;
}

} // namespace de
