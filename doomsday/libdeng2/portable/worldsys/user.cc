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

#include "de/User"
#include "de/Variable"
#include "de/TextValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

User::User()
{
    /// @todo  The name is read from the configuration.
    info_.addText("name", "read-from-config");
}

User::~User()
{}

const String User::name() const
{
    return info_["name"].value().asText();
}

void User::setName(const String& name)
{
    info_["name"] = new TextValue(name);
}

void User::operator >> (Writer& to) const
{
    to << duint32(id_) << info_;
}

void User::operator << (Reader& from)
{
    duint32 newId = 0;
    from >> newId;
    if(newId != 0)
    {
        // Once assigned, the id can't be cleared.
        id_ = newId;
    }
    from >> info_;
}
