/** @file bitfield.cpp  Array of integers packed together.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <sstream>

namespace de {

DE_PIMPL(BitField)
{
    const Elements *elements;
    Block packed;

    Impl(Public *i) : Base(i), elements(0)
    {}

    Impl(Public *i, const Impl &other)
        : Base     (i)
        , elements (other.elements)
        , packed   (other.packed)
    {}

    static dbyte elementMask(int numBits, int skipBits)
    {
        dbyte mask = 0xff;
        if (numBits - skipBits < 8)
        {
            mask >>= 8 - (numBits - skipBits);
        }
        return mask;
    }

    void set(Id id, duint value)
    {
        DE_ASSERT(elements != 0);

        int eFirstBit = 0;
        int eNumBits = 0;
        elements->elementLayout(id, eFirstBit, eNumBits);

        int packedIdx = eFirstBit >> 3;
        int shift     = eFirstBit & 7;
        int written   = 0;

        while (written < eNumBits)
        {
            const dbyte mask = elementMask(eNumBits, written) << shift;

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
        DE_ASSERT(elements != 0);

        duint value = 0;

        //DE_ASSERT(elements.contains(id));
        //const Element &f = elements.constFind(id).value();

        int eFirstBit = 0;
        int eNumBits = 0;
        elements->elementLayout(id, eFirstBit, eNumBits);

        int packedIdx = eFirstBit >> 3;
        int shift     = eFirstBit & 7;
        int read      = 0;

        while (read < eNumBits)
        {
            const dbyte mask = elementMask(eNumBits, read) << shift;

            value |= ((packed[packedIdx] & mask) >> shift) << read;

            read += 8 - shift;
            shift = 0;
            packedIdx++;
        }

        return value;
    }

    Ids delta(const Impl &other) const
    {
        DE_ASSERT(elements != 0);
        DE_ASSERT(other.elements != 0);

        if (elements->size() != other.elements->size())
        {
            throw ComparisonError("BitField::delta",
                                  "The compared fields have a different number of elements");
        }
        if (packed.size() != other.packed.size())
        {
            throw ComparisonError("BitField::delta",
                                  "The compared fields have incompatible element sizes");
        }

        Ids diffs;
        for (duint pos = 0; pos < packed.size(); ++pos)
        {
            if (packed[pos] == other.packed[pos])
                continue;

            // The elements on this byte are different; which are they?
            const Ids lookup = elements->idsLaidOutOnByte(pos);
            DE_FOR_EACH_CONST(Ids, i, lookup)
            {
                const Id &id = *i;

                if (diffs.contains(id))
                    continue; // Already in the delta.

                if (get(id) != other.get(id))
                {
                    diffs.insert(id);
                }
            }
        }
        return diffs;
    }
};

BitField::BitField() : d(new Impl(this))
{}

BitField::BitField(const Elements &elements) : d(new Impl(this))
{
    setElements(elements);
}

BitField::BitField(const BitField &other) : d(new Impl(this, *other.d))
{}

BitField::BitField(const Block &data) : d(new Impl(this))
{
    d->packed = data;
}

BitField &BitField::operator = (const BitField &other)
{
    d->elements = other.d->elements;
    d->packed   = other.d->packed;
    return *this;
}

void BitField::setElements(const Elements &elements)
{
    clear();

    d->elements = &elements;

    // Initialize all new elements to zero.
    for (int i = 0; i < elements.size(); ++i)
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

const BitField::Elements &BitField::elements() const
{
    return *d->elements;
}

Block BitField::data() const
{
    return d->packed;
}

bool BitField::operator == (const BitField &other) const
{
    return d->packed == other.d->packed;
}

bool BitField::operator != (const BitField &other) const
{
    return d->packed != other.d->packed;
}

BitField::Ids BitField::delta(const BitField &other) const
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
    std::ostringstream os;
    os << "BitField (" << d->elements->bitCount() << " bits, " << d->elements->size() << " elements):";
    for (int i = int(d->packed.size()) - 1; i >= 0; --i)
    {
        os << stringf(" %02x", d->packed[i]);
    }
    return os.str();
}

} // namespace de
