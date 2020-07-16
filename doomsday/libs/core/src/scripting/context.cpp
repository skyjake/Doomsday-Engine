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

#include "de/scripting/context.h"
#include "de/scripting/statement.h"
#include "de/scripting/process.h"
#include "de/recordvalue.h"

namespace de {

DE_PIMPL(Context)
{
    /**
     * Information about the control flow is stored within a stack of
     * ControlFlow instances.
     */
    class ControlFlow {
    public:
        /**
         * Constructor.
         *
         * @param current  Current statement being executed.
         * @param f        Statement where normal flow continues.
         * @param c        Statement where to jump on "continue".
         *                 @c NULL if continuing is not allowed.
         * @param b        Statement where to jump to and flow from on "break".
         *                 @c NULL if breaking is not allowed.
         */
        ControlFlow(const Statement *current,
                    const Statement *f = nullptr,
                    const Statement *c = nullptr,
                    const Statement *b = nullptr)
            : flow(f), jumpContinue(c), jumpBreak(b), _current(current) {}

        /// Returns the currently executed statement.
        const Statement *current() const { return _current; }

        /// Sets the currently executed statement. When the statement
        /// changes, the phase is reset back to zero.
        void setCurrent(const Statement *s) { _current = s; }

    public:
        const Statement *flow;
        const Statement *jumpContinue;
        const Statement *jumpBreak;
        std::unique_ptr<Value> iteration;

    private:
        const Statement *_current;
    };

    /// Type of the execution context.
    Type type;

    /// The process that owns this context.
    Process *owner;

    /// Control flow stack.
    typedef std::vector<ControlFlow> FlowStack;
    FlowStack controlFlow;

    /// Expression evaluator.
    Evaluator evaluator;

    /// Determines whether the namespace is owned by the context.
    bool ownsNamespace;

    /// The local namespace of this context.
    Record *names;

    std::unique_ptr<Value> nativeSelf;

    Variable throwaway;

    Impl(Public *i, Type type, Process *owner, Record *globals)
        : Base(i)
        , type(type)
        , owner(owner)
        , evaluator(*i)
        , ownsNamespace(false)
        , names(globals)
    {
        if (!names)
        {
            // Create a private empty namespace.
            DE_ASSERT(type != GlobalNamespace);
            names = new Record;
            ownsNamespace = true;
        }
    }

    ~Impl()
    {
        if (ownsNamespace)
        {
            delete names;
        }
        self().reset();
        DE_ASSERT(controlFlow.empty());
    }

    /// Returns the topmost control flow information.
    ControlFlow &flow()
    {
        return controlFlow.back();
    }

    /// Pops the topmost control flow instance off of the stack. The
    /// iteration value is deleted, if it has been defined.
    void popFlow()
    {
        DE_ASSERT(!controlFlow.empty());
        controlFlow.pop_back();
    }

    /// Sets the currently executed statement.
    void setCurrent(const Statement *statement)
    {
        if (controlFlow.size())
        {
            evaluator.reset();
            flow().setCurrent(statement);
        }
        else
        {
            DE_ASSERT(statement == nullptr);
        }
    }
};

Context::Context(Type type, Process *owner, Record *globals)
    : d(new Impl(this, type, owner, globals))
{}

Context::Type Context::type()
{
    return d->type;
}

Process &Context::process()
{
    return *d->owner;
}

Evaluator &Context::evaluator()
{
    return d->evaluator;
}

bool Context::hasExternalGlobalNamespace() const
{
    return !d->ownsNamespace;
}

Record &Context::names()
{
    return *d->names;
}

void Context::start(const Statement *statement,    const Statement *fallback,
                    const Statement *jumpContinue, const Statement *jumpBreak)
{
    d->controlFlow.push_back(Impl::ControlFlow(statement, fallback, jumpContinue, jumpBreak));

    // When the current statement is NULL it means that the sequence of statements
    // has ended, so we shouldn't do that until there really is no more statements.
    if (!current())
    {
        proceed();
    }
}

void Context::reset()
{
    while (!d->controlFlow.empty())
    {
        d->popFlow();
    }
    d->evaluator.reset();
}

bool Context::execute()
{
    if (current())
    {
        current()->execute(*this);
        return true;
    }
    return false;
}

void Context::proceed()
{
    const Statement *st = nullptr;
    if (current())
    {
        st = current()->next();
    }
    // Should we fall back to a point that was specified earlier?
    while (!st && d->controlFlow.size())
    {
        st = d->controlFlow.back().flow;
        d->popFlow();
    }
    d->setCurrent(st);
}

void Context::jumpContinue()
{
    const Statement *st = nullptr;
    while (!st && d->controlFlow.size())
    {
        st = d->controlFlow.back().jumpContinue;
        d->popFlow();
    }
    if (!st)
    {
        throw JumpError("Context::jumpContinue", "No jump targets defined for continue");
    }
    d->setCurrent(st);
}

void Context::jumpBreak(duint count)
{
    if (count < 1)
    {
        throw JumpError("Context::jumpBreak", "Invalid number of nested breaks");
    }

    const Statement *st = nullptr;
    while ((!st || count > 0) && d->controlFlow.size())
    {
        st = d->controlFlow.back().jumpBreak;
        if (st)
        {
            --count;
        }
        d->popFlow();
    }
    if (count > 0)
    {
        throw JumpError("Context::jumpBreak", "Too few nested compounds to break out of");
    }
    if (!st)
    {
        throw JumpError("Context::jumpBreak", "No jump targets defined for break");
    }
    d->setCurrent(st);
    proceed();
}

const Statement *Context::current()
{
    if (d->controlFlow.size())
    {
        return d->flow().current();
    }
    return nullptr;
}

Value *Context::iterationValue()
{
    DE_ASSERT(d->controlFlow.size());
    return d->controlFlow.back().iteration.get();
}

void Context::setIterationValue(Value *value)
{
    DE_ASSERT(d->controlFlow.size());
    d->flow().iteration.reset(value);
}

void Context::setNativeSelf(Value *scope)
{
    d->nativeSelf.reset(scope);
}

Value &Context::nativeSelf() const
{
    DE_ASSERT(d->nativeSelf);
    if (!d->nativeSelf)
    {
        throw UndefinedScopeError("Context::nativeSelf",
                                  "Context is not executing in scope of any instance");
    }
    return *d->nativeSelf;
}

Record &Context::selfInstance() const
{
    Record *obj = nativeSelf().as<RecordValue>().record();
    if (!obj)
    {
        throw UndefinedScopeError("Context::selfInstance", "No \"self\" instance has been set");
    }
    return *obj;
}

Variable &Context::throwaway()
{
    return d->throwaway;
}

} // namespace de
