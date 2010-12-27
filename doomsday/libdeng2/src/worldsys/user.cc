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

#include "de/User"
#include "de/Variable"
#include "de/TextValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

User::User()
{
    _info.addText("name", "");
}

User::~User()
{}

const String User::name() const
{
    return _info["name"].value().asText();
}

void User::setName(const String& name)
{
    _info["name"] = new TextValue(name);
}

const Variable& User::info(const String& infoMember) const
{
    return _info[infoMember];
}

Variable& User::info(const String& infoMember)
{
    return _info[infoMember];
}

void User::operator >> (Writer& to) const
{
    to << duint32(_id) << _info;
}

void User::operator << (Reader& from)
{
    duint32 newId = 0;
    from >> newId;
    if(newId != 0)
    {
        // Once assigned, the id can't be cleared.
        _id = newId;
    }
    from >> _info;
}
