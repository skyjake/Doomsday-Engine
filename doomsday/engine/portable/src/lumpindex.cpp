/**
 * @file lumpindex.cpp
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @author Copyright &copy; 1999-2006 by Colin Phipps, Florian Schulze, Neil Stevens, Andrey Budko (PrBoom 2.2.6)
 * @author Copyright &copy; 1999-2001 by Jess Haas, Nicolas Kalkhof (PrBoom 2.2.6)
 * @author Copyright &copy; 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "m_misc.h" // for M_NumDigits()
#include "lumpindex.h"

#include <QBitArray>
#include <QList>
#include <QVector>

#include <de/Log>
#include <de/NativePath>

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

struct LumpIndex::Instance
{
    typedef QVector<LumpIndexHashRecord> HashMap;

    LumpIndex* self;
    int flags; /// @see lumpIndexFlags
    LumpIndex::Lumps lumps;
    HashMap* hashMap;

    Instance(LumpIndex* d, int _flags)
        : self(d), flags(_flags & ~LIF_INTERNAL_MASK), lumps(), hashMap(0)
    {}

    ~Instance()
    {
        if(hashMap) delete hashMap;
    }

    void buildHashMap()
    {
        if(!(flags & LIF_NEED_REBUILD_HASH)) return;

        int const numElements = lumps.size();
        if(!hashMap)    hashMap = new HashMap(numElements);
        else            hashMap->resize(numElements);

        // Clear the chains.
        DENG2_FOR_EACH(HashMap, i, *hashMap)
        {
            i->head = -1;
        }

        // Prepend nodes to each chain, in first-to-last load order, so that
        // the last lump with a given name appears first in the chain.
        for(int i = 0; i < numElements; ++i)
        {
            File1 const& lump = *(lumps[i]);
            PathTree::Node const& node = lump.directoryNode();
            ushort j = node.hash() % (unsigned)numElements;

            (*hashMap)[i].next = (*hashMap)[j].head; // Prepend to the chain.
            (*hashMap)[j].head = i;
        }

        flags &= ~LIF_NEED_REBUILD_HASH;

        LOG_DEBUG("Rebuilt hashMap for LumpIndex %p.") << self;
    }

    /**
     * @param pruneFlags  Passed by reference to avoid deep copy on value-write.
     * @return Number of lumps newly flagged during this op.
     */
    int flagContainedLumps(QBitArray& pruneFlags, File1& file)
    {
        DENG_ASSERT(pruneFlags.size() == lumps.size());

        int const numRecords = lumps.size();
        int numFlagged = 0;
        for(int i = 0; i < numRecords; ++i)
        {
            if(pruneFlags.testBit(i)) continue;
            if(reinterpret_cast<File1*>(&lumps[i]->container()) != &file) continue;
            pruneFlags.setBit(i, true);
            numFlagged += 1;
        }
        return numFlagged;
    }

    struct LumpSortInfo
    {
        File1 const* lump;
        String* path;
        int origIndex;
    };
    static int lumpSorter(void const* a, void const* b)
    {
        LumpSortInfo const* infoA = (LumpSortInfo const*)a;
        LumpSortInfo const* infoB = (LumpSortInfo const*)b;
        int result = infoA->path->compare(infoB->path, Qt::CaseInsensitive);
        if(0 != result) return result;

        // Still matched; try the file load order indexes.
        result = (infoA->lump->container().loadOrderIndex() -
                  infoB->lump->container().loadOrderIndex());
        if(0 != result) return result;

        // Still matched (i.e., present in the same package); use the original indexes.
        return (infoB->origIndex - infoA->origIndex);
    }

    /**
     * @param pruneFlags  Passed by reference to avoid deep copy on value-write.
     * @return Number of lumps newly flagged during this op.
     */
    int flagDuplicateLumps(QBitArray& pruneFlags)
    {
        DENG_ASSERT(pruneFlags.size() == lumps.size());

        // Any work to do?
        if(!(flags & LIF_UNIQUE_PATHS)) return 0;
        if(!(flags & LIF_NEED_PRUNE_DUPLICATES)) return 0;

        int const numRecords = lumps.size();
        if(numRecords <= 1) return 0;

        // Sort in descending load order for pruning.
        LumpSortInfo* sortInfos = new LumpSortInfo[numRecords];
        for(int i = 0; i < numRecords; ++i)
        {
            LumpSortInfo& sortInfo   = sortInfos[i];
            File1 const* lump = lumps[i];

            sortInfo.lump = lump;
            sortInfo.origIndex = i;
            sortInfo.path = new String(lump->composePath());
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
            LumpSortInfo& sortInfo = sortInfos[i];
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

    void pruneDuplicates()
    {
        int const numRecords = lumps.size();
        if(numRecords <= 1) return;

        QBitArray pruneFlags(numRecords);
        flagDuplicateLumps(pruneFlags);
        pruneFlaggedLumps(pruneFlags);

        flags &= ~LIF_NEED_PRUNE_DUPLICATES;
    }
};

LumpIndex::LumpIndex(int flags)
{
    d = new Instance(this, flags);
}

LumpIndex::~LumpIndex()
{
    clear();
    delete d;
}

bool LumpIndex::isValidIndex(lumpnum_t lumpNum) const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();
    return (lumpNum >= 0 && lumpNum < d->lumps.size());
}

