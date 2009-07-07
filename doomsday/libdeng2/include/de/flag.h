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

#ifndef LIBDENG2_FLAG_H
#define LIBDENG2_FLAG_H

#include <bitset>

namespace de
{
    /**
     * Up to 32 flags can be accessed directly with a bitmask (the @c Flag type).
     * The rest can be accessed using the bit position (@c FLAGNAME_BIT).
     */
    typedef duint Flag;
}

#define DEFINE_FLAG(Name, NBit) \
    static const duint Name##_BIT = NBit; \
    static const Flag Name = 1L << Name##_BIT; \

#define DEFINE_FINAL_FLAG(Name, NBit, FlagSetName) \
    DEFINE_FLAG(Name, NBit) \
    static const duint FlagSetName##_NUM_FLAGS = Name##_BIT + 1; \
    typedef std::bitset<FlagSetName##_NUM_FLAGS> FlagSetName;

#endif /* LIBDENG2_FLAG_H */
