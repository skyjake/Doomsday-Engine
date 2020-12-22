/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ZEROED_H
#define LIBCORE_ZEROED_H

#include "de/libcore.h"

namespace de {

/**
 * Template for primitive types that are automatically initialized to zero.
 *
 * @ingroup data
 */
template <typename Type>
class Zeroed
{
public:
    Zeroed(const Type &v = 0) : value(v) {}
    operator const Type &() const { return value; }
    operator Type &() { return value; }
    const Type *ptr() const { return &value; }
    Type *ptr() { return &value; }
    Zeroed<Type> &operator = (const Type &v) {
        value = v;
        return *this;
    }
    Type operator -> () {
        return value;
    }
    Type const operator -> () const {
        return value;
    }

public:
    Type value;
};

using Int8   = Zeroed<dint8>;
using Int16  = Zeroed<dint16>;
using Int32  = Zeroed<dint32>;
using Int64  = Zeroed<dint64>;
using Uint8  = Zeroed<duint8>;
using Uint16 = Zeroed<duint16>;
using Uint32 = Zeroed<duint32>;
using Uint64 = Zeroed<duint64>;

} // namespace de

#endif /* LIBCORE_ZEROED_H */
