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

#include "de/Process"
#include "de/Variable"
#include "de/ArrayValue"
#include "de/TextValue"
#include "de/NoneValue"

#include <sstream>

using namespace de;
using std::auto_ptr;
    
/// If execution continues for longer than this, a HangError is thrown.
static const Time::Delta MAX_EXECUTION_TIME = 10;

Process::Process() : state_(STOPPED), workingPath_("/")
{
    // Push the first context on the stack. This bottommost context
    // is never popped from the stack. Its namespace is the global namespace
    // of the process.
    stack_.push_back(new Context(Context::PROCESS, this));
}

Process::Process(const Script& script) : state_(STOPPED), workingPath_("/")
{
    stack_.push_back(new Context(Context::PROCESS, this));

    // If a script is provided, start running it automatically.
    run(script);
}

Process::~Process()
{
    clearStack();
}

void Process::clearStack(duint downToLevel)
{
    while(depth() > downToLevel)
    {
        delete stack_.back();
        stack_.pop_back();
    }
}

duint Process::depth() const
{
    return stack_.size();
}

void Process::run(const Script& script)
{
    if(state_ != STOPPED)
    {
        throw NotStoppedError("Process::run", 
            "When a new script is started the process must be stopped first");
    }
    state_ = RUNNING;
    
    // Make sure the stack is clear except for the process context.
    clearStack(1);
    
    context().start(script.firstStatement());
    
    // Set up the automatic variables.
    Record& ns = globals();
    if(ns.has("__file__"))
    {
        ns["__file__"].set(TextValue(script.path()));
    }
    else
    {
        ns.add(new Variable("__file__", new TextValue(script.path()), Variable::TEXT));
    }
}

void Process::suspend(bool suspended)
{
    if(state_ == STOPPED)
    {
        throw SuspendError("Process:suspend", 
            "Stopped processes cannot be suspended or resumed");
    }    
    
    state_ = (suspended? SUSPENDED : RUNNING);
}

void Process::stop()
{
    state_ = STOPPED;
    
    // Clear the context stack, apart from the bottommost context, which 
    // represents the process itself.
    for(ContextStack::reverse_iterator i = stack_.rbegin(); i != stack_.rend(); ++i)
    {
        if(*i != stack_[0])
        {
            delete *i;
        }
    }
    assert(!stack_.empty());

    // Erase all but the first context.
    stack_.erase(stack_.begin() + 1, stack_.end());
    
    // This will reset any half-done evaluations, but it won't clear the namespace.
    context().reset();
}

void Process::execute(const Time::Delta& timeBox)
{
    if(state_ == SUSPENDED || state_ == STOPPED)
    {
        // The process is not active.
        return;
    }
    
    //LOG_AS_STRING("Process " + name());

    // We will execute until this depth is complete.
    duint startDepth = depth();
    if(startDepth == 1)
    {
        // Mark the start time.
        startedAt_ = Time();
    }

    try
    {
        // Execute the next command(s).
        while(state_ == RUNNING && depth() >= startDepth)
        {
            if(!context().execute())
            {
                finish();
            }
            if(startedAt_.since() > MAX_EXECUTION_TIME)
            {
                /// @throw HangError  Execution takes too long.
                throw HangError("Process::execute", 
                    "Script execution takes too long, or is stuck in an infinite loop");
            }
        }
    }
    catch(const Error& err)
    {
        if(startDepth == 1)
        {
            std::cout << "Process::execute:" << err.what() << "\n";
            std::cout << "Stopping process.\n";
            stop();
        }
        else
        {
            err.raise();
        }
    }
}

Context& Process::context(duint downDepth)
{
    assert(downDepth < depth());
    return **(stack_.rbegin() + downDepth);
}

void Process::finish(Value* returnValue)
{
    assert(depth() >= 1);

    // Move one level downwards in the context stack.
    if(depth() > 1)
    {
        // Finish the topmost context.
        Context* topmost = stack_.back();
        stack_.pop_back();

        // Pop the global namespace as well, if present.
        if(context().type() == Context::GLOBAL_NAMESPACE)
        {
            delete stack_.back();
            stack_.pop_back();
        }

        if(topmost->type() == Context::FUNCTION_CALL)
        {
            // Return value to the new topmost level.
            context().evaluator().pushResult(returnValue? returnValue : new NoneValue());
        }
        delete topmost;
    }
    else
    {
        assert(stack_.back()->type() == Context::PROCESS);
        
        // This was the last level.
        state_ = STOPPED;
    }   
}

const String& Process::workingPath() const
{
    return workingPath_;
}

void Process::setWorkingPath(const String& newWorkingPath)
{
    workingPath_ = newWorkingPath;
}

void Process::call(Function& function, const ArrayValue& arguments)
{
    // First map the argument values.
    Function::ArgumentValues argValues;
    function.mapArgumentValues(arguments, argValues);
    
    if(!function.callNative(context(), argValues))
    {
        // If the function resides in another process's namespace, push
        // that namespace on the stack first.
        if(function.globals() && function.globals() != &globals())
        {
            stack_.push_back(new Context(Context::GLOBAL_NAMESPACE, this, 
                function.globals()));
        }
        
        // Create a new context.
        stack_.push_back(new Context(Context::FUNCTION_CALL, this));
        
        // Create local variables for the arguments in the new context.
        Function::ArgumentValues::const_iterator b = argValues.begin();
        Function::Arguments::const_iterator a = function.arguments().begin();
        for(; b != argValues.end() && a != function.arguments().end(); ++b, ++a)
        {
            context().names().add(new Variable(*a, (*b)->duplicate()));
        }
        
        // Execute the function.
        context().start(function.compound().firstStatement());
        execute();
    }
}

void Process::namespaces(Namespaces& spaces)
{
    spaces.clear();
    
    for(ContextStack::reverse_iterator i = stack_.rbegin(); i != stack_.rend(); ++i)
    {
        Context& context = **i;
        spaces.push_back(&context.names());
        if(context.type() == Context::GLOBAL_NAMESPACE)
        {
            // This shadows everything below.
            break;
        }
    }
}

Record& Process::globals()
{
    return stack_[0]->names();
}
