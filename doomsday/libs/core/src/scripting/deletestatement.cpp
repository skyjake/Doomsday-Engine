/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/deletestatement.h"
#include "de/scripting/context.h"
#include "de/arrayvalue.h"
#include "de/refvalue.h"
#include "de/writer.h"
#include "de/reader.h"

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

    DE_FOR_EACH_CONST(ArrayValue::Elements, i, results.elements())
    {
        RefValue *ref = dynamic_cast<RefValue *>(*i);
        if (!ref)
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
    to << dbyte(SerialId::Delete) << *_targets;
}

void DeleteStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Delete)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("DeleteStatement::operator <<", "Invalid ID");
    }
    from >> *_targets;
}
