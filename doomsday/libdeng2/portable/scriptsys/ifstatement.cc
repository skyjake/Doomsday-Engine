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

#include "de/IfStatement"
#include "de/Context"
#include "de/Expression"
#include "de/Value"

using namespace de;

IfStatement::~IfStatement()
{
    for(Branches::iterator i = branches_.begin(); i != branches_.end(); ++i)
    {
        delete i->condition;
        delete i->compound;
    }
}

void IfStatement::newBranch()
{
    branches_.push_back(Branch(new Compound()));
}

void IfStatement::setBranchCondition(Expression* condition)
{
    branches_.back().condition = condition;
}

Compound& IfStatement::branchCompound()
{
    return *branches_.back().compound;
}

void IfStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();

    for(Branches::const_iterator i = branches_.begin(); i != branches_.end(); ++i)
    {
        if(eval.evaluate(i->condition).isTrue())
        {
            context.start(i->compound->firstStatement(), next());
            return;
        }
    }
    if(elseCompound_.size())
    {
        context.start(elseCompound_.firstStatement(), next());
    }
    else
    {
        context.proceed();
    }
}
