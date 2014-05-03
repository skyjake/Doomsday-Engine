/** @file bitfield.cpp  Array of integers packed together.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QTextStream>

namespace de {

DENG2_PIMPL(BitField)
{
    Elements const *elements;
    Block packed;

    Instance(Public *i) : Base(i), elements(0)
    {}

    Instance(Public *i, Instance const &other)
        : Base     (i)
        , elements (other.elements)
        , packed   (other.packed)
    {}

    static dbyte elementMask(int numBits, int skipBits)
    {
        dbyte mask = 0xff;
        if(numBits - skipBits < 8)
        {
            mask >>= 8 - (numBits - skipBits);
        }
        return mask;
    }

    void set(Id id, duint value)
    {
        DENG2_ASSERT(elements != 0);

        int eFirstBit = 0;
        int eNumBits = 0;
        elements->elementLayout(id, eFirstBit, eNumBits);

        int packedIdx = eFirstBit >> 3;
        int shift     = eFirstBit & 7;
        int written   = 0;

        while(written < eNumBits)
        {
            dbyte const mask = elementMask(eNumBits, written) << shift;

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
        DENG2_ASSERT(elements != 0);

        duint value = 0;

        //DENG2_ASSERT(elements.contains(id));
        //Element const &f = elements.constFind(id).value();

        int eFirstBit = 0;
        int eNumBits = 0;
        elements->elementLayout(id, eFirstBit, eNumBits);

        int packedIdx = eFirstBit >> 3;
        int shift     = eFirstBit & 7;
        int read      = 0;

        while(read < eNumBits)
        {
            dbyte const mask = elementMask(eNumBits, read) << shift;

            value |= ((packed[packedIdx] & mask) >> shift) << read;

            read += 8 - shift;
            shift = 0;
            packedIdx++;
        }

        return value;
    }

    Ids delta(Instance const &other) const
    {
        DENG2_ASSERT(elements != 0);
        DENG2_ASSERT(other.elements != 0);

        if(elements->size() != other.elements->size())
        {
            throw ComparisonError("BitField::delta",
                                  "The compared fields have a different number of elements");
        }
        if(packed.size() != other.packed.size())
        {
            throw ComparisonError("BitField::delta",
                                  "The compared fields have incompatible element sizes");
        }

        Ids diffs;
        for(duint pos = 0; pos < packed.size(); ++pos)
        {
            if(packed[pos] == other.packed[pos])
                continue;

            // The elements on this byte are different; which are they?
            Ids const lookup = elements->idsLaidOutOnByte(pos);
            DENG2_FOR_EACH_CONST(Ids, i, lookup)
            {
                Id const &id = *i;

                if(diffs.contains(id))
                    continue; // Already in the delta.

                if(get(id) != other.get(id))
                {
                    diffs.insert(id);
                }
            }
        }
        return diffs;
    }
};

BitField::BitField() : d(new Instance(this))
{}

BitField::BitField(Elements const &elements) : d(new Instance(this))
{
    setElements(elements);
}

BitField::BitField(BitField const &other) : d(new Instance(this, *other.d))
{}

BitField::BitField(Block const &data) : d(new Instance(this))
{
    d->packed = data;
}

BitField &BitField::operator = (BitField const &other)
{
    d->elements = other.d->elements;
    d->packed   = other.d->packed;
    return *this;
}

void BitField::setElements(Elements const &elements)
{
    clear();

    d->elements = &elements;

    // Initialize all new elements to zero.
    for(int i = 0; i < elements.size(); ++i)
    {
        set(elements.at(i).id, 0u);
    }
}

void BitField::clear()
{
    d->packed.clear();
    d->elements = 0;
}

bool BitField::isEmpty() const
{
    return !d->elements || !d->elements->size();
}

BitField::Elements const &BitField::elements() const
{
    return *d->elements;
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
    return d->delta(*other.d);
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
    os << "BitField (" << d->elements->bitCount() << " bits, " << d->elements->size() << " elements):";
    os.setIntegerBase(2);
    for(int i = d->packed.size() - 1; i >= 0; --i)
    {
        os << " " << qSetPadChar('0') << qSetFieldWidth(8) << dbyte(d->packed[i])
           << qSetFieldWidth(0);
    }
    return str;
}

} // namespace de
