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

#include "de/PointerSet"
#include <cstdlib>

namespace de {

static duint16 const POINTERSET_MIN_ALLOC = 2;
static duint16 const POINTERSET_MAX_SIZE  = 0xffff;

PointerSet::Flag const PointerSet::AllowInsertionDuringIteration = 0x1;
PointerSet::Flag const PointerSet::BeingIterated                 = 0x2;

PointerSet::PointerSet()
    : _pointers(nullptr)
    , _flags   (0)
    , _size    (0)
{}

PointerSet::PointerSet(PointerSet const &other)
    : _flags(other._flags)
    , _size (other._size)
    , _range(other._range)
{
    auto const bytes = sizeof(Pointer) * _size;
    _pointers = reinterpret_cast<Pointer *>(malloc(bytes));
    std::memcpy(_pointers, other._pointers, bytes);
}

PointerSet::PointerSet(PointerSet &&moved)
    : _pointers(moved._pointers)
    , _flags   (moved._flags)
    , _size    (moved._size)
    , _range   (moved._range)
{
    moved._pointers = nullptr; // taken
}

PointerSet::~PointerSet()
{
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
        duint16 const pos = _size / 2;
        _pointers[pos] = ptr;
        _range.start = pos;
        _range.end = pos + 1;
    }
    else
    {
        auto const loc = locate(ptr);
        if (!loc.isEmpty()) return; // Already got it.

        if (_flags & BeingIterated)
        {
            DENG2_ASSERT(_flags & AllowInsertionDuringIteration);
        }

        // Do we need to expand?
        if (_range.size() == _size)
        {
            DENG2_ASSERT(_size < POINTERSET_MAX_SIZE);

            duint const oldSize = _size;
            _size = (_size < 0x8000? (_size * 2) : POINTERSET_MAX_SIZE);
            _pointers = reinterpret_cast<Pointer *>(realloc(_pointers, sizeof(Pointer) * _size));
            std::memset(_pointers + oldSize, 0, sizeof(Pointer) * (_size - oldSize));
        }

        // Addition to the ends with room to spare?
        duint16 const pos = loc.start;
        if (pos == _range.start && _range.start > 0)
        {
            _pointers[--_range.start] = ptr;
        }
        else if (pos == _range.end && _range.end < _size)
        {
            _pointers[_range.end++] = ptr;
        }
        else // Need to move first to make room.
        {
            duint16 const middle = (_range.start + _range.end + 1)/2;
            if ((pos > middle && _range.end < _size) || // Less stuff to move toward the end.
                _range.start == 0)
            {
                DENG2_ASSERT(_range.end < _size);
                std::memmove(_pointers + pos + 1,
                             _pointers + pos,
                             sizeof(Pointer) * (_range.end - pos));
                _range.end++;
                _pointers[pos] = ptr;
            }
            else
            {
                std::memmove(_pointers + _range.start - 1,
                             _pointers + _range.start,
                             sizeof(Pointer) * (pos - _range.start + 1));
                _range.start--;
                _pointers[pos - 1] = ptr;
            }
        }
    }
}

void PointerSet::remove(Pointer ptr)
{
    auto const loc = locate(ptr);
    if (!loc.isEmpty())
    {
        DENG2_ASSERT(!_range.isEmpty());

        // Removing the first or last item needs just a range adjustment.
        if (loc.start == _range.start)
        {
            _pointers[_range.start++] = nullptr;
        }
        else if (loc.start == _range.end - 1 &&
                 !(_flags & BeingIterated))
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

        DENG2_ASSERT(_range.start <= _range.end);
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
        _range = Rangeui16();
    }
}

PointerSet &PointerSet::operator = (PointerSet &&moved)
{
    free(_pointers);
    _pointers = moved._pointers;
    moved._pointers = nullptr;

    _flags = moved._flags;
    _size  = moved._size;
    _range = moved._range;
    return *this;
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
        Pointer const mid = at(rightHalf.start);
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
