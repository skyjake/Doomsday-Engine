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

#include <de/Error>

#include "dd_share.h"
#include "map/p_dmu.h"

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
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

public:
    MapElement(int t = DMU_NONE)
        : _type(t), _indexInArchive(NoIndex), _indexInMap(NoIndex) {}

    virtual ~MapElement() {}

    /**
     * Returns the DMU_* type of the object.
     */
    int type() const;

    /**
     * Returns the archive index for the map element. The archive index is
     * the position of the relevant data or definition in the archived map.
     * For example, in the case of a DMU_SIDE element that is produced from
     * an id tech 1 format map, this should be the index of the definition
     * in the SIDEDEFS data lump.
     *
     * @see setIndexInArchive()
     */
    int indexInArchive() const;

    /**
     * Change the "archive index" of the map element to @a newIndex.
     *
     * @see indexInArchive()
     */
    void setIndexInArchive(int newIndex = NoIndex);

    int indexInMap() const;

    void setIndexInMap(int newIndex = NoIndex);

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

    /**
     * @note The Current index indices are retained.
     *
     * @see setIndexInArchive(), setIndexInMap()
     */
    MapElement &operator = (MapElement const &other);

    /**
     * Get a property value, selected by DMU_* name.
     *
     * Derived classes can override this to implement read access for additional
     * DMU properties. MapElement::property() must be called from an overridding
     * method if the named property is unknown/not handled, returning the result.
     * If the property is known and the read access is handled the overriding
     * method should return @c false.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    virtual int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * Derived classes can override this to implement write access for additional
     * DMU properties. MapElement::setProperty() must be called from an overridding
     * method if the named property is unknown/not handled, returning the result.
     * If the property is known and the write access is handled the overriding
     * method should return @c false.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    virtual int setProperty(setargs_t const &args);

private:
    int _type;
    int _indexInArchive;
    int _indexInMap;
};

} // namespace de

#endif // DENG_WORLD_MAPELEMENT
