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

namespace dmu_lib {

/**
 * An index of objects which can be looked up by @ref DMU_ARCHIVE_INDEX.
 *
 * @note Population of the index is deferred until it is first accessed.
 */
class ArchiveIndex
{
public:
    /**
     * Create a new archive index for the specified DMU @a elementType.
     *
     * @param elementType  DMU element type of the objects to be indexed.
     */
    ArchiveIndex(int elementType)
        : _elementType(elementType),
          _indexBase(-1),
          _lut(0)
    {}

    /**
     * Returns the DMU element type which "this" indexes.
     */
    int type() const { return _elementType; }

    /**
     * Returns a pointer to the DMU object associated with the specified @a index.
     * May return @c 0 if no object is associated with that index.
     *
     * @see at()
     */
    inline MapElementPtr operator [] (int archiveIndex) const {
        return at(archiveIndex);
    }

    /**
     * Returns a pointer to the DMU object associated with the specified @a index.
     * May return @c 0 if no object is associated with that index.
     *
     * @see operator []
     */
    MapElementPtr at(int index) const
    {
        // Time to build the LUT?
        if(!_lut.get())
        {
            const_cast<ArchiveIndex *>(this)->buildLut();
        }

        // Not indexed?
        if(!indexInLutRange(index))
            return 0;

        return (*_lut)[index];
    }

private:
    bool inline indexInLutRange(int index) const
    {
        if(!_lut.get()) return false;
        return (index - _indexBase >= 0 && (index - _indexBase) < int( _lut->size() ));
    }

    void findIndexRange(int &minIdx, int &maxIdx)
    {
        minIdx = DDMAXINT;
        maxIdx = DDMININT;

        int numElements = P_Count(_elementType);
        for(int i = 0; i < numElements; ++i)
        {
            MapElementPtr element = P_ToPtr(_elementType, i);
            DENG_ASSERT(DMU_GetType(element) == _elementType);
            int index = P_GetIntp(element, DMU_ARCHIVE_INDEX);

            // Not indexed?
            if(index < 0) continue;

            if(index < minIdx) minIdx = index;
            if(index > maxIdx) maxIdx = index;
        }
    }

    /// @pre lut has been initialized and is large enough!
    void linkInLut(MapElementPtr element)
    {
        int index = P_GetIntp(element, DMU_ARCHIVE_INDEX);

        // Not indexed?
        if(index < 0) return;

        DENG_ASSERT(indexInLutRange(index));
        (*_lut)[index - _indexBase] = element;
    }

    void buildLut()
    {
        // Determine the size of the LUT.
        int minIdx, maxIdx;
        findIndexRange(minIdx, maxIdx);

        int lutSize = 0;
        if(minIdx > maxIdx) // None found?
        {
            _indexBase = 0;
        }
        else
        {
            _indexBase = minIdx;
            lutSize = maxIdx - minIdx + 1;
        }

        if(lutSize == 0)
            return;

        // Fill the LUT with initial values.
        _lut.reset(new ElementLut(lutSize, 0));

        // Populate the LUT.
        int numElements = P_Count(_elementType);
        for(int i = 0; i < numElements; ++i)
        {
            linkInLut(P_ToPtr(_elementType, i));
        }
    }

private:
    typedef std::vector<MapElementPtr> ElementLut;

    int _elementType;
    int _indexBase;
    std::auto_ptr<ElementLut> _lut;
};

} // namespace dmu_lib

#endif // LIBCOMMON_DMU_ARCHIVEINDEX_H
