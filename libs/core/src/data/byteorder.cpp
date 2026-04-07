/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/byteorder.h"

namespace de {

DE_PUBLIC BigEndianByteOrder    bigEndianByteOrder;
DE_PUBLIC LittleEndianByteOrder littleEndianByteOrder;

void ByteOrder::hostToNetwork(dint16 hostValue, dint16 &networkValue) const
{
    hostToNetwork(hostValue, reinterpret_cast<duint16 &>(networkValue));
}

void ByteOrder::hostToNetwork(dint32 hostValue, dint32 &networkValue) const
{
    hostToNetwork(hostValue, reinterpret_cast<duint32 &>(networkValue));
}

void ByteOrder::hostToNetwork(dint64 hostValue, dint64 &networkValue) const
{
    hostToNetwork(hostValue, reinterpret_cast<duint64 &>(networkValue));
}

void ByteOrder::networkToHost(dint16 networkValue, dint16 &hostValue) const
{
    networkToHost(networkValue, reinterpret_cast<duint16 &>(hostValue));
}

void ByteOrder::networkToHost(dint32 networkValue, dint32 &hostValue) const
{
    networkToHost(networkValue, reinterpret_cast<duint32 &>(hostValue));
}

void ByteOrder::networkToHost(dint64 networkValue, dint64 &hostValue) const
{
    networkToHost(networkValue, reinterpret_cast<duint64 &>(hostValue));
}

void ByteOrder::hostToNetwork(dfloat hostValue, dfloat &networkValue) const
{
    DE_ASSERT(sizeof(dfloat) == sizeof(duint32));
    duint32 hv, net;
    std::memcpy(&hv, &hostValue, 4);
    hostToNetwork(hv, net);
    std::memcpy(&networkValue, &net, 4);
}

void ByteOrder::hostToNetwork(ddouble hostValue, ddouble &networkValue) const
{
    DE_ASSERT(sizeof(ddouble) == sizeof(duint64));
    duint64 hv, net;
    std::memcpy(&hv, &hostValue, 8);
    hostToNetwork(hv, net);
    std::memcpy(&networkValue, &net, 8);
}

void ByteOrder::networkToHost(dfloat networkValue, dfloat &hostValue) const
{
    DE_ASSERT(sizeof(dfloat) == sizeof(duint32));
    duint32 net, hv;
    std::memcpy(&net, &networkValue, 4);
    networkToHost(net, hv);
    std::memcpy(&hostValue, &hv, 4);
}

void ByteOrder::networkToHost(ddouble networkValue, ddouble &hostValue) const
{
    DE_ASSERT(sizeof(ddouble) == sizeof(duint64));
    duint64 net, hv;
    std::memcpy(&net, &networkValue, 8);
    networkToHost(net, hv);
    std::memcpy(&hostValue, &hv, 8);
}

void BigEndianByteOrder::networkToHost(duint16 networkValue, duint16 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void BigEndianByteOrder::networkToHost(duint32 networkValue, duint32 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void BigEndianByteOrder::networkToHost(duint64 networkValue, duint64 &hostValue) const
{
#ifdef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(duint16 hostValue, duint16 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(duint32 hostValue, duint32 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

void BigEndianByteOrder::hostToNetwork(duint64 hostValue, duint64 &networkValue) const
{
#ifdef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

void LittleEndianByteOrder::networkToHost(duint16 networkValue, duint16 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void LittleEndianByteOrder::networkToHost(duint32 networkValue, duint32 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void LittleEndianByteOrder::networkToHost(duint64 networkValue, duint64 &hostValue) const
{
#ifndef __BIG_ENDIAN__
    hostValue = networkValue;
#else
    hostValue = swapBytes(networkValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(duint16 hostValue, duint16 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(duint32 hostValue, duint32 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

void LittleEndianByteOrder::hostToNetwork(duint64 hostValue, duint64 &networkValue) const
{
#ifndef __BIG_ENDIAN__
    networkValue = hostValue;
#else
    networkValue = swapBytes(hostValue);
#endif
}

duint64 swap64(duint64 n)
{
    duint64 result;
    const dbyte *in = reinterpret_cast<const dbyte *>(&n);
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
