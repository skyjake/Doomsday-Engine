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

#include "de/Evaluator"
#include "de/Expression"
#include "de/Value"
#include "de/Context"
#include "de/Process"

using namespace de;

Evaluator::Evaluator(Context& owner) : context_(owner), current_(0), names_(0)
{}

Evaluator::~Evaluator()
{
    clearNames();
    clearResults();
}

Process& Evaluator::process()
{ 
    return context_.process(); 
}

void Evaluator::reset()
{
    current_ = NULL;
    
    clearStack();
    clearNames();
}

Value& Evaluator::evaluate(const Expression* expression)
{
    assert(names_ == NULL);
    assert(stack_.empty());
        
    // Begin a new evaluation operation.
    current_ = expression;
    expression->push(*this);

    // Clear the result stack.
    clearResults();

    while(!stack_.empty())
    {
        // Continue by processing the next step in the evaluation.
        ScopedExpression top = stack_.back();
        stack_.pop_back();
        clearNames();
        names_ = top.names;
        pushResult(top.expression->evaluate(*this));
    }

    // During function call evaluation the process's context changes. We should
    // now be back at the level we started from.
    assert(&process().context() == &context_);

    // Exactly one value should remain in the result stack: the result of the
    // evaluated expression.
    assert(hasResult());

    clearNames();
    current_ = NULL;
    return result();
}

void Evaluator::namespaces(Namespaces& spaces)
{
    if(names_)
    {
        // A specific namespace has been defined.
        spaces.clear();
        spaces.push_back(names_);
    }
    else
    {
        // Collect namespaces from the process's call stack.
        process().namespaces(spaces);
    }
}

bool Evaluator::hasResult() const
{
    return results_.size() == 1;
}

Value& Evaluator::result()
{
    assert(!results_.empty());
    return *results_.front();
}

void Evaluator::push(const Expression* expression, Record* names)
{
    stack_.push_back(ScopedExpression(expression, names));
}

void Evaluator::pushResult(Value* value)
{
    // NULLs are not pushed onto the results stack as they indicate that
    // no result was given.
    if(value)
    {
        results_.push_back(value);
    }
}

Value* Evaluator::popResult()
{
    assert(results_.size() > 0);

    Value* result = results_.back();
    results_.pop_back();
    return result;
}

void Evaluator::clearNames()
{
    if(names_)
    {
        names_ = 0;
    }
}

void Evaluator::clearResults()
{
    //LOG_TRACE("Evaluator::clearResults");
    
    for(Results::iterator i = results_.begin(); i != results_.end(); ++i)
    {
        delete *i;
    }
    results_.clear();
}

void Evaluator::clearStack()
{
    while(!stack_.empty())
    {
        ScopedExpression top = stack_.back();
        stack_.pop_back();
        clearNames();
        names_ = top.names;
    }
}
