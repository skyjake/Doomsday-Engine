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

#include "de/scripting/ifstatement.h"
#include "de/scripting/context.h"
#include "de/scripting/expression.h"
#include "de/value.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

IfStatement::~IfStatement()
{
    clear();
}

void IfStatement::clear()
{
    for (Branches::iterator i = _branches.begin(); i != _branches.end(); ++i)
    {
        delete i->condition;
        delete i->compound;
    }
    _branches.clear();
}

void IfStatement::newBranch()
{
    _branches.push_back(Branch(new Compound()));
}

void IfStatement::setBranchCondition(Expression *condition)
{
    _branches.back().condition = condition;
}

Compound &IfStatement::branchCompound()
{
    return *_branches.back().compound;
}

void IfStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    for (Branches::const_iterator i = _branches.begin(); i != _branches.end(); ++i)
    {
        if (eval.evaluate(i->condition).isTrue())
        {
            context.start(i->compound->firstStatement(), next());
            return;
        }
    }
    if (_elseCompound.size())
    {
        context.start(_elseCompound.firstStatement(), next());
    }
    else
    {
        context.proceed();
    }
}

void IfStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::If);

    // Branches.
    to << duint16(_branches.size());
    for (Branches::const_iterator i = _branches.begin(); i != _branches.end(); ++i)
    {
        DE_ASSERT(i->condition != NULL);
        to << *i->condition << *i->compound;
    }

    to << _elseCompound;
}

void IfStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::If)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("IfStatement::operator <<", "Invalid ID");
    }
    clear();

    // Branches.
    duint16 count;
    from >> count;
    while (count--)
    {
        newBranch();
        setBranchCondition(Expression::constructFrom(from));
        from >> branchCompound();
    }

    from >> _elseCompound;
}
