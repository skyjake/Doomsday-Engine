/** @file defs/dedarray.h  Definition struct (POD) array.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_DEFINITION_ARRAY_H
#define LIBDOOMSDAY_DEFINITION_ARRAY_H

#include "../libdoomsday.h"
#include <de/vector.h>
#include <de/legacy/memory.h>
#include <cstring>

struct LIBDOOMSDAY_PUBLIC ded_count_s
{
    int num;
    int max;

    ded_count_s() : num(0), max(0) {}
};

typedef struct ded_count_s ded_count_t;

/**
 * Array of DED definitions.
 *
 * The array uses a memory management convention suitable for POD elements that are
 * copied using @c memcpy: element constructors and destructors are never called, so
 * ownership of data is managed manually using the elements' release() and reallocate()
 * methods.
 *
 * Any memory allocated by the elements is @em not released automatically. Also, the
 * array itself is also not freed in the destructor; clear() must be called before the
 * array goes out of scope.
 *
 * @a PODType must be a POD class. All elements are initialized to zero.
 * - The class must have a release() method that frees all memory owned by the element.
 * - The class must have a reallocate() method if the entire array is assigned to
 *   another or if elements are copied. The method must duplicate all memory owned by
 *   the element; a plain @c memcpy has already been done by DEDArray.
 *
 */
template <typename PODType>
struct LIBDOOMSDAY_PUBLIC DEDArray
{
    PODType *elements;
    ded_count_t count;

    DE_NO_COPY (DEDArray)

public:
    DEDArray() : elements(0) {}

    ~DEDArray()
    {
        // This should have been cleared by now.
        DE_ASSERT(elements == 0);
    }

    /// @note Previous elements are not released -- they must be cleared manually.
    DEDArray<PODType> &operator = (DEDArray<PODType> const &other)
    {
        elements = other.elements;
        count    = other.count;
        reallocate();
        return *this;
    }

    void releaseAll()
    {
        for (int i = 0; i < count.num; ++i)
        {
            elements[i].release();
        }
    }

    /**
     * Duplicates the array and all the elements. Previous elements are not released. The
     * assumption is that the array has been memcpy'd so it currently doesn't have
     * ownership of the elements.
     */
    void reallocate()
    {
        PODType *copied = (PODType *) M_Malloc(sizeof(PODType) * count.max);
        memcpy(copied, elements, sizeof(PODType) * count.num);
        elements = copied;
        for (int i = 0; i < count.num; ++i)
        {
            elements[i].reallocate();
        }
    }

    bool isEmpty() const
    {
        return !size();
    }

    int size() const
    {
        return count.num;
    }

    const PODType &at(int index) const
    {
        DE_ASSERT(index >= 0);
        DE_ASSERT(index < size());
        return elements[index];
    }

    PODType &operator [] (int index) const
    {
        DE_ASSERT(index >= 0);
        DE_ASSERT(index < size());
        return elements[index];
    }

    const PODType &first() const
    {
        return at(0);
    }

    const PODType &last() const
    {
        return at(size() - 1);
    }

    /**
     * Appends new, zeroed elements to the end of the array.
     *
     * @param addedCount  Number of elements to add.
     *
     * @return Pointer to the first new element.
     */
    PODType *append(int addedCount = 1)
    {
        DE_ASSERT(addedCount >= 0);

        const int first = count.num;

        count.num += addedCount;
        if (count.num > count.max)
        {
            count.max *= 2; // Double the size of the array.
            if (count.num > count.max) count.max = count.num;
            elements = reinterpret_cast<PODType *>(M_Realloc(elements, sizeof(PODType) * count.max));
        }

        PODType *np = elements + first;
        memset(np, 0, sizeof(PODType) * addedCount); // Clear the new entries.
        return np;
    }

    void removeAt(int index)
    {
        DE_ASSERT(index >= 0);
        DE_ASSERT(index < size());

        if (index < 0 || index >= size()) return;

        elements[index].release();

        memmove(elements + index,
                elements + (index + 1),
                sizeof(PODType) * (count.num - index - 1));

        if (--count.num < count.max / 2)
        {
            count.max /= 2;
            elements = (PODType *) M_Realloc(elements, sizeof(PODType) * count.max);
        }
    }

    void copyTo(int destIndex, int srcIndex)
    {
        DE_ASSERT(destIndex >= 0 && destIndex < size());
        DE_ASSERT(srcIndex >= 0 && srcIndex < size());

        // Free all existing allocations.
        elements[destIndex].release();

        // Do a plain copy and then duplicate allocations.
        memcpy(&elements[destIndex], &elements[srcIndex], sizeof(PODType));
        elements[destIndex].reallocate();
    }

    void copyTo(PODType *dest, const PODType *src)
    {
        copyTo(indexOf(dest), indexOf(src));
    }

    void copyTo(PODType *dest, int srcIndex)
    {
        copyTo(indexOf(dest), srcIndex);
    }

    int indexOf(const PODType *element) const
    {
        if (size() > 0 && element >= &first() && element <= &last())
            return element - elements;
        return -1;
    }

    void clear()
    {
        releaseAll();

        if (elements) M_Free(elements);
        elements = 0;

        count.num = count.max = 0;
    }
};

#endif // LIBDOOMSDAY_DEFINITION_ARRAY_H
