/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#include "de/WhileStatement"
#include "de/Expression"
#include "de/Evaluator"
#include "de/Context"
#include "de/Value"
#include "de/Writer"
#include "de/Reader"

using namespace de;

WhileStatement::~WhileStatement()
{
    delete _loopCondition;
}

void WhileStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    if(eval.evaluate(_loopCondition).isTrue())
    {
        // Continue and break jump points are defined within a while compound.
        context.start(_compound.firstStatement(), this, this, this);
    }
    else
    {
        context.proceed();
    }
}

void WhileStatement::operator >> (Writer &to) const
{
    to << SerialId(WHILE) << *_loopCondition << _compound;
}

void WhileStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != WHILE)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("WhileStatement::operator <<", "Invalid ID");
    }
    delete _loopCondition;
    _loopCondition = 0;
    _loopCondition = Expression::constructFrom(from);
    
    from >> _compound;
}
