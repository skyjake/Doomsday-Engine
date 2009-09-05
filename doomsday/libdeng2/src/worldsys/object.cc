/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#include "de/Object"
#include "de/Writer"
#include "de/Reader"

using namespace de;

Object::Object()
{}

Object::~Object()
{}

Thinker* Object::fromReader(Reader& reader)
{
    SerialId sid;
    reader >> sid;
    if(sid != OBJECT)
    {
        return 0;
    }
    std::auto_ptr<Object> ob(new Object);
    reader >> *ob.get();
    return ob.release();
}

void Object::operator >> (Writer& to) const
{
    Thinker::operator >> (to);
    to << _pos;
}

void Object::operator << (Reader& from)
{
    Thinker::operator << (from);
    from >> _pos;
}
