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

#include "de/ForStatement"
#include "de/Expression"
#include "de/NameExpression"
#include "de/Context"
#include "de/Evaluator"
#include "de/Process"
#include "de/Value"
#include "de/Variable"
#include "de/RefValue"

using namespace de;

ForStatement::~ForStatement()
{
    delete iterator_;
    delete iteration_;
}

void ForStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();
    if(!context.iterationValue())
    {
        eval.evaluate(iteration_);
        // We now have the iterated value.
        context.setIterationValue(eval.popResult());
    }

    // The variable gets ownership of this value.
    Value* nextValue = context.iterationValue()->next();
    if(nextValue)
    {
        // Assign the variable specified.
        RefValue& ref = eval.evaluateTo<RefValue>(iterator_);
        ref.assign(nextValue);
        
        // Let's begin the compound.
        context.start(compound_.firstStatement(), this, this, this);
    }
    else
    {
        context.setIterationValue(NULL);
        context.proceed();
    }            
}
