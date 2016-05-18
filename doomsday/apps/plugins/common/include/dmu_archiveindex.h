/** @file common/dmu_archiveindex.h DMU (object) Archive Index.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_DMU_ARCHIVEINDEX_H
#define LIBCOMMON_DMU_ARCHIVEINDEX_H

#include <vector>
#include <memory>

#include "common.h"

namespace dmu_lib {

/**
 * An index of objects which can be looked up by @ref DMU_ARCHIVE_INDEX.
 *
 * @note Population of the index is deferred until it is first accessed.
 */
template <de::dint ObjectTypeId>
class ArchiveIndex
{
public:
    ArchiveIndex() : _indexBase(-1) {}

    /**
     * Returns the object type id for DMU objects in this index.
     */
    static de::dint typeId() { return ObjectTypeId; }

    /**
     * Returns a pointer to the DMU object associated with the specified @a index.
     * May return @c 0 if no object is associated with that index.
     *
     * @see at()
     */
    inline DmuObjectPtr operator [] (de::dint index) const {
        return at(index);
    }

    /**
     * Returns a pointer to the DMU object associated with the specified @a index.
     * May return @c 0 if no object is associated with that index.
     *
     * @see operator []
     */
    DmuObjectPtr at(de::dint index) const {
        // Time to build the LUT?
        if(!_lut.get()) {
            const_cast<ArchiveIndex *>(this)->buildLut();
        }

        // Not indexed?
        if(!indexInLutRange(index)) return nullptr;

        return (*_lut)[index];
    }

private:
    bool inline indexInLutRange(de::dint index) const
    {
        if(!_lut.get()) return false;
        return (index - _indexBase >= 0 && (index - _indexBase) < de::dint( _lut->size() ));
    }

    void findIndexRange(de::dint &minIdx, de::dint &maxIdx)
    {
        minIdx = DDMAXINT;
        maxIdx = DDMININT;

        de::dint numElements = P_Count(ObjectTypeId);
        for(de::dint i = 0; i < numElements; ++i)
        {
            DmuObjectPtr ob = P_ToPtr(ObjectTypeId, i);
            DENG_ASSERT(DMU_GetType(ob) == ObjectTypeId);
            de::dint index = P_GetIntp(ob, DMU_ARCHIVE_INDEX);

            // Not indexed?
            if(index < 0) continue;

            if(index < minIdx) minIdx = index;
            if(index > maxIdx) maxIdx = index;
        }
    }

    /// @pre lut has been initialized and is large enough!
    void linkInLut(DmuObjectPtr ob)
    {
        de::dint index = P_GetIntp(ob, DMU_ARCHIVE_INDEX);

        // Not indexed?
        if(index < 0) return;

        DENG_ASSERT(indexInLutRange(index));
        (*_lut)[index - _indexBase] = ob;
    }

    void buildLut()
    {
        // Determine the size of the LUT.
        de::dint minIdx, maxIdx;
        findIndexRange(minIdx, maxIdx);

        de::dint lutSize = 0;
        if(minIdx > maxIdx) // None found?
        {
            _indexBase = 0;
        }
        else
        {
            _indexBase = minIdx;
            lutSize = maxIdx - minIdx + 1;
        }

        if(lutSize == 0) return;

        // Fill the LUT with initial values.
        _lut.reset(new DmuObjectLut(lutSize, DmuObjectPtr(nullptr)));

        // Populate the LUT.
        de::dint numElements  = P_Count(ObjectTypeId);
        for(de::dint i = 0; i < numElements; ++i)
        {
            linkInLut(P_ToPtr(ObjectTypeId, i));
        }
    }

private:
    typedef std::vector<DmuObjectPtr> DmuObjectLut;

    de::dint _indexBase;
    std::unique_ptr<DmuObjectLut> _lut;
};

///@{
typedef ArchiveIndex<DMU_LINE>    LineArchive;   ///< ArchiveIndex of DMU_LINE
typedef ArchiveIndex<DMU_SIDE>    SideArchive;   ///< ArchiveIndex of DMU_SIDE
typedef ArchiveIndex<DMU_SECTOR>  SectorArchive; ///< ArchiveIndex of DMU_SECTOR
///@}

} // namespace dmu_lib

#endif // LIBCOMMON_DMU_ARCHIVEINDEX_H
