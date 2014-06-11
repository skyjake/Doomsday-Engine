/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Evaluator"
#include "de/Expression"
#include "de/Value"
#include "de/Context"
#include "de/Process"

#include <QList>

namespace de {

DENG2_PIMPL(Evaluator)
{
    /// The context that owns this evaluator.
    Context &context;

    struct ScopedExpression {
        Expression const *expression;
        Value *scope; // owned

        ScopedExpression(Expression const *e = 0, Value *s = 0) : expression(e), scope(s) {}
        Record *names() const {
            if(!scope) return 0;
            return scope->memberScope();
        }
    };
    struct ScopedResult {
        Value *result;
        Value *scope; // owned

        ScopedResult(Value *v, Value *s = 0) : result(v), scope(s) {}
    };

    typedef QList<ScopedExpression> Expressions;
    typedef QList<ScopedResult> Results;

    /// The expression that is currently being evaluated.
    Expression const *current;

    /// Namespace for the current expression.
    Record *names;

    Expressions stack;
    Results results;

    /// Returned when there is no result to give.
    NoneValue noResult;

    Instance(Public *i, Context &owner)
        : Base(i)
        , context(owner)
        , current(0)
        , names(0)
    {}

    ~Instance()
    {
        clearNames();
        clearResults();
    }

    void clearNames()
    {
        if(names)
        {
            names = 0;
        }
    }

    void clearResults()
    {
        foreach(ScopedResult const &i, results)
        {
            delete i.result;
        }
        results.clear();
    }

    void clearStack()
    {
        while(!stack.empty())
        {
            ScopedExpression top = stack.takeLast();
            clearNames();
            names = top.names();
            delete top.scope;
        }
    }

    void pushResult(Value *value, Value *scope = 0 /*take*/)
    {
        // NULLs are not pushed onto the results stack as they indicate that
        // no result was given.
        if(value)
        {
            qDebug() << "Evaluator: Pushing result" << value->asText() << "in scope"
                        << (scope? scope->asText() : "null");
            results << ScopedResult(value, scope);
        }
    }

    Value &result()
    {
        if(results.isEmpty())
        {
            return noResult;
        }
        return *results.first().result;
    }

    Value &evaluate(Expression const *expression)
    {
        DENG2_ASSERT(names == NULL);
        DENG2_ASSERT(stack.empty());

        qDebug() << "Evaluator: Starting evaluation of" << expression;

        // Begin a new evaluation operation.
        current = expression;
        expression->push(self);

        // Clear the result stack.
        clearResults();

        while(!stack.empty())
        {
            // Continue by processing the next step in the evaluation.
            ScopedExpression top = stack.takeLast();
            clearNames();
            names = top.names();
            qDebug() << "Evaluator: Evaluating latest scoped expression" << top.expression
                     << "in" << (top.scope? names->asText() : "null scope");
            pushResult(top.expression->evaluate(self), top.scope);
        }

        // During function call evaluation the process's context changes. We should
        // now be back at the level we started from.
        DENG2_ASSERT(&self.process().context() == &context);

        // Exactly one value should remain in the result stack: the result of the
        // evaluated expression.
        DENG2_ASSERT(self.hasResult());

        clearNames();
        current = NULL;
        return result();
    }
};

} // namespace de

namespace de {

Evaluator::Evaluator(Context &owner) : d(new Instance(this, owner))
{}

Context &Evaluator::context()
{
    return d->context;
}

Process &Evaluator::process()
{ 
    return d->context.process();
}

Process const &Evaluator::process() const
{
    return d->context.process();
}

void Evaluator::reset()
{
    d->current = NULL;
    
    d->clearStack();
    d->clearNames();
}

Value &Evaluator::evaluate(Expression const *expression)
{
    return d->evaluate(expression);
}

void Evaluator::namespaces(Namespaces &spaces) const
{
    if(d->names)
    {
        // A specific namespace has been defined.
        spaces.clear();
        spaces.push_back(d->names);
    }
    else
    {
        // Collect namespaces from the process's call stack.
        process().namespaces(spaces);
    }
}

Record *Evaluator::localNamespace() const
{
    Namespaces spaces;
    namespaces(spaces);
    DENG2_ASSERT(!spaces.empty());
    DENG2_ASSERT(spaces.front() != 0);
    return spaces.front();
}

bool Evaluator::hasResult() const
{
    return d->results.size() == 1;
}

Value &Evaluator::result()
{
    return d->result();
}

void Evaluator::push(Expression const *expression, Value *scope)
{
    d->stack.push_back(Instance::ScopedExpression(expression, scope));
}

void Evaluator::pushResult(Value *value)
{
    d->pushResult(value);
}

Value *Evaluator::popResult(Value **evaluationScope)
{
    DENG2_ASSERT(d->results.size() > 0);

    Instance::ScopedResult result = d->results.takeLast();
    qDebug() << "Evaluator: Popping result" << result.result->asText()
                << "in scope" << (result.scope? result.scope->asText() : "null");

    if(evaluationScope)
    {
        *evaluationScope = result.scope;
    }
    else
    {
        delete result.scope; // Was owned by us and the caller didn't want it.
    }

    return result.result;
}

} // namespace de
