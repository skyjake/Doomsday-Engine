/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Id"
#include "de/String"
#include "de/Writer"
#include "de/Reader"

namespace de {

// The Id generator starts from one.
Id::Type Id::_generator = 1;

Id::Id() : _id(_generator++)
{
    if(_id == NONE) 
    {
        ++_id;
    }
}

Id::Id(String const &text) : _id(NONE)
{
    if(text.beginsWith("{") && text.endsWith("}"))
    {
        _id = text.substr(1, text.size() - 2).toUInt();
    }
}

Id::~Id()
{}

Id::operator String () const
{
    return asText();
}
    
Id::operator Value::Number () const
{
    return static_cast<Value::Number>(_id);
}
    
String Id::asText() const
{
    return QString("{%1}").arg(_id);
}

ddouble Id::asNumber() const
{
    return _id;
}

QTextStream &operator << (QTextStream &os, Id const &id)
{
    os << id.asText();
    return os;
}

void Id::operator >> (Writer &to) const
{
    to << _id;
}

void Id::operator << (Reader &from)
{
    from >> _id;
}

} // namespace de
