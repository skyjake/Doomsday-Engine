/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/scripting/compound.h"
#include "de/scripting/statement.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

Compound::Compound()
{}

Compound::~Compound()
{
    clear();
}

void Compound::clear()
{
    for (Statements::iterator i = _statements.begin(); i != _statements.end(); ++i)
    {
        delete *i;
    }
    _statements.clear();
}

const Statement *Compound::firstStatement() const
{
    if (_statements.empty())
    {
        return NULL;
    }
    return *_statements.begin();
}

void Compound::add(Statement *statement, duint lineNumber)
{
    statement->setLineNumber(lineNumber);
    if (_statements.size() > 0)
    {
        _statements.back()->setNext(statement);
    }
    _statements.push_back(statement);
}

void Compound::operator >> (Writer &to) const
{
    to << duint32(_statements.size());
    for (Statements::const_iterator i = _statements.begin(); i != _statements.end(); ++i)
    {
        to << **i;
    }
}

void Compound::operator << (Reader &from)
{
    duint32 count;
    from >> count;
    clear();
    while (count--)
    {
        add(Statement::constructFrom(from), 0 /* unknown line number */);
    }
}
