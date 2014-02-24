/** @file bitfield_elements.cpp  Bit field elements.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BitField"

#include <QMap>
#include <QList>

namespace de {

DENG2_PIMPL(BitField::Elements)
{
    struct Element
    {
        int numBits;
        int firstBit;
    };
    typedef QMap<Id, Element> Elements;

    Elements elements;
    dsize totalBits;

    /**
     * Lookup table for quickly finding out which elements are on which bytes of
     * the packed data. Indexed using the packed data byte index; size ==
     * packed size.
     */
    QList<Ids> lookup;

    Instance(Public *i)
        : Base(i)
        , totalBits(0)
    {}

    Instance(Public *i, Instance const &other)
        : Base     (i)
        , elements (other.elements)
        , totalBits(other.totalBits)
        , lookup   (other.lookup)
    {}

    Element const &element(Id id) const
    {
        DENG2_ASSERT(elements.contains(id));
        return elements.constFind(id).value();
    }
};

BitField::Elements::Elements() : d(new Instance(this))
{}

BitField::Elements::Elements(Spec const *elements, dsize count)
    : d(new Instance(this))
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
    DENG2_ASSERT(numBits >= 1);

    Instance::Element elem;
    elem.numBits  = numBits;
    elem.firstBit = d->totalBits;
    d->elements.insert(id, elem);
    d->totalBits += numBits;

    // Update the lookup table.
    int pos = elem.firstBit / 8;
    int endPos = (elem.firstBit + (numBits - 1)) / 8;
    while(d->lookup.size() <= endPos)
    {
        d->lookup.append(Ids());
    }
    for(int i = pos; i <= endPos; ++i)
    {
        d->lookup[i].insert(id);
    }

    return *this;
}

void BitField::Elements::add(Spec const *elements, dsize count)
{
    while(count-- > 0)
    {
        add(elements->id, elements->numBits);
        elements++;
    }
}

void BitField::Elements::add(QList<Spec> const &elements)
{
    foreach(Spec spec, elements)
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
    DENG2_ASSERT(index >= 0);
    DENG2_ASSERT(index < size());

    Instance::Element elem = d->elements.values()[index];
    Spec spec;
    spec.id = d->elements.keys()[index];
    spec.numBits = elem.numBits;
    return spec;
}

void BitField::Elements::elementLayout(Id const &id, int &firstBit, int &numBits) const
{
    Instance::Element const &elem = d->element(id);
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
    foreach(Id id, d->elements.keys())
    {
        ids.insert(id);
    }
    return ids;
}

BitField::Ids BitField::Elements::idsLaidOutOnByte(int index) const
{
    return d->lookup.at(index);
}

} // namespace de
