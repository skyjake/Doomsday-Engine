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

Context::Context(Type type, Process* owner)
    : type_(type), owner_(owner), evaluator_(*this)
{}

Context::~Context()
{
    reset();
}

Evaluator& Context::evaluator()
{
    return evaluator_;
}

Record& Context::names() 
{
    return names_;
}

void Context::start(const Statement* statement, const Statement* fallback, 
    const Statement* jumpContinue, const Statement* jumpBreak)
{
    controlFlow_.push_back(ControlFlow(statement, fallback, jumpContinue, jumpBreak));
}

void Context::reset()
{
    while(!controlFlow_.empty())
    {
        popFlow();
    }    
    evaluator_.reset();
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
    if(current())
    {
        const Statement* st = current()->next();
        
        // Should we fall back to a point that was specified earlier?
        while(!st && controlFlow_.size())
        {
            st = controlFlow_.back().flow;
            popFlow();
        }
        setCurrent(st);
    }
}

void Context::jumpContinue()
{
    const Statement* st = NULL;
    while(!st && controlFlow_.size())
    {
        st = controlFlow_.back().jumpContinue;
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
    while((!st || count > 0) && controlFlow_.size())
    {
        st = controlFlow_.back().jumpBreak;
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
    if(controlFlow_.size())
    {
        return flow().current();
    }
    return NULL;
}

void Context::setCurrent(const Statement* statement)
{
    if(controlFlow_.size())
    {
        evaluator_.reset();
        flow().setCurrent(statement);
    }
    else
    {
        assert(statement == NULL);
    }
}

Value* Context::iterationValue()
{
    assert(controlFlow_.size());
    return controlFlow_.back().iteration;
}

void Context::setIterationValue(Value* value)
{
    assert(controlFlow_.size());
    
    ControlFlow& fl = flow();
    if(fl.iteration)
    {
        delete fl.iteration;
    }
    fl.iteration = value;
}

void Context::popFlow()
{
    assert(!controlFlow_.empty());
    delete flow().iteration;
    controlFlow_.pop_back();
}
