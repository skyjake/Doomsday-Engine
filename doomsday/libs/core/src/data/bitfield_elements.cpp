/** @file bitfield_elements.cpp  Bit field elements.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/bitfield.h"
#include "de/keymap.h"
#include "de/list.h"

namespace de {

DE_PIMPL(BitField::Elements)
{
    struct Element
    {
        int numBits;
        int firstBit;
    };
    typedef KeyMap<Id, Element> Elements; // needs to be ordered

    Elements elements;
    dsize totalBits;

    /**
     * Lookup table for quickly finding out which elements are on which bytes of
     * the packed data. Indexed using the packed data byte index; size ==
     * packed size.
     */
    List<Ids> lookup;

    Impl(Public *i)
        : Base(i)
        , totalBits(0)
    {}

    Impl(Public *i, const Impl &other)
        : Base     (i)
        , elements (other.elements)
        , totalBits(other.totalBits)
        , lookup   (other.lookup)
    {}

    const Element &element(Id id) const
    {
        DE_ASSERT(elements.contains(id));
        return elements.constFind(id)->second;
    }
};

BitField::Elements::Elements() : d(new Impl(this))
{}

BitField::Elements::Elements(const Spec *elements, dsize count)
    : d(new Impl(this))
{
    add(elements, count);
}

void BitField::Elements::clear()
{
    d->totalBits = 0;
    d->elements.clear();
    d->lookup.clear();
}

BitField::Elements &BitField::Elements::add(Id id, dsize numBits)
{
    DE_ASSERT(numBits >= 1);

    Impl::Element elem;
    elem.numBits  = numBits;
    elem.firstBit = d->totalBits;
    d->elements.insert(id, elem);
    d->totalBits += numBits;

    // Update the lookup table.
    int pos = elem.firstBit / 8;
    int endPos = (elem.firstBit + (numBits - 1)) / 8;
    while (d->lookup.sizei() <= endPos)
    {
        d->lookup.append(Ids());
    }
    for (int i = pos; i <= endPos; ++i)
    {
        d->lookup[i].insert(id);
    }

    return *this;
}

void BitField::Elements::add(const Spec *elements, dsize count)
{
    while (count-- > 0)
    {
        add(elements->id, elements->numBits);
        elements++;
    }
}

void BitField::Elements::add(const List<Spec> &elements)
{
    for (const Spec &spec : elements)
    {
        add(spec.id, spec.numBits);
    }
}

int BitField::Elements::size() const
{
    return d->elements.size();
}

BitField::Spec BitField::Elements::at(int index) const
{
    DE_ASSERT(index >= 0);
    DE_ASSERT(index < size());

    auto elem = d->elements.cbegin();
    for (int i = 0; elem != d->elements.cend(); ++i, ++elem)
    {
        if (i == index)
        {
            return Spec{elem->first, elem->second.numBits};
        }
    }
    return {};
}

void BitField::Elements::elementLayout(const Id &id, int &firstBit, int &numBits) const
{
    const Impl::Element &elem = d->element(id);
    firstBit = elem.firstBit;
    numBits  = elem.numBits;
}

int BitField::Elements::bitCount() const
{
    return d->totalBits;
}

BitField::Ids BitField::Elements::ids() const
{
    Ids ids;
    for (auto i = d->elements.cbegin(); i != d->elements.cend(); ++i)
    {
        ids.insert(i->first);
    }
    return ids;
}

BitField::Ids BitField::Elements::idsLaidOutOnByte(int index) const
{
    return d.getConst()->lookup.at(index);
}

} // namespace de
