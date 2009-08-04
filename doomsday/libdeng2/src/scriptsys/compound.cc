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

#include "de/Compound"
#include "de/Statement"
#include "de/Writer"
#include "de/Reader"

using namespace de;

Compound::Compound()
{}

Compound::~Compound()
{
    clear();
}

void Compound::clear()
{
    for(Statements::iterator i = statements_.begin(); i != statements_.end(); ++i)
    {
        delete *i;
    }
    statements_.clear();
}

const Statement* Compound::firstStatement() const
{
    if(statements_.empty())
    {
        return NULL;
    }
    return *statements_.begin();
}

void Compound::add(Statement* statement)
{
    if(statements_.size() > 0)
    {
        statements_.back()->setNext(statement);
    }
    statements_.push_back(statement);
}

void Compound::operator >> (Writer& to) const
{
    to << duint32(statements_.size());
    for(Statements::const_iterator i = statements_.begin(); i != statements_.end(); ++i)
    {
        to << **i;
    }
}

void Compound::operator << (Reader& from)
{
    duint32 count;
    from >> count;
    clear();
    while(count--)
    {
        statements_.push_back(Statement::constructFrom(from));
    }
}
