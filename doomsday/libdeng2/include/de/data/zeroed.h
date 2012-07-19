/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ZEROED_H
#define LIBDENG2_ZEROED_H

#include "../libdeng2.h"

namespace de
{
    /**
     * Template for primitive types that are automatically initialized to zero.
     */
    template <typename Type>
    class Zeroed
    {
    public:
        Zeroed(const Type& v = 0) : value(v) {}
        operator const Type& () const { return value; }
        operator Type& () { return value; }
        Zeroed<Type>& operator = (const Type& v) { 
            value = v; 
            return *this; 
        }

    public:
        Type value;
    };
    
    typedef Zeroed<dint8> Int8;
    typedef Zeroed<dint16> Int16;
    typedef Zeroed<dint32> Int32;
    typedef Zeroed<dint64> Int64;
    typedef Zeroed<duint8> Uint8;
    typedef Zeroed<duint16> Uint16;
    typedef Zeroed<duint32> Uint32;
    typedef Zeroed<duint64> Uint64;
}

#endif /* LIBDENG2_ZEROED_H */
