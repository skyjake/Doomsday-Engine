/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ENUMERATOR_H
#define LIBDENG2_ENUMERATOR_H

#include "../deng.h"

namespace de
{
    /**
     * Provides unique 32-bit unsigned integer numbers.
     *
     * @see Id
     *
     * @ingroup types
     */
    class LIBDENG2_API Enumerator
    {
    public:
        typedef duint32 Type;
        
        enum { 
            /// Zero is reserved as a special case.
            NONE = 0 
        };
        
    public:
        Enumerator();
        
        /**
         * Returns the next unique 32-bit unsigned integer. Never returns zero.
         */
        Type get();
        
        /**
         * Resets the enumerator so that it starts generating values starting from 1.
         */
        void reset();
        
        /**
         * Increments the enumerator so that the next generated value is greater than 
         * the claimed value @a value.
         */
        void claim(Type value);
        
        bool overflown() const { return _overflown; }
        
    private:
        Type _current;
        
        /// @c true after the 32-bit range of integers has been exhausted. The 
        /// enumerator wraps back to 1.
        bool _overflown;
    };
}

#endif /* LIBDENG2_ENUMERATOR_H */
