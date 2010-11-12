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

#include "de/Context"
#include "de/Statement"
#include "de/Process"

using namespace de;

Context::Context(Type type, Process* owner, Record* globals)
    : _type(type), _owner(owner), _evaluator(*this), _ownsNamespace(false), _names(globals)
{
    if(!_names)
    {
        // Create a private empty namespace.
        Q_ASSERT(_type != GLOBAL_NAMESPACE);
        _names = new Record();
        _ownsNamespace = true;
    }
}

Context::~Context()
{
    if(_ownsNamespace)
    {
        delete _names;
    }        
    reset();
}

Evaluator& Context::evaluator()
{
    return _evaluator;
}

Record& Context::names() 
{
    return *_names;
}

void Context::start(const Statement* statement, const Statement* fallback, 
    const Statement* jumpContinue, const Statement* jumpBreak)
{
    _controlFlow.push_back(ControlFlow(statement, fallback, jumpContinue, jumpBreak));

    // When the current statement is NULL it means that the sequence of statements
    // has ended, so we shouldn't do that until there really is no more statements.
    if(!current())
    {
        proceed();
    }
}

void Context::reset()
{
    while(!_controlFlow.empty())
    {
        popFlow();
    }    
    _evaluator.reset();
}

bool Context::execute()
{
    if(current() != NULL)
    {
        current()->execute(*this);
        return true;
    }
    return false;
}

void Context::proceed()
{
    const Statement* st = NULL;
    if(current())
    {
        st = current()->next();
    }
    // Should we fall back to a point that was specified earlier?
    while(!st && _controlFlow.size())
    {
        st = _controlFlow.back().flow;
        popFlow();
    }
    setCurrent(st);
}

void Context::jumpContinue()
{
    const Statement* st = NULL;
    while(!st && _controlFlow.size())
    {
        st = _controlFlow.back().jumpContinue;
        popFlow();
    }
    if(!st)
    {
        throw JumpError("Context::jumpContinue", "No jump targets defined for continue");
    }
    setCurrent(st);
}

void Context::jumpBreak(duint count)
{
    if(count < 1)
    {
        throw JumpError("Context::jumpBreak", "Invalid number of nested breaks");
    }
    
    const Statement* st = NULL;
    while((!st || count > 0) && _controlFlow.size())
    {
        st = _controlFlow.back().jumpBreak;
        if(st)
        {
            --count;
        }
        popFlow();
    }
    if(count > 0)
    {
        throw JumpError("Context::jumpBreak", "Too few nested compounds to break out of");
    }
    if(!st)
    {
        throw JumpError("Context::jumpBreak", "No jump targets defined for break");
    }
    setCurrent(st);
    proceed();
}

const Statement* Context::current()
{
    if(_controlFlow.size())
    {
        return flow().current();
    }
    return NULL;
}

void Context::setCurrent(const Statement* statement)
{
    if(_controlFlow.size())
    {
        _evaluator.reset();
        flow().setCurrent(statement);
    }
    else
    {
        Q_ASSERT(statement == NULL);
    }
}

Value* Context::iterationValue()
{
    Q_ASSERT(_controlFlow.size());
    return _controlFlow.back().iteration;
}

void Context::setIterationValue(Value* value)
{
    Q_ASSERT(_controlFlow.size());
    
    ControlFlow& fl = flow();
    if(fl.iteration)
    {
        delete fl.iteration;
    }
    fl.iteration = value;
}

void Context::popFlow()
{
    Q_ASSERT(!_controlFlow.empty());
    delete flow().iteration;
    _controlFlow.pop_back();
}
