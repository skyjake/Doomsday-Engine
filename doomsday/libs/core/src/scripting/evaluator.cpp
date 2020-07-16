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

#include "de/scripting/evaluator.h"
#include "de/scripting/expression.h"
#include "de/scripting/context.h"
#include "de/scripting/process.h"
#include "de/value.h"

namespace de {

DE_PIMPL(Evaluator)
{
    /// The context that owns this evaluator.
    Context &context;

    struct ScopedExpression {
        const Expression *expression;
        Value *           scope; // owned

        ScopedExpression(const Expression *e = nullptr, Value *s = nullptr)
            : expression(e)
            , scope(s)
        {}
        Record *names() const
        {
            if (!scope) return nullptr;
            return scope->memberScope();
        }
    };
    struct ScopedResult {
        Value *result;
        Value *scope; // owned

        ScopedResult(Value *v, Value *s = nullptr) : result(v), scope(s) {}
    };

    typedef List<ScopedExpression> Expressions;
    typedef List<ScopedResult> Results;

    /// The expression that is currently being evaluated.
    const Expression *current;

    /// Namespace for the current expression.
    Record *names;

    Expressions expressions;
    Results results;

    /// Returned when there is no result to give.
    NoneValue noResult;

    Impl(Public *i, Context &owner)
        : Base(i)
        , context(owner)
        , current(nullptr)
        , names(nullptr)
    {}

    ~Impl()
    {
        DE_ASSERT(expressions.isEmpty());
        clearNames();
        clearResults();
    }

    void clearNames()
    {
        if (names)
        {
            names = nullptr;
        }
    }

    void clearResults()
    {
        for (const ScopedResult &i : results)
        {
            delete i.result;
            delete i.scope;
        }
        results.clear();
    }

    void clearExpressions()
    {
        while (!expressions.empty())
        {
            ScopedExpression top = expressions.takeLast();
            clearNames();
            names = top.names();
            delete top.scope;
        }
    }

    void pushResult(Value *value, Value *scope = nullptr /*taken*/)
    {
        // NULLs are not pushed onto the results expressions as they indicate that
        // no result was given.
        if (value)
        {
            /*qDebug() << "Evaluator: Pushing result" << value << value->asText() << "in scope"
                        << (scope? scope->asText() : "null")
                        << "result stack size:" << results.size();*/
            results << ScopedResult(value, scope);
        }
        else
        {
            DE_ASSERT(scope == nullptr);
        }
    }

    Value &result()
    {
        if (results.isEmpty())
        {
            return noResult;
        }
        return *results.first().result;
    }

    Value &evaluate(const Expression *expression)
    {
        DE_ASSERT(names == nullptr);
        DE_ASSERT(expressions.empty());

        //qDebug() << "Evaluator: Starting evaluation of" << expression;

        // Begin a new evaluation operation.
        current = expression;
        expression->push(self());

        // Clear the result stack.
        clearResults();

        while (!expressions.empty())
        {
            // Continue by processing the next step in the evaluation.
            ScopedExpression top = expressions.takeLast();
            clearNames();
            names = top.names();
            /*qDebug() << "Evaluator: Evaluating latest scoped expression" << top.expression
                     << "in" << (top.scope? names->asText() : "null scope");*/
            pushResult(top.expression->evaluate(self()), top.scope);
        }

        // During function call evaluation the process's context changes. We should
        // now be back at the level we started from.
        DE_ASSERT(&self().process().context() == &context);

        // Exactly one value should remain in the result stack: the result of the
        // evaluated expression.
        DE_ASSERT(self().hasResult());

        clearNames();
        current = nullptr;
        return result();
    }
};

} // namespace de

namespace de {

Evaluator::Evaluator(Context &owner) : d(new Impl(this, owner))
{}

Context &Evaluator::context()
{
    return d->context;
}

Process &Evaluator::process()
{
    return d->context.process();
}

const Process &Evaluator::process() const
{
    return d->context.process();
}

void Evaluator::reset()
{
    d->current = nullptr;

    d->clearExpressions();
    d->clearNames();
}

Value &Evaluator::evaluate(const Expression *expression)
{
    return d->evaluate(expression);
}

void Evaluator::namespaces(Namespaces &spaces) const
{
    if (d->names)
    {
        // A specific namespace has been defined.
        spaces.clear();
        spaces.push_back({d->names, Context::Namespace});
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
    DE_ASSERT(!spaces.empty());
    DE_ASSERT(spaces.front().names != nullptr);
    return spaces.front().names;
}

bool Evaluator::hasResult() const
{
    return d->results.size() == 1;
}

Value &Evaluator::result()
{
    return d->result();
}

void Evaluator::push(const Expression *expression, Value *scope)
{
    d->expressions.push_back(Impl::ScopedExpression(expression, scope));
}

void Evaluator::pushResult(Value *value)
{
    d->pushResult(value);
}

Value *Evaluator::popResult(Value **evaluationScope)
{
    DE_ASSERT(d->results.size() > 0);

    Impl::ScopedResult result = d->results.takeLast();
    /*qDebug() << "Evaluator: Popping result" << result.result << result.result->asText()
             << "in scope" << (result.scope? result.scope->asText() : "null");*/

    if (evaluationScope)
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
