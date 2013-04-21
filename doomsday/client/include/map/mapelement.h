/** @file mapelement.h Base class for all map elements.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_MAPELEMENT
#define DENG_WORLD_MAPELEMENT

#include "dd_share.h"

namespace de {

/**
 * Base class for all elements of a map. Provides runtime type information and
 * safe dynamic casting to various derived types.
 *
 * Maps are composed out of vertices, lines, sectors, etc.
 *
 * Abstract handling of map elements is particularly helpful in the public Map
 * Update (DMU) API, where objects can be referenced either by type and index
 * or by an opaque pointer.
 *
 * @ingroup map
 */
class MapElement
{
public:
    enum { NoIndex = -1 };

public:
    MapElement(int t = DMU_NONE)
        : _type(t), _indexInArchive(NoIndex), _indexInMap(NoIndex) {}

    virtual ~MapElement() {}

    int type() const
    {
        return _type;
    }

    /**
     * Returns the archive index for the map element. The archive index is
     * the position of the relevant data or definition in the archived map.
     * For example, in the case of a DMU_SIDE element that is produced from
     * an id tech 1 format map, this should be the index of the definition
     * in the SIDEDEFS data lump.
     *
     * @see setIndexInArchive()
     */
    int indexInArchive() const
    {
        return _indexInArchive;
    }

    /**
     * Change the "archive index" of the map element to @a newIndex.
     *
     * @see indexInArchive()
     */
    void setIndexInArchive(int newIndex = NoIndex)
    {
        _indexInArchive = newIndex;
    }

    int indexInMap() const
    {
        return _indexInMap;
    }

    void setIndexInMap(int newIndex = NoIndex)
    {
        _indexInMap = newIndex;
    }

    template <typename Type>
    inline Type *castTo()
    {
        Type *t = dynamic_cast<Type *>(this);
        DENG2_ASSERT(t != 0);
        return t;
    }

    template <typename Type>
    inline Type const *castTo() const
    {
        Type const *t = dynamic_cast<Type const *>(this);
        DENG_ASSERT(t != 0);
        return t;
    }

    MapElement &operator = (MapElement const &other)
    {
        _type = other._type;
        // We retain our current indexes.
        return *this;
    }

private:
    int _type;
    int _indexInArchive;
    int _indexInMap;
};

} // namespace de

#endif // DENG_WORLD_MAPELEMENT