static QString invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    QString msg = QString("Invalid lump index %1 ").arg(invalidIdx);
    if(lastValidIdx < 0) msg += "(file is empty)";
    else                 msg += QString("(valid range: [0..%2])").arg(lastValidIdx);
    return msg;
}

File1& LumpIndex::lump(lumpnum_t lumpNum) const
{
    if(!isValidIndex(lumpNum)) throw NotFoundError("LumpIndex::lump", invalidIndexMessage(lumpNum, size() - 1));
    return *d->lumps[lumpNum];
}

LumpIndex::Lumps const& LumpIndex::lumps() const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();

    return d->lumps;
}

int LumpIndex::size() const
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();

    return d->lumps.size();
}

int LumpIndex::pruneByFile(File1& file)
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

bool LumpIndex::pruneLump(File1& lump)
{
    if(d->lumps.empty()) return 0;

    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();

    // Prune this lump.
    if(!d->lumps.removeOne(&lump)) return false;

    // We'll need to rebuild the path hash chains.
    d->flags |= LIF_NEED_REBUILD_HASH;
    return true;
}

void LumpIndex::catalogLumps(File1& file, int lumpIdxBase, int numLumps)
{
    if(numLumps <= 0) return;

#ifdef DENG2_QT_4_7_OR_NEWER
    // Allocate more memory for the new records.
    d->lumps.reserve(d->lumps.size() + numLumps);
#endif

    for(int i = 0; i < numLumps; ++i)
    {
        d->lumps.push_back(&file.lump(lumpIdxBase + i));
    }

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

bool LumpIndex::catalogues(File1& file)
{
    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();

    DENG2_FOR_EACH(Lumps, i, d->lumps)
    {
        File1 const& lump = **i;
        if(&lump.container() == &file) return true;
    }
    return false;
}

lumpnum_t LumpIndex::indexForPath(char const* path)
{
    if(!path || !path[0] || d->lumps.empty()) return -1;

    // We may need to prune path-duplicate lumps.
    d->pruneDuplicates();

    // We may need to rebuild the path hash map.
    d->buildHashMap();
    DENG_ASSERT(d->hashMap);

    // Perform the search.

    int hash = PathTree::hashPathFragment(path, strlen(path)) % d->hashMap->size();
    if((*d->hashMap)[hash].head == -1) return -1;

    PathMap searchPattern = PathMap(PathTree::hashPathFragment, path);

    for(int idx = (*d->hashMap)[hash].head; idx != -1; idx = (*d->hashMap)[idx].next)
    {
        File1 const& lump = *d->lumps[idx];
        PathTree::Node const& node = lump.directoryNode();

        if(!node.comparePath(searchPattern, 0)) continue;

        // This is the lump we are looking for.
        return idx;
    }

    return -1;
}

void LumpIndex::print(LumpIndex const& index)
{
    int const numRecords = index.size();
    int const numIndexDigits = MAX_OF(3, M_NumDigits(numRecords));

    Con_Printf("LumpIndex %p (%i records):\n", &index, numRecords);

    int idx = 0;
    DENG2_FOR_EACH_CONST(Lumps, i, index.lumps())
    {
        File1 const& lump = **i;
        QByteArray containerPath = de::NativePath(lump.container().composePath()).pretty().toUtf8();
        QByteArray lumpPath = de::NativePath(lump.composePath()).pretty().toUtf8();
        Con_Printf("%0*i - \"%s:%s\" (size: %lu bytes%s)\n", numIndexDigits, idx++,
                   containerPath.constData(),
                   lumpPath.constData(),
                   (unsigned long) lump.info().size,
                   (lump.info().isCompressed()? " compressed" : ""));
    }
    Con_Printf("---End of lumps---\n");
}

} // namespace de
