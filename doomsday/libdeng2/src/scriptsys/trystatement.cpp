/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/TryStatement"
#include "de/Context"
#include "de/Writer"
#include "de/Reader"

using namespace de;

void TryStatement::execute(Context &context) const
{
    context.start(_compound.firstStatement(), next());
}

void TryStatement::operator >> (Writer &to) const
{
    to << SerialId(TRY) << _compound;
}

void TryStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != TRY)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("TryStatement::operator <<", "Invalid ID");
    }
    from >> _compound;
}
