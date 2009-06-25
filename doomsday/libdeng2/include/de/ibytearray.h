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

#ifndef LIBDENG2_IBYTEARRAY_H
#define LIBDENG2_IBYTEARRAY_H

#include <de/deng.h>
#include <de/Error>

namespace de
{
    /**
     * IByteArray is an interface for byte arrays that support random
     * access to the array elements.
     */ 
	class PUBLIC_API IByteArray
	{
	public:
		DEFINE_ERROR(OffsetError);
		
		/// Array elements are indexed using this type.
		typedef duint Offset;
		
		/// The elements of the array must be of type Byte.
		typedef dbyte Byte;
		
	public:
		virtual ~IByteArray() {}

		/// Returns the length of the array.
		virtual duint size() const = 0;
		
		/// Get elements from the array.  Raises an OffsetError if the
		/// offset is invalid.
		virtual void get(Offset at, Byte* values, duint count) const = 0;
		
		/// Set the array elements starting from location @c at to the
		/// given values.  If the offset is exactly at the end of the
		/// array, the array is grown by @c count.  An illegal offset
		/// causes an OffsetError exception.
		virtual void set(Offset at, const Byte* values, duint count) = 0;
	};
}

#endif /* LIBDENG2_IBYTEARRAY_H */
