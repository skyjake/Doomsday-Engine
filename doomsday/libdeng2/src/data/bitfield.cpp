/** @file bitfield.cpp  Array of integers packed together.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BitField"

#include <QMap>
#include <QTextStream>

namespace de {

DENG2_PIMPL(BitField)
{
    struct Element
    {
        int numBits;
        int firstBit;

        dbyte mask(int skipBits) const {
            dbyte mask = 0xff;
            if(numBits - skipBits < 8) {
                mask >>= 8 - (numBits - skipBits);
            }
            return mask;
        }

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

    Block packed;

    Instance(Public *i) : Base(i), totalBits(0)
    {}

    Instance(Public *i, Instance const &other)
        : Base     (i),
          elements (other.elements),
          totalBits(other.totalBits),
          lookup   (other.lookup),
          packed   (other.packed)
    {}

    Element &element(Id id)
    {
        DENG2_ASSERT(elements.contains(id));
        return elements[id];
    }

    void set(Id id, duint value)
    {
        Element &f      = element(id);
        int packedIdx = f.firstBit >> 3;
        int shift     = f.firstBit & 7;
        int written   = 0;

        while(written < f.numBits)
        {
            dbyte const mask = f.mask(written) << shift;

            dbyte pv = packed[packedIdx] & ~mask;
            pv |= mask & ((value >> written) << shift);
            packed[packedIdx] = pv;

            written += 8 - shift;
            shift = 0;
            packedIdx++;
        }
    }

    duint get(Id id) const
    {
        duint value = 0;

        DENG2_ASSERT(elements.contains(id));
        Element const &f = elements.constFind(id).value();

        int packedIdx = f.firstBit >> 3;
        int shift     = f.firstBit & 7;
        int read      = 0;

        while(read < f.numBits)
        {
            dbyte const mask = f.mask(read) << shift;

            value |= ((packed[packedIdx] & mask) >> shift) << read;

            read += 8 - shift;
            shift = 0;
            packedIdx++;
        }

        return value;
    }
};

BitField::BitField() : d(new Instance(this))
{}

BitField::BitField(BitField const &other) : d(new Instance(this, *other.d))
{}

BitField::BitField(Block const &data) : d(new Instance(this))
{
    d->packed = data;
}

BitField &BitField::operator = (BitField const &other)
{
    d->elements  = other.d->elements;
    d->totalBits = other.d->totalBits;
    d->lookup    = other.d->lookup;
    d->packed    = other.d->packed;
    return *this;
}

void BitField::clear()
{
    d->packed.clear();
    d->elements.clear();
    d->lookup.clear();
    d->totalBits = 0;
}

BitField &BitField::addElement(Id id, dsize numBits)
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

    // Initialize all new elements to zero.
    set(id, 0u);

    return *this;
}

void BitField::addElements(Spec const *elements, dsize count)
{
    while(count-- > 0)
    {
        addElement(elements->id, elements->numBits);
        elements++;
    }
}

void BitField::addElements(QList<Spec> const &elements)
{
    foreach(Spec spec, elements)
    {
        addElement(spec.id, spec.numBits);
    }
}

int BitField::size() const
{
    return d->elements.size();
}

BitField::Spec BitField::element(int index) const
{
    DENG2_ASSERT(index >= 0);
    DENG2_ASSERT(index < size());

    Instance::Element elem = d->elements.values()[index];
    Spec spec;
    spec.id = d->elements.keys()[index];
    spec.numBits = elem.numBits;
    return spec;
}

int BitField::bitCount() const
{
    return d->totalBits;
}

BitField::Ids BitField::elementIds() const
{
    Ids ids;
    foreach(Id id, d->elements.keys())
    {
        ids.insert(id);
    }
    return ids;
}

Block BitField::data() const
{
    return d->packed;
}

bool BitField::operator == (BitField const &other) const
{
    return d->packed == other.d->packed;
}

bool BitField::operator != (BitField const &other) const
{
    return d->packed != other.d->packed;
}

BitField::Ids BitField::delta(BitField const &other) const
{
    if(d->elements.size() != other.d->elements.size())
    {
        throw ComparisonError("BitField::delta",
                              "The compared fields have a different number of elements");
    }
    if(d->packed.size() != other.d->packed.size())
    {
        throw ComparisonError("BitField::delta",
                              "The compared fields have incompatible element sizes");
    }

    Ids diffs;
    for(duint pos = 0; pos < d->packed.size(); ++pos)
    {
        if(d->packed[pos] == other.d->packed[pos])
            continue;

        // The elements on this byte are different; which are they?
        foreach(Id id, d->lookup[pos])
        {
            if(diffs.contains(id))
                continue; // Already in the delta.

            if(asUInt(id) != other.asUInt(id))
            {
                diffs.insert(id);
            }
        }
    }
    return diffs;
}

void BitField::set(Id id, bool value)
{
    set(id, duint(value? 1 : 0));
}

void BitField::set(Id id, duint value)
{
    d->set(id, value);
}

bool BitField::asBool(Id id) const
{
    return d->get(id)? true : false;
}

duint BitField::asUInt(Id id) const
{
    return d->get(id);
}

String BitField::asText() const
{
    QString str;
    QTextStream os(&str);
    os << "BitField (" << d->totalBits << " bits, " << d->elements.size() << " elements):";
    os.setIntegerBase(2);
    for(int i = d->packed.size() - 1; i >= 0; --i)
    {
        os << " " << qSetPadChar('0') << qSetFieldWidth(8) << dbyte(d->packed[i])
           << qSetFieldWidth(0);
    }
    return str;
}

} // namespace de
