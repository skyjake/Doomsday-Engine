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

#ifndef LIBDENG2_COUNTED_H
#define LIBDENG2_COUNTED_H

#include "../libdeng2.h"

namespace de
{
    /**
     * Reference-counted object. Gets destroyed when its reference counter
     * hits zero.
     *
     * @ingroup data
     */
    class Counted
    {
    public:
        /**
         * New counted objects have a reference count of 1 -- it is assumed
         * that whoever constructs the object holds on to one reference.
         */
        Counted();

        /**
         * When a counted object is destroyed, its reference counter must be zero.
         */
        virtual ~Counted();
        
        /**
         * Acquires a reference to the reference-counted object.
         * Use the template to get the correct type of pointer from
         * a derived class.
         */
        template <typename Type>
        Type* ref() {
            ++_refCount;
            return static_cast<Type*>(this);
        }
        
        /**
         * Releases a reference that was acquired earlier with ref().
         * The object destroys itself when the reference counter hits
         * zero.
         */
        void release();
        
    private:
        /// Number of other things that refer to this object, i.e. have
        /// a pointer to it.
        duint _refCount;
    };
}

#endif /* LIBDENG2_COUNTED_H */
