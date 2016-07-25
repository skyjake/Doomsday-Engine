/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/data/byteorder.h"

namespace de {

DENG2_PUBLIC BigEndianByteOrder    bigEndianByteOrder;
DENG2_PUBLIC LittleEndianByteOrder littleEndianByteOrder;

void ByteOrder::hostToNetwork(de::dint16 const &hostValue, de::dint16 &networkValue) const
{
    hostToNetwork(reinterpret_cast<de::duint16 const &>(hostValue),
                  reinterpret_cast<      de::duint16 &>(networkValue));
}

void ByteOrder::hostToNetwork(de::dint32 const &hostValue, de::dint32 &networkValue) const
{
    hostToNetwork(reinterpret_cast<de::duint32 const &>(hostValue),
                  reinterpret_cast<      de::duint32 &>(networkValue));
}

void ByteOrder::hostToNetwork(de::dint64 const &hostValue, de::dint64 &networkValue) const
{
    hostToNetwork(reinterpret_cast<de::duint64 const &>(hostValue),
                  reinterpret_cast<      de::duint64 &>(networkValue));
}

void ByteOrder::networkToHost(de::dint16 const &networkValue, de::dint16 &hostValue) const
{
    networkToHost(reinterpret_cast<de::duint16 const &>(networkValue),
                  reinterpret_cast<      de::duint16 &>(hostValue));
}

void ByteOrder::networkToHost(de::dint32 const &networkValue, de::dint32 &hostValue) const
{
    networkToHost(reinterpret_cast<de::duint32 const &>(networkValue),
                  reinterpret_cast<      de::duint32 &>(hostValue));
}

void ByteOrder::networkToHost(de::dint64 const &networkValue, de::dint64 &hostValue) const
{
    networkToHost(reinterpret_cast<de::duint64 const &>(networkValue),
                  reinterpret_cast<      de::duint64 &>(hostValue));
}

void ByteOrder::hostToNetwork(de::dfloat const &hostValue, de::dfloat &networkValue) const
{
    DENG2_ASSERT(sizeof(de::dfloat) == sizeof(de::duint32));
    hostToNetwork(reinterpret_cast<de::duint32 const &>(hostValue),
                  reinterpret_cast<      de::duint32 &>(networkValue));
}

void ByteOrder::hostToNetwork(de::ddouble const &hostValue, de::ddouble &networkValue) const
{
    DENG2_ASSERT(sizeof(de::ddouble) == sizeof(de::duint64));
    hostToNetwork(reinterpret_cast<de::duint64 const &>(hostValue),
                  reinterpret_cast<      de::duint64 &>(networkValue));
}

void ByteOrder::networkToHost(de::dfloat const &networkValue, de::dfloat &hostValue) const
{
    DENG2_ASSERT(sizeof(de::dfloat) == sizeof(de::duint32));
    networkToHost(reinterpret_cast<de::duint32 const &>(networkValue),
                  reinterpret_cast<      de::duint32 &>(hostValue));
}

void ByteOrder::networkToHost(de::ddouble const &networkValue, de::ddouble &hostValue) const
{
    DENG2_ASSERT(sizeof(de::ddouble) == sizeof(de::duint64));
    networkToHost(reinterpret_cast<de::duint64 const &>(networkValue),
                  reinterpret_cast<      de::duint64 &>(hostValue));
}

void BigEndianByteOrder::networkToHost(de::duint16 const &networkValue, de::duint16 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap16(networkValue);
#endif
}

void BigEndianByteOrder::networkToHost(de::duint32 const &networkValue, de::duint32 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap32(networkValue);
#endif
}

void BigEndianByteOrder::networkToHost(de::duint64 const &networkValue, de::duint64 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap64(networkValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(de::duint16 const &hostValue, de::duint16 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap16(hostValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(de::duint32 const &hostValue, de::duint32 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap32(hostValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(de::duint64 const &hostValue, de::duint64 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap64(hostValue);
#endif
}

void LittleEndianByteOrder::networkToHost(de::duint16 const &networkValue, de::duint16 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap16(networkValue);
#endif
}

void LittleEndianByteOrder::networkToHost(de::duint32 const &networkValue, de::duint32 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap32(networkValue);
#endif
}

void LittleEndianByteOrder::networkToHost(de::duint64 const &networkValue, de::duint64 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swap64(networkValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(de::duint16 const &hostValue, de::duint16 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap16(hostValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(de::duint32 const &hostValue, de::duint32 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap32(hostValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(de::duint64 const &hostValue, de::duint64 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swap64(hostValue);
#endif
}

de::duint64 swap64(de::duint64 const &n)
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

} // namespace de
