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

#include "de/scripting/process.h"
#include "de/variable.h"
#include "de/arrayvalue.h"
#include "de/recordvalue.h"
#include "de/textvalue.h"
#include "de/nonevalue.h"
#include "de/scripting/trystatement.h"
#include "de/scripting/catchstatement.h"

#include <sstream>

namespace de {

DE_PIMPL(Process)
{
    State state;

    // The execution environment.
    typedef List<Context *> ContextStack;
    ContextStack stack;

    /// This is the current working folder of the process. Relative paths
    /// given to workingFile() are located in relation to this
    /// folder. Initial value is the root folder.
    String workingPath;

    /// Time when execution was started at depth 1.
    Time startedAt;

    Impl(Public *i)
        : Base(i)
        , state(Stopped)
        , workingPath("/")
    {}

    ~Impl()
    {
        clearStack();
    }

    void clear()
    {
        // We'll remember a global namespace specified in the constructor.
        Record *externalGlobals = (!stack.empty() && stack.front()->hasExternalGlobalNamespace()?
                                   &stack.front()->names() : 0);

        state = Stopped;
        clearStack();
        self().pushContext(new Context(Context::BaseProcess, thisPublic, externalGlobals));
        workingPath = "/";
    }

    Context &context(duint downDepth = 0)
    {
        DE_ASSERT(downDepth < depth());
        return **(stack.rbegin() + downDepth);
    }

    inline dsize depth() const
    {
        return stack.size();
    }

    /// Pops contexts off the stack until depth @a downToLevel is reached.
    void clearStack(duint downToLevel = 0)
    {
        while (depth() > downToLevel)
        {
            delete stack.back();
            stack.pop_back();
        }
    }

    void run(const Statement *firstStatement)
    {
        if (state != Stopped)
        {
            throw NotStoppedError("Process::run", "Process must be stopped first");
        }
        state = Running;

        // Make sure the stack is clear except for the process context.
        clearStack(1);

        context().start(firstStatement);
    }

    /// Fast forward to a suitable catch statement for @a err.
    /// @return  @c true, if suitable catch statement found.
    bool jumpIntoCatch(const Error &err)
    {
        dint level = 0;

        // Proceed along default flow.
        for (context().proceed(); context().current(); context().proceed())
        {
            const Statement *statement = context().current();
            if (is<TryStatement>(statement))
            {
                // Encountered a nested try statement.
                ++level;
                continue;
            }
            if (const auto *catchStatement = dynamic_cast<const CatchStatement *>(statement))
            {
                if (!level)
                {
                    // This might be the catch for us.
                    if (catchStatement->matches(err))
                    {
                        catchStatement->executeCatch(context(), err);
                        return true;
                    }
                }
                if (catchStatement->isFinal() && level > 0)
                {
                    // A sequence of catch statements has ended.
                    --level;
                }
            }
        }

        // Failed to find a catch statement that matches the given error.
        return false;
    }
};

/// If execution continues for longer than this, a HangError is thrown.
static constexpr TimeSpan MAX_EXECUTION_TIME = 10.0_s;

Process::Process(Record *externalGlobalNamespace) : d(new Impl(this))
{
    // Push the first context on the stack. This bottommost context
    // is never popped from the stack. Its namespace is the global namespace
    // of the process.
    pushContext(new Context(Context::BaseProcess, this, externalGlobalNamespace));
}

Process::Process(const Script &script) : d(new Impl(this))
{
    clear();

    // If a script is provided, start running it automatically.
    run(script);
}

Process::State Process::state() const
{
    return d->state;
}

void Process::clear()
{
    d->clear();
}

dsize Process::depth() const
{
    return d->depth();
}

void Process::run(const Script &script)
{
    d->run(script.firstStatement());

    // Set up the automatic variables.
    globals().set(Record::VAR_FILE, script.path());
}

void Process::suspend(bool suspended)
{
    if (d->state == Stopped)
    {
        throw SuspendError("Process:suspend",
            "Stopped processes cannot be suspended or resumed");
    }

    d->state = (suspended? Suspended : Running);
}

void Process::stop()
{
    d->state = Stopped;

    // Clear the context stack, apart from the bottommost context, which
    // represents the process itself.
    DE_FOR_EACH_REVERSE(Impl::ContextStack, i, d->stack)
    {
        if (*i != d->stack[0])
        {
            delete *i;
        }
    }
    DE_ASSERT(!d->stack.empty());

    // Erase all but the first context.
    d->stack.erase(d->stack.begin() + 1, d->stack.end());

    // This will reset any half-done evaluations, but it won't clear the namespace.
    context().reset();
}

void Process::execute()
{
    if (d->state == Suspended || d->state == Stopped)
    {
        // The process is not active.
        return;
    }

    // We will execute until this depth is complete.
    dsize startDepth = d->depth();
    if (startDepth == 1)
    {
        // Mark the start time.
        d->startedAt = Time();
    }

    // Execute the next command(s).
    while (d->state == Running && d->depth() >= startDepth)
    {
        try
        {
            dsize execDepth = d->depth();
            if (!context().execute() && d->depth() == execDepth)
            {
                // There was no statement left to execute, and no new contexts were
                // added to the stack.
                finish();
            }
            else if (d->startedAt.since() > MAX_EXECUTION_TIME)
            {
                /// @throw HangError  Execution takes too long.
                throw HangError("Process::execute",
                    "Script execution takes too long, or is stuck in an infinite loop");
            }
        }
        catch (const Error &err)
        {
            //qDebug() << "Caught " << err.asText() << " at depth " << depth() << "\n";

            // Fast-forward to find a suitable catch statement.
            if (d->jumpIntoCatch(err))
            {
                // Suitable catch statement was found. The current statement is now
                // pointing at the catch compound's first statement.
                continue;
            }

            if (startDepth > 1)
            {
                // Pop this context off, it has not caught the exception.
                delete popContext();
                throw;
            }
            else
            {
                // Exception uncaught by all contexts, script execution stops.
                LOG_SCR_ERROR("Stopping process: ") << err.asText();
                stop();
            }
        }
    }
}

Context &Process::context(duint downDepth)
{
    return d->context(downDepth);
}

void Process::pushContext(Context *context)
{
    d->stack.push_back(context);
}

Context *Process::popContext()
{
    Context *topmost = d->stack.back();
    d->stack.pop_back();

    // Pop a global namespace as well, if present.
    if (context().type() == Context::GlobalNamespace)
    {
        delete d->stack.back();
        d->stack.pop_back();
    }

    return topmost;
}

void Process::finish(Value *returnValue)
{
    DE_ASSERT(depth() >= 1);

    // Move one level downwards in the context stack.
    if (depth() > 1)
    {
        // Finish the topmost context.
        std::unique_ptr<Context> topmost(popContext());
        if (topmost->type() == Context::FunctionCall)
        {
            // Return value to the new topmost level.
            //qDebug() << "Process: Pushing function return value" << returnValue << returnValue->asText();
            context().evaluator().pushResult(returnValue? returnValue : new NoneValue);
        }
        else
        {
            DE_ASSERT(returnValue == nullptr);
        }
    }
    else
    {
        DE_ASSERT(d->stack.back()->type() == Context::BaseProcess);

        if (returnValue) delete returnValue; // Possible return value ignored.

        // This was the last level.
        d->state = Stopped;
    }
}

const String &Process::workingPath() const
{
    return d->workingPath;
}

void Process::setWorkingPath(const String &newWorkingPath)
{
    d->workingPath = newWorkingPath;
}

void Process::call(const Function &function, const ArrayValue &arguments, Value *self)
{
    // First map the argument values.
    Function::ArgumentValues argValues;
    function.mapArgumentValues(arguments, argValues);

    if (function.isNative())
    {
        // Do a native function call.
        context().setNativeSelf(self);
        context().evaluator().pushResult(function.callNative(context(), argValues));
        context().setNativeSelf(0);
    }
    else
    {
        // If the function resides in another process's namespace, push
        // that namespace on the stack first.
        if (function.globals() && function.globals() != &globals())
        {
            pushContext(new Context(Context::GlobalNamespace, this, function.globals()));
        }

        // Create a new context.
        pushContext(new Context(Context::FunctionCall, this));

        // If the scope is defined, create the "self" variable for it.
        if (self)
        {
            context().names().add(new Variable(DE_STR("self"), self /*taken*/));
        }

        // Create local variables for the arguments in the new context.
        Function::ArgumentValues::const_iterator b = argValues.begin();
        Function::Arguments     ::const_iterator a = function.arguments().begin();
        for (; b != argValues.end() && a != function.arguments().end(); ++b, ++a)
        {
            // Records must only be passed as unowned references.
            context().names().add(new Variable(*a, (*b)->duplicateAsReference()));
        }

        // This should never be called if the process is suspended.
        DE_ASSERT(d->state != Suspended);

        if (d->state == Running)
        {
            // Execute the function as part of the currently running process.
            context().start(function.compound().firstStatement());
            execute();
        }
        else if (d->state == Stopped)
        {
            // We'll execute just this one function.
            d->state = Running;
            context().start(function.compound().firstStatement());
            execute();
            d->state = Stopped;
        }
    }
}

void Process::namespaces(Namespaces &spaces) const
{
    spaces.clear();

    bool gotFunction = false;

    DE_FOR_EACH_CONST_REVERSE(Impl::ContextStack, i, d->stack)
    {
        Context &context = **i;
        if (context.type() == Context::FunctionCall)
        {
            // Only the topmost function call namespace is available: one cannot
            // access the local variables of the callers.
            if (gotFunction) continue;
            gotFunction = true;
        }
        spaces.push_back({&context.names(), unsigned(context.type())});
        if (context.type() == Context::GlobalNamespace)
        {
            // This shadows everything below.
            break;
        }
    }
}

Record &Process::globals()
{
    // Find the global namespace currently in effect.
    DE_FOR_EACH_CONST_REVERSE(Impl::ContextStack, i, d->stack)
    {
        Context &context = **i;
        if (context.type() == Context::GlobalNamespace || context.type() == Context::BaseProcess)
        {
            return context.names();
        }
    }
    return d->stack[0]->names();
}

Record &Process::locals()
{
    return d->stack.back()->names();
}

} // namespace de
