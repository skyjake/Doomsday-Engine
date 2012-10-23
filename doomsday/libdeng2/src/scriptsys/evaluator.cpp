/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Evaluator::Evaluator(Context& owner) : _context(owner), _current(0), _names(0)
{}

Evaluator::~Evaluator()
{
    clearNames();
    clearResults();
}

Process& Evaluator::process()
{ 
    return _context.process(); 
}

void Evaluator::reset()
{
    _current = NULL;
    
    clearStack();
    clearNames();
}

Value& Evaluator::evaluate(const Expression* expression)
{
    DENG2_ASSERT(_names == NULL);
    DENG2_ASSERT(_stack.empty());
        
    // Begin a new evaluation operation.
    _current = expression;
    expression->push(*this);

    // Clear the result stack.
    clearResults();

    while(!_stack.empty())
    {
        // Continue by processing the next step in the evaluation.
        ScopedExpression top = _stack.back();
        _stack.pop_back();
        clearNames();
        _names = top.names;
        pushResult(top.expression->evaluate(*this));
    }

    // During function call evaluation the process's context changes. We should
    // now be back at the level we started from.
    DENG2_ASSERT(&process().context() == &_context);

    // Exactly one value should remain in the result stack: the result of the
    // evaluated expression.
    DENG2_ASSERT(hasResult());

    clearNames();
    _current = NULL;
    return result();
}

void Evaluator::namespaces(Namespaces& spaces)
{
    if(_names)
    {
        // A specific namespace has been defined.
        spaces.clear();
        spaces.push_back(_names);
    }
    else
    {
        // Collect namespaces from the process's call stack.
        process().namespaces(spaces);
    }
}

bool Evaluator::hasResult() const
{
    return _results.size() == 1;
}

Value& Evaluator::result()
{
    if(_results.empty())
    {
        return _noResult;
    }
    return *_results.front();
}

void Evaluator::push(const Expression* expression, Record* names)
{
    _stack.push_back(ScopedExpression(expression, names));
}

void Evaluator::pushResult(Value* value)
{
    // NULLs are not pushed onto the results stack as they indicate that
    // no result was given.
    if(value)
    {
        _results.push_back(value);
    }
}

Value* Evaluator::popResult()
{
    DENG2_ASSERT(_results.size() > 0);

    Value* result = _results.back();
    _results.pop_back();
    return result;
}

void Evaluator::clearNames()
{
    if(_names)
    {
        _names = 0;
    }
}

void Evaluator::clearResults()
{
    //LOG_TRACE("Evaluator::clearResults");
    
    for(Results::iterator i = _results.begin(); i != _results.end(); ++i)
    {
        delete *i;
    }
    _results.clear();
}

void Evaluator::clearStack()
{
    while(!_stack.empty())
    {
        ScopedExpression top = _stack.back();
        _stack.pop_back();
        clearNames();
        _names = top.names;
    }
}
