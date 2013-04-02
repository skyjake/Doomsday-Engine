/** @file mapelement.h Base class for all map elements.
 * @ingroup map
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

#ifndef LIBDENG_MAPELEMENT_H
#define LIBDENG_MAPELEMENT_H

#include "dd_share.h"

#ifndef __cplusplus
#  error "map/mapelement.h requires C++"
#endif

#include <QList>

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
 */
class MapElement
{
public:
    enum { NoIndex = -1 };

public:
    MapElement(int t = DMU_NONE)
        : _type(t), _indexInList(NoIndex) {}

    virtual ~MapElement() {}

    int type() const
    {
        return _type;
    }

    int indexInList() const
    {
        return _indexInList;
    }

    void setIndexInList(int idx = NoIndex)
    {
        _indexInList = idx;
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
        DENG2_ASSERT(t != 0);
        return t;
    }

    MapElement &operator = (MapElement const &other)
    {
        _type = other._type;
        // We retain our current index in the list.
        return *this;
    }

private:
    int _type;
    int _indexInList;
};

/**
 * Collection of owned map elements with type @a Type. The template argument
 * @a Type must be a class derived from MapElement.
 *
 * The base class is protected because MapElement instances remember their
 * indices: all access to the list must occur through this class's public
 * interface so that the indices can be maintained appropriately.
 */
template <typename Type>
class MapElementList : protected QList<MapElement *>
{
    typedef QList<MapElement *> Base;

public:
    MapElementList() {}

    /**
     * Deletes all the objects in the list.
     */
    virtual ~MapElementList()
    {
        clear();
    }

    int size() const
    {
        return Base::size();
    }

    void clear()
    {
        // Delete all the objects.
        for(iterator i = begin(); i != end(); ++i)
        {
            delete *i;
        }
        Base::clear();
    }

    void clearAndResize(int count)
    {
        clear();
        while(count-- > 0)
        {
            Type *t = new Type;
            t->setIndexInList(size());
            append(t);
        }
    }

    /**
     * Looks up the index of an element in the list. Complexity: O(1).
     *
     * @param t  Element. Must be in the list.
     *
     * @return Index of the element @a t.
     */
    int indexOf(Type const *t) const
    {
        //DENG2_ASSERT(Base::indexOf(const_cast<Type *>(t)) == t->indexInList());
        return t->indexInList();
    }

    Type &operator [] (int index)
    {
        DENG_ASSERT(dynamic_cast<Type *>(at(index)) != 0);
        return *static_cast<Type *>(Base::at(index));
    }

    Type const &operator [] (int index) const
    {
        DENG_ASSERT(dynamic_cast<Type const *>(at(index)) != 0);
        return *static_cast<Type const *>(Base::at(index));
    }
};

} // namespace de

#endif // LIBDENG_MAPELEMENT_H
