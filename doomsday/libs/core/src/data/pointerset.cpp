/** @file pointerset.cpp  Set of pointers.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/pointerset.h"
#include <cstdlib>

namespace de {

static const duint16 POINTERSET_MIN_ALLOC      = 2;
static const duint16 POINTERSET_MAX_SIZE       = 0xffff;
static const PointerSet::Flag POINTERSET_ITERATION_MASK = 0x00ff;

const PointerSet::Flag PointerSet::AllowInsertionDuringIteration = 0x8000;

PointerSet::PointerSet()
    : _pointers(nullptr)
    , _iterationObserver(nullptr)
    , _flags(0)
    , _size (0)
{}

PointerSet::PointerSet(const PointerSet &other)
    : _iterationObserver(other._iterationObserver)
    , _flags(other._flags)
    , _size (other._size)
    , _range(other._range)
{
    const auto bytes = sizeof(Pointer) * _size;
    _pointers = reinterpret_cast<Pointer *>(malloc(bytes));
    std::memcpy(_pointers, other._pointers, bytes);
}

PointerSet::PointerSet(PointerSet &&moved)
    : _pointers(moved._pointers)
    , _iterationObserver(moved._iterationObserver)
    , _flags(moved._flags)
    , _size (moved._size)
    , _range(moved._range)
{
    moved._pointers = nullptr; // taken
}

PointerSet::~PointerSet()
{
    // PointerSet must not be deleted while someone is iterating it. If this happens,
    // you need to use Garbage instead of deleting the object immediately (e.g.,
    // deleting in response to a audience notification).
    DE_ASSERT(!isBeingIterated());

    free(_pointers);
}

void PointerSet::insert(Pointer ptr)
{
    if (!_pointers)
    {
        // Make a minimum allocation.
        _size = POINTERSET_MIN_ALLOC;
        _pointers = reinterpret_cast<Pointer *>(calloc(sizeof(Pointer), _size));
    }

    if (_range.isEmpty())
    {
        // Nothing is currently allocated. Place the first item in the middle.
        const duint16 pos = _size / 2;
        _pointers[pos] = ptr;
        _range.start = pos;
        _range.end = pos + 1;
    }
    else
    {
        const auto loc = locate(ptr);
        if (!loc.isEmpty()) return; // Already got it.

        if (isBeingIterated())
        {
            DE_ASSERT(_flags & AllowInsertionDuringIteration);

            if (!(_flags & AllowInsertionDuringIteration))
            {
                // This would likely cause the iteration to skip or repeat an item,
                // or even segfault if a reallocation occurs. Normally we will never
                // get here (user must ensure that the AllowInsertionDuringIteration
                // flag is set if needed).
                return;
            }

            // User must be aware that the allocation may change.
            DE_ASSERT(_iterationObserver != nullptr);
        }

        // Expand the array when the used range covers the entire array.
        if (_range.size() == _size)
        {
            DE_ASSERT(_size < POINTERSET_MAX_SIZE);
            if (_size == POINTERSET_MAX_SIZE) return; // Can't do it.

            Pointer *oldBase = _pointers;
            const duint oldSize = _size;

            _size = (_size < 0x8000? (_size * 2) : POINTERSET_MAX_SIZE);
            _pointers = reinterpret_cast<Pointer *>(realloc(_pointers, sizeof(Pointer) * _size));
            std::memset(_pointers + oldSize, 0, sizeof(Pointer) * (_size - oldSize));

            // If someone is interested, let them know about the relocation.
            if (_iterationObserver && _pointers != oldBase)
            {
                _iterationObserver->pointerSetIteratorsWereInvalidated(oldBase, _pointers);
            }
        }

        // Addition to the ends with room to spare?
        const duint16 pos = loc.start;
        if (pos == _range.start && _range.start > 0)
        {
            _pointers[--_range.start] = ptr;
        }
        else if (pos == _range.end && _range.end < _size)
        {
            _pointers[_range.end++] = ptr;
        }
        else
        {
            // We need to move existing items first to make room for the insertion.

            // Figure out the smallest portion of the range that needs to move.
            const duint16 middle = (_range.start + _range.end + 1)/2;
            if ((pos > middle && _range.end < _size) || // Less stuff to move toward the end.
                _range.start == 0)
            {
                // Move the second half of the range forward, extending it by one.
                DE_ASSERT(_range.end < _size);
                std::memmove(_pointers + pos + 1,
                             _pointers + pos,
                             sizeof(Pointer) * (_range.end - pos));
                _range.end++;
                _pointers[pos] = ptr;
            }
            else
            {
                // Have to move the first half of the range backward.
                DE_ASSERT(_range.start > 0);
                std::memmove(_pointers + _range.start - 1,
                             _pointers + _range.start,
                             sizeof(Pointer) * (pos < _range.end? (pos - _range.start + 1) :
                                                                  (_range.size())));
                _pointers[pos - 1] = ptr;
                _range.start--;
            }
        }
    }
}

void PointerSet::remove(Pointer ptr)
{
    const auto loc = locate(ptr);

    if (!loc.isEmpty())
    {
        DE_ASSERT(!_range.isEmpty());

        // Removing the first or last item needs just a range adjustment.
        if (loc.start == _range.start)
        {
            _pointers[_range.start++] = nullptr;
        }
        else if (loc.start == _range.end - 1 && !isBeingIterated())
        {
            _pointers[--_range.end] = nullptr;
        }
        else
        {
            // Move forward so that during iteration the future items won't be affected.
            std::memmove(_pointers + _range.start + 1,
                         _pointers + _range.start,
                         sizeof(Pointer) * (loc.start - _range.start));
            _pointers[_range.start++] = nullptr;
        }

        DE_ASSERT(_range.start <= _range.end);
    }
}

bool PointerSet::contains(Pointer ptr) const
{
    return !locate(ptr).isEmpty();
}

void PointerSet::clear()
{
    if (_pointers)
    {
        std::memset(_pointers, 0, sizeof(Pointer) * _size);
        _range = Rangeui16(_range.end, _range.end);
    }
}
    
PointerSet::Pointer PointerSet::take()
{
    if (isEmpty()) return nullptr;
    Pointer ptr = nullptr;
    // Make room at both ends.
    if (_size - _range.end < _range.start)
    {
        std::swap(ptr, _pointers[--_range.end]);
    }
    else
    {
        std::swap(ptr, _pointers[_range.start++]);
    }
    return ptr;
}

PointerSet &PointerSet::operator = (const PointerSet &other)
{
    const auto bytes = sizeof(Pointer) * other._size;

    if (_size != other._size)
    {
        _size = other._size;
        _pointers = reinterpret_cast<Pointer *>(realloc(_pointers, bytes));
    }
    std::memcpy(_pointers, other._pointers, bytes);
    _flags = other._flags;
    _range = other._range;
    _iterationObserver = other._iterationObserver;
    return *this;
}

PointerSet &PointerSet::operator = (PointerSet &&moved)
{
    free(_pointers);
    _pointers = moved._pointers;
    moved._pointers = nullptr;

    _flags = moved._flags;
    _size  = moved._size;
    _range = moved._range;

    _iterationObserver = moved._iterationObserver;
    return *this;
}

void PointerSet::setBeingIterated(bool yes) const
{
    duint16 count = _flags & POINTERSET_ITERATION_MASK;
    _flags ^= count;
    if (yes)
    {
        DE_ASSERT(count != POINTERSET_ITERATION_MASK);
        ++count;
    }
    else
    {
        DE_ASSERT(count != 0);
        --count;
    }
    _flags |= count & POINTERSET_ITERATION_MASK;
}

bool PointerSet::isBeingIterated() const
{
    return (_flags & POINTERSET_ITERATION_MASK) != 0;
}

void PointerSet::setIterationObserver(IIterationObserver *observer) const
{
    _iterationObserver = observer;
}

Rangeui16 PointerSet::locate(Pointer ptr) const
{
    // We will narrow down the span until the pointer is found or we'll know where
    // it would be if it were inserted.
    Rangeui16 span = _range;

    while (!span.isEmpty())
    {
        // Arrived at a single item?
        if (span.size() == 1)
        {
            if (at(span.start) == ptr)
            {
                return span; // Found it.
            }
            // Then the ptr would go before or after this position.
            if (ptr < at(span.start))
            {
                return Rangeui16(span.start, span.start);
            }
            return Rangeui16(span.end, span.end);
        }

        // Narrow down the search by a half.
        Rangeui16 const rightHalf((span.start + span.end + 1) / 2, span.end);
        const Pointer mid = at(rightHalf.start);
        if (ptr == mid)
        {
            // Oh, it's here.
            return Rangeui16(rightHalf.start, rightHalf.start + 1);
        }
        else if (ptr > mid)
        {
            span = rightHalf;
        }
        else
        {
            span = Rangeui16(span.start, rightHalf.start);
        }
    }
    return span;
}

} // namespace de
