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
#include "de/Stopwatch"
#include "de/Variable"
#include "de/ArrayValue"
#include "de/TextValue"
#include "de/NoneValue"

#include <sstream>

using namespace de;
using std::auto_ptr;
    
/// If execution continues for longer this, a HangError is thrown.
static const Time::Delta MAX_EXECUTION_TIME = 10;

Process::Process(const Script& script) : state_(STOPPED), firstExecute_(true)
{
    // Place the first context on the stack. This bottommost context
    // is never removed from the stack. Its namespace is the local namespace
    // of the process.
    stack_.push_back(new Context(Context::PROCESS, this));

    // Initially use the root folder as the cwd.
    workingPath_ = "/";

    // If a script is provided, start running it automatically.
    if(script.firstStatement())
    {
        state_ = RUNNING;
        context().start(script.firstStatement());
    }
}

Process::~Process()
{
    for(ContextStack::reverse_iterator i = stack_.rbegin(); i != stack_.rend(); ++i)
    {
        delete *i;
    }
}

void Process::run(const Script& script)
{
    if(state_ != STOPPED)
    {
        throw NotStoppedError("Process::run", 
            "When a new script is started the process must be stopped first");
    }
    
    state_ = RUNNING;
    context().start(script.firstStatement());
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
    
    // On the first time execute is called, we need to do some extra 
    // preparations.
    if(firstExecute_)
    {
        firstExecute_ = false;

        /*
        // Create a variable that provides access to the Process node itself.
        Object* object = 
            kernel().names().node<NodeObject>("Process")->instantiate(path(), context());
        newVariable(context().names(), "PROC", new ObjectValue(object));
        object->release();
        
        // Some other convenient built-in variables.
        object = kernel().names().node<NodeObject>("Node")->instantiate("/", context());
        newVariable(context().names(), "ROOT", new ObjectValue(object));
        object->release();
        
        object = kernel().names().node<NodeObject>("Node")->instantiate("/ui", context());
        newVariable(context().names(), "UI", new ObjectValue(object));
        object->release();
        */
    }

    // Is the process sleeping?
    if(sleepUntil_.until() > 0)
    {
        // We'll be back later.
        return;
    }

    try
    {
        Time startedAt;
        // Execute the next command(s).
        while(sleepUntil_.until() < 0 && state_ == RUNNING)
        {
            if(!context().execute())
            {
                finish();
            }
            if(startedAt.since() > MAX_EXECUTION_TIME)
            {
                /// @throw HangError  Execution takes too long.
                throw HangError("Process::execute", 
                    "Script execution takes too long, or is stuck in an infinite loop");
            }
        }
    }
    catch(const Error& err)
    {
        std::cout << "Process::execute:" << err.what() << "\n";
        std::cout << "Stopping process.\n";
        stop();
    }
}

Context& Process::context()
{
    return *stack_.back();
}

void Process::finish(Value* returnValue)
{
    assert(stack_.size() >= 1);

    // Move one level downwards in the context stack.
    if(stack_.size() > 1)
    {
        // Finish the topmost context.
        Context* topmost = stack_.back();
        stack_.pop_back();
        
        if(topmost->type() == Context::FUNCTION_CALL)
        {
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

#if 0
Variable* Process::newVariable(Folder& names, const std::string& path, Value* initialValue)
{
    Object* object = dynamic_cast<Object*>(getContainingFolder(names, path));
    
    string variableName = Folder::extractNode(path);
    if(object->has(variableName))
    {
        throw AlreadyExistsError("Process::newVariable",
            "'" + variableName + "' already exists in '" + Folder::extractFolder(path) + "'");
    }
    
    Variable* variable = new Variable(variableName, initialValue);
    object->add(variable);
    return variable;
}

Method* Process::newMethod(const std::string& path, const Method::Arguments& arguments, const Method::Defaults& defaults, const Statement* firstStatement)
{
    Object* object = dynamic_cast<Object*>(
        getContainingFolder(stack_.back()->names(), path));
    
    string methodName = Folder::extractNode(path);
    if(object->has(methodName))
    {
        throw AlreadyExistsError("Process::newMethod",
            "'" + methodName + "' already exists in '" + Folder::extractFolder(path) + "'");
    }

    Method* method = new Method(methodName, arguments, defaults, firstStatement);
    object->add(method);
    return method;
}

void Process::call(Method& method, Object& self, const ArrayValue* arguments)
{
    assert(arguments != NULL);
    
    // First map the argument values.
    Method::ArgumentValues argValues;
    method.mapArgumentValues(*arguments, argValues);
    
    if(!method.callNative(context(), self, argValues))
    {
        // Create a new context.
        stack_.push_back(new Context(Context::METHOD_CALL, this));

        Method::ArgumentValues::const_iterator b = argValues.begin();
        Method::Arguments::const_iterator a = method.arguments().begin();
        for(; b != argValues.end() && a != method.arguments().end(); ++b, ++a)
        {
            newVariable(context().names(), *a, (*b)->duplicate());
        }
    
        // Initialize the variables of the context.
        newVariable(context().names(), "self", new ObjectValue(&self));
    
        // Start executing the method.
        context().start(method.firstStatement());
    }
}
#endif
