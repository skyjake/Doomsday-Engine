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

#ifndef LIBDENG2_NONEVALUE_H
#define LIBDENG2_NONEVALUE_H

#include "../Value"

namespace de
{
    /**
     * The NoneValue class is a subclass of Value that does not contain any actual data.
     *
     * @ingroup data
     */
    class LIBDENG2_API NoneValue : public Value
    {
    public:
        NoneValue();

        Value* duplicate() const;
        Text asText() const;
        bool isTrue() const;
        dint compare(const Value& value) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
    };
}

#endif /* LIBDENG2_NONEVALUE_H */
