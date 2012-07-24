/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

DENG2_PUBLIC BigEndianByteOrder de::bigEndianByteOrder;
DENG2_PUBLIC LittleEndianByteOrder de::littleEndianByteOrder;

void ByteOrder::nativeToForeign(const dint16& nativeValue, dint16& foreignValue) const
{
    nativeToForeign(reinterpret_cast<const duint16&>(nativeValue),
                    reinterpret_cast<      duint16&>(foreignValue));
}

void ByteOrder::nativeToForeign(const dint32& nativeValue, dint32& foreignValue) const
{
    nativeToForeign(reinterpret_cast<const duint32&>(nativeValue),
                    reinterpret_cast<      duint32&>(foreignValue));
}

void ByteOrder::nativeToForeign(const dint64& nativeValue, dint64& foreignValue) const
{
    nativeToForeign(reinterpret_cast<const duint64&>(nativeValue),
                    reinterpret_cast<      duint64&>(foreignValue));
}

void ByteOrder::foreignToNative(const dint16& foreignValue, dint16& nativeValue) const
{
    foreignToNative(reinterpret_cast<const duint16&>(foreignValue),
                    reinterpret_cast<      duint16&>(nativeValue));
}

void ByteOrder::foreignToNative(const dint32& foreignValue, dint32& nativeValue) const
{
    foreignToNative(reinterpret_cast<const duint32&>(foreignValue),
                    reinterpret_cast<      duint32&>(nativeValue));
}

void ByteOrder::foreignToNative(const dint64& foreignValue, dint64& nativeValue) const
{
    foreignToNative(reinterpret_cast<const duint64&>(foreignValue),
                    reinterpret_cast<      duint64&>(nativeValue));
}

void ByteOrder::nativeToForeign(const dfloat& nativeValue, dfloat& foreignValue) const
{
    DENG2_ASSERT(sizeof(dfloat) == sizeof(duint32));
    nativeToForeign(reinterpret_cast<const duint32&>(nativeValue),
                    reinterpret_cast<      duint32&>(foreignValue));
}

void ByteOrder::nativeToForeign(const ddouble& nativeValue, ddouble& foreignValue) const
{
    DENG2_ASSERT(sizeof(ddouble) == sizeof(duint64));
    nativeToForeign(reinterpret_cast<const duint64&>(nativeValue),
                    reinterpret_cast<      duint64&>(foreignValue));
}

void ByteOrder::foreignToNative(const dfloat& foreignValue, dfloat& nativeValue) const
{
    DENG2_ASSERT(sizeof(dfloat) == sizeof(duint32));
    foreignToNative(reinterpret_cast<const duint32&>(foreignValue),
                    reinterpret_cast<      duint32&>(nativeValue));
}

void ByteOrder::foreignToNative(const ddouble& foreignValue, ddouble& nativeValue) const
{
    DENG2_ASSERT(sizeof(ddouble) == sizeof(duint64));
    foreignToNative(reinterpret_cast<const duint64&>(foreignValue),
                    reinterpret_cast<      duint64&>(nativeValue));
}

void BigEndianByteOrder::foreignToNative(const duint16& foreignValue, duint16& nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap16(foreignValue);
#endif
}

void BigEndianByteOrder::foreignToNative(const duint32& foreignValue, duint32& nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap32(foreignValue);
#endif
}

void BigEndianByteOrder::foreignToNative(const duint64& foreignValue, duint64& nativeValue) const
{
#ifdef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap64(foreignValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(const duint16& nativeValue, duint16& foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap16(nativeValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(const duint32& nativeValue, duint32& foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap32(nativeValue);
#endif
}

void BigEndianByteOrder::nativeToForeign(const duint64& nativeValue, duint64& foreignValue) const
{
#ifdef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap64(nativeValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(const duint16& foreignValue, duint16& nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap16(foreignValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(const duint32& foreignValue, duint32& nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap32(foreignValue);
#endif
}

void LittleEndianByteOrder::foreignToNative(const duint64& foreignValue, duint64& nativeValue) const
{
#ifndef __BIG_ENDIAN__ 
    nativeValue = foreignValue;
#else
    nativeValue = swap64(foreignValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(const duint16& nativeValue, duint16& foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap16(nativeValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(const duint32& nativeValue, duint32& foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap32(nativeValue);
#endif
}

void LittleEndianByteOrder::nativeToForeign(const duint64& nativeValue, duint64& foreignValue) const
{
#ifndef __BIG_ENDIAN__ 
    foreignValue = nativeValue;
#else
    foreignValue = swap64(nativeValue);
#endif
}

duint64 de::swap64(const duint64& n)
{
    duint64 result;
    const dbyte* in = reinterpret_cast<const dbyte*>(&n);
    dbyte* out = reinterpret_cast<dbyte*>(&result);
    
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
