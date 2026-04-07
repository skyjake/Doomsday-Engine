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

#ifndef LIBCORE_BYTEORDER_H
#define LIBCORE_BYTEORDER_H

#include "de/libcore.h"

namespace de {

/**
 * Interface for a byte order converter.
 *
 * @ingroup data
 */
class DE_PUBLIC ByteOrder
{
public:
    virtual ~ByteOrder() {}

    /**
     * Converts a 16-bit unsigned integer from network byte order to the host order.
     *
     * @param networkValue  Value in network byte order.
     * @param hostValue     Value in the host's host byte order.
     */
    virtual void networkToHost(duint16 networkValue, duint16 &hostValue) const = 0;

    /**
     * Converts a 32-bit unsigned integer from network byte order to the host order.
     *
     * @param networkValue  Value in network byte order.
     * @param hostValue     Value in the host's host byte order.
     */
    virtual void networkToHost(duint32 networkValue, duint32 &hostValue) const = 0;

    /**
     * Converts a 64-bit unsigned integer from network byte order to the host order.
     *
     * @param networkValue  Value in network byte order.
     * @param hostValue     Value in the host's host byte order.
     */
    virtual void networkToHost(duint64 networkValue, duint64 &hostValue) const = 0;

    /**
     * Converts a 16-bit unsigned integer from host byte order to the network order.
     *
     * @param hostValue     Value in the host's host byte order.
     * @param networkValue  Value in network byte order is written here.
     */
    virtual void hostToNetwork(duint16 hostValue, duint16 &networkValue) const = 0;

    /**
     * Converts a 32-bit unsigned integer from host byte order to the network order.
     *
     * @param hostValue     Value in the host's host byte order.
     * @param networkValue  Value in network byte order is written here.
     */
    virtual void hostToNetwork(duint32 hostValue, duint32 &networkValue) const = 0;

    /**
     * Converts a 64-bit unsigned integer from host byte order to the network order.
     *
     * @param hostValue     Value in the host's host byte order.
     * @param networkValue  Value in network byte order is written here.
     */
    virtual void hostToNetwork(duint64 hostValue, duint64 &networkValue) const = 0;

    // The signed variants.
    void hostToNetwork(dint16 hostValue, dint16 &networkValue) const;
    void hostToNetwork(dint32 hostValue, dint32 &networkValue) const;
    void hostToNetwork(dint64 hostValue, dint64 &networkValue) const;
    void networkToHost(dint16 networkValue, dint16 &hostValue) const;
    void networkToHost(dint32 networkValue, dint32 &hostValue) const;
    void networkToHost(dint64 networkValue, dint64 &hostValue) const;

    // Floating point.
    void hostToNetwork(dfloat hostValue, dfloat &networkValue) const;
    void hostToNetwork(ddouble hostValue, ddouble &networkValue) const;
    void networkToHost(dfloat networkValue, dfloat &hostValue) const;
    void networkToHost(ddouble networkValue, ddouble &hostValue) const;

    // Convenience.
    template <typename T>
    T toNetwork(T hostValue) const {
        T networkValue;
        hostToNetwork(hostValue, networkValue);
        return networkValue;
    }

    template <typename T>
    T toHost(T networkValue) const {
        T hostValue;
        networkToHost(networkValue, hostValue);
        return hostValue;
    }
};

/**
 * Big-endian byte order converter.
 *
 * @ingroup data
 */
class DE_PUBLIC BigEndianByteOrder : public ByteOrder
{
public:
    using ByteOrder::networkToHost;
    using ByteOrder::hostToNetwork;

    void networkToHost(duint16 networkValue, duint16 &hostValue) const;
    void networkToHost(duint32 networkValue, duint32 &hostValue) const;
    void networkToHost(duint64 networkValue, duint64 &hostValue) const;
    void hostToNetwork(duint16 hostValue, duint16 &networkValue) const;
    void hostToNetwork(duint32 hostValue, duint32 &networkValue) const;
    void hostToNetwork(duint64 hostValue, duint64 &networkValue) const;
};

/// Network byte order is big endian.
typedef BigEndianByteOrder NetworkByteOrder;

/**
 * Little-endian byte order converter.
 *
 * @ingroup data
 */
class DE_PUBLIC LittleEndianByteOrder : public ByteOrder
{
public:
    using ByteOrder::networkToHost;
    using ByteOrder::hostToNetwork;

    void networkToHost(duint16 networkValue, duint16 &hostValue) const;
    void networkToHost(duint32 networkValue, duint32 &hostValue) const;
    void networkToHost(duint64 networkValue, duint64 &hostValue) const;
    void hostToNetwork(duint16 hostValue, duint16 &networkValue) const;
    void hostToNetwork(duint32 hostValue, duint32 &networkValue) const;
    void hostToNetwork(duint64 hostValue, duint64 &networkValue) const;
};

// Swaps the bytes of a 16-bit unsigned integer.
inline duint16 swapBytes(duint16 n) {
    return duint16((n & 0xff) << 8) | duint16((n & 0xff00) >> 8);
}

inline dint16 swapBytes(dint16 n) {
    return dint16(swapBytes(duint16(n)));
}

/// Swaps the bytes of a 32-bit unsigned integer.
inline duint32 swapBytes(duint32 n) {
    return ( ((n & 0xff)     << 24) | ((n & 0xff00)     << 8)
           | ((n & 0xff0000) >> 8)  | ((n & 0xff000000) >> 24));
}

inline dint32 swapBytes(dint32 n) {
    return dint32(swapBytes(duint32(n)));
}

/// Swaps the bytes in a 64-bit unsigned integer.
DE_PUBLIC duint64 swap64(duint64 n);

inline dint64 swapBytes(dint64 n) {
    return swap64(n);
}

inline duint64 swapBytes(duint64 n) {
    return swap64(n);
}

template <typename T>
inline T fromLittleEndian(T n) {
#ifdef __BIG_ENDIAN__
    return swapBytes(n);
#else
    return n;
#endif
}

template <typename T>
inline T fromBigEndian(T n) {
#ifdef __BIG_ENDIAN__
    return n;
#else
    return swapBytes(n);
#endif
}

/// Globally available big-endian byte order converter.
DE_PUBLIC extern BigEndianByteOrder bigEndianByteOrder;

/// Globally available little-endian byte order converter.
DE_PUBLIC extern LittleEndianByteOrder littleEndianByteOrder;

}

#endif /* LIBCORE_BYTEORDER_H */
