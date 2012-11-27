/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/data/byteorder.h"

using de::ByteOrder;
using de::BigEndianByteOrder;
using de::LittleEndianByteOrder;

DENG2_PUBLIC BigEndianByteOrder de::bigEndianByteOrder;
DENG2_PUBLIC LittleEndianByteOrder de::littleEndianByteOrder;

void ByteOrder::nativeToForeign(de::dint16 const &nativeValue, de::dint16 &foreignValue) const
{
    nativeToForeign(reinterpret_cast<de::duint16 const &>(nativeValue),
                    reinterpret_cast<      de::duint16 &>(foreignValue));
}

void ByteOrder::nativeToForeign(de::dint32 const &nativeValue, de::dint32 &foreignValue) const
{
    nativeToForeign(reinterpret_cast<de::duint32 const &>(nativeValue),
                    reinterpret_cast<      de::duint32 &>(foreignValue));
}

void ByteOrder::nativeToForeign(de::dint64 const &nativeValue, de::dint64 &foreignValue) const
{
    nativeToForeign(reinterpret_cast<de::duint64 const &>(nativeValue),
                    reinterpret_cast<      de::duint64 &>(foreignValue));
}

void ByteOrder::foreignToNative(de::dint16 const &foreignValue, de::dint16 &nativeValue) const
{
    foreignToNative(reinterpret_cast<de::duint16 const &>(foreignValue),
                    reinterpret_cast<      de::duint16 &>(nativeValue));
}

void ByteOrder::foreignToNative(de::dint32 const &foreignValue, de::dint32 &nativeValue) const
{
    foreignToNative(reinterpret_cast<de::duint32 const &>(foreignValue),
                    reinterpret_cast<      de::duint32 &>(nativeValue));
}

void ByteOrder::foreignToNative(de::dint64 const &foreignValue, de::dint64 &nativeValue) const
{
    foreignToNative(reinterpret_cast<de::duint64 const &>(foreignValue),
                    reinterpret_cast<      de::duint64 &>(nativeValue));
}

void ByteOrder::nativeToForeign(de::dfloat const &nativeValue, de::dfloat &foreignValue) const
{
    DENG2_ASSERT(sizeof(de::dfloat) == sizeof(de::duint32));
    nativeToForeign(reinterpret_cast<de::duint32 const &>(nativeValue),
                    reinterpret_cast<      de::duint32 &>(foreignValue));
}

void ByteOrder::nativeToForeign(de::ddouble const &nativeValue, de::ddouble &foreignValue) const
{
    DENG2_ASSERT(sizeof(de::ddouble) == sizeof(de::duint64));
    nativeToForeign(reinterpret_cast<de::duint64 const &>(nativeValue),
                    reinterpret_cast<      de::duint64 &>(foreignValue));
}

void ByteOrder::foreignToNative(de::dfloat const &foreignValue, de::dfloat &nativeValue) const
{
    DENG2_ASSERT(sizeof(de::dfloat) == sizeof(de::duint32));
    foreignToNative(reinterpret_cast<de::duint32 const &>(foreignValue),
                    reinterpret_cast<      de::duint32 &>(nativeValue));
}

void ByteOrder::foreignToNative(de::ddouble const &foreignValue, de::ddouble &nativeValue) const
{
    DENG2_ASSERT(sizeof(de::ddouble) == sizeof(de::duint64));
    foreignToNative(reinterpret_cast<de::duint64 const &>(foreignValue),
                    reinterpret_cast<      de::duint64 &>(nativeValue));
}

void BigEndianByteOrder::foreignToNative(de::duint16 const &foreignValue, de::duint16 &nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap16(foreignValue);
#endif
}

void BigEndianByteOrder::foreignToNative(de::duint32 const &foreignValue, de::duint32 &nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap32(foreignValue);
#endif
}

void BigEndianByteOrder::foreignToNative(de::duint64 const &foreignValue, de::duint64 &nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap64(foreignValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(de::duint16 const &nativeValue, de::duint16 &foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap16(nativeValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(de::duint32 const &nativeValue, de::duint32 &foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap32(nativeValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(de::duint64 const &nativeValue, de::duint64 &foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap64(nativeValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(de::duint16 const &foreignValue, de::duint16 &nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap16(foreignValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(de::duint32 const &foreignValue, de::duint32 &nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap32(foreignValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(de::duint64 const &foreignValue, de::duint64 &nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap64(foreignValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(de::duint16 const &nativeValue, de::duint16 &foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap16(nativeValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(de::duint32 const &nativeValue, de::duint32 &foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap32(nativeValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(de::duint64 const &nativeValue, de::duint64 &foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap64(nativeValue);
#endif
}

de::duint64 de::swap64(de::duint64 const &n)
{
    de::duint64 result;
    dbyte const *in = reinterpret_cast<dbyte const *>(&n);
    dbyte *out = reinterpret_cast<dbyte *>(&result);
    
    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];
    
    return result;
}
