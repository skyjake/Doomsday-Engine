/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DeleteStatement"
#include "de/Context"
#include "de/ArrayValue"
#include "de/RefValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

DeleteStatement::DeleteStatement() : _targets(new ArrayExpression)
{}

DeleteStatement::DeleteStatement(ArrayExpression *targets) : _targets(targets)
{}

DeleteStatement::~DeleteStatement()
{
    delete _targets;
}

void DeleteStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();
    ArrayValue &results = eval.evaluateTo<ArrayValue>(_targets);

    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, results.elements())
    {
        RefValue *ref = dynamic_cast<RefValue *>(*i);
        if(!ref)
        {
            throw LeftValueError("DeleteStatement::execute",
                                 "Cannot delete l-value '" + (*i)->asText() + "'");
        }

        // Possible owning record will be notified via deletion audience.
        delete ref->variable();
    }

    context.proceed();
}

void DeleteStatement::operator >> (Writer &to) const
{
    to << SerialId(DELETE) << *_targets;
}

void DeleteStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != DELETE)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("DeleteStatement::operator <<", "Invalid ID");
    }
    from >> *_targets;
}
