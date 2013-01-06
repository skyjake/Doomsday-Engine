/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_BYTEORDER_H
#define LIBDENG2_BYTEORDER_H

#include "../libdeng2.h"

namespace de {

/**
 * Interface for a byte order converter.
 *
 * @ingroup data
 */
class DENG2_PUBLIC ByteOrder
{
public:
    virtual ~ByteOrder() {}

    /**
     * Converts a 16-bit unsigned integer from foreign byte order to the native one.
     *
     * @param foreignValue  Value in foreign byte order.
     * @param nativeValue   Value in the host's native byte order.
     */
    virtual void foreignToNative(duint16 const &foreignValue, duint16 &nativeValue) const = 0;

    /**
     * Converts a 32-bit unsigned integer from foreign byte order to the native one.
     *
     * @param foreignValue  Value in foreign byte order.
     * @param nativeValue   Value in the host's native byte order.
     */
    virtual void foreignToNative(duint32 const &foreignValue, duint32 &nativeValue) const = 0;

    /**
     * Converts a 64-bit unsigned integer from foreign byte order to the native one.
     *
     * @param foreignValue  Value in foreign byte order.
     * @param nativeValue   Value in the host's native byte order.
     */
    virtual void foreignToNative(duint64 const &foreignValue, duint64 &nativeValue) const = 0;

    /**
     * Converts a 16-bit unsigned integer from native byte order to the foreign one.
     *
     * @param nativeValue   Value in the host's native byte order.
     * @param foreignValue  Value in foreign byte order is written here.
     */
    virtual void nativeToForeign(duint16 const &nativeValue, duint16 &foreignValue) const = 0;

    /**
     * Converts a 32-bit unsigned integer from native byte order to the foreign one.
     *
     * @param nativeValue   Value in the host's native byte order.
     * @param foreignValue  Value in foreign byte order is written here.
     */
    virtual void nativeToForeign(duint32 const &nativeValue, duint32 &foreignValue) const = 0;

    /**
     * Converts a 64-bit unsigned integer from native byte order to the foreign one.
     *
     * @param nativeValue   Value in the host's native byte order.
     * @param foreignValue  Value in foreign byte order is written here.
     */
    virtual void nativeToForeign(duint64 const &nativeValue, duint64 &foreignValue) const = 0;

    // The signed variants.
    void nativeToForeign(dint16 const &nativeValue, dint16 &foreignValue) const;
    void nativeToForeign(dint32 const &nativeValue, dint32 &foreignValue) const;
    void nativeToForeign(dint64 const &nativeValue, dint64 &foreignValue) const;
    void foreignToNative(dint16 const &foreignValue, dint16 &nativeValue) const;
    void foreignToNative(dint32 const &foreignValue, dint32 &nativeValue) const;
    void foreignToNative(dint64 const &foreignValue, dint64 &nativeValue) const;

    // Floating point.
    void nativeToForeign(dfloat const &nativeValue, dfloat &foreignValue) const;
    void nativeToForeign(ddouble const &nativeValue, ddouble &foreignValue) const;
    void foreignToNative(dfloat const &foreignValue, dfloat &nativeValue) const;
    void foreignToNative(ddouble const &foreignValue, ddouble &nativeValue) const;

    // Convenience.
    template <typename T>
    T toForeign(T const &nativeValue) const {
        T foreignValue;
        nativeToForeign(nativeValue, foreignValue);
        return foreignValue;
    }

    template <typename T>
    T toNative(T const &foreignValue) const {
        T nativeValue;
        foreignToNative(foreignValue, nativeValue);
        return nativeValue;
    }
};

/**
 * Big-endian byte order converter.
 *
 * @ingroup data
 */
class DENG2_PUBLIC BigEndianByteOrder : public ByteOrder
{
public:
    using ByteOrder::foreignToNative;
    using ByteOrder::nativeToForeign;

    void foreignToNative(duint16 const &foreignValue, duint16 &nativeValue) const;
    void foreignToNative(duint32 const &foreignValue, duint32 &nativeValue) const;
    void foreignToNative(duint64 const &foreignValue, duint64 &nativeValue) const;
    void nativeToForeign(duint16 const &nativeValue, duint16 &foreignValue) const;
    void nativeToForeign(duint32 const &nativeValue, duint32 &foreignValue) const;
    void nativeToForeign(duint64 const &nativeValue, duint64 &foreignValue) const;
};

/// Network byte order is big endian.
typedef BigEndianByteOrder NetworkByteOrder;

/**
 * Little-endian byte order converter.
 *
 * @ingroup data
 */
class DENG2_PUBLIC LittleEndianByteOrder : public ByteOrder
{
public:
    using ByteOrder::foreignToNative;
    using ByteOrder::nativeToForeign;

    void foreignToNative(duint16 const &foreignValue, duint16 &nativeValue) const;
    void foreignToNative(duint32 const &foreignValue, duint32 &nativeValue) const;
    void foreignToNative(duint64 const &foreignValue, duint64 &nativeValue) const;
    void nativeToForeign(duint16 const &nativeValue, duint16 &foreignValue) const;
    void nativeToForeign(duint32 const &nativeValue, duint32 &foreignValue) const;
    void nativeToForeign(duint64 const &nativeValue, duint64 &foreignValue) const;
};

// Swaps the bytes of a 16-bit unsigned integer.
inline duint16 swap16(duint16 const &n) {
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

/// Swaps the bytes of a 32-bit unsigned integer.
inline duint32 swap32(duint32 const &n) {
    return ( ((n & 0xff)     << 24) | ((n & 0xff00)     << 8)
           | ((n & 0xff0000) >> 8)  | ((n & 0xff000000) >> 24));
}

/// Swaps the bytes in a 64-bit unsigned integer.
duint64 swap64(duint64 const &n);

/// Globally available big-endian byte order converter.
DENG2_PUBLIC extern BigEndianByteOrder bigEndianByteOrder;

/// Globally available little-endian byte order converter.
DENG2_PUBLIC extern LittleEndianByteOrder littleEndianByteOrder;

}

#endif /* LIBDENG2_BYTEORDER_H */
