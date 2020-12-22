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

#ifndef LIBCORE_EVALUATOR_H
#define LIBCORE_EVALUATOR_H

#include "../libcore.h"
#include "de/nonevalue.h"

#include <vector>
#include <list>

namespace de {

class Context;
class Process;
class Expression;
class Value;
class Record;

/**
 * Stack for evaluating expressions.
 *
 * @ingroup script
 */
class DE_PUBLIC Evaluator
{
public:
    /// Result is of wrong type. @ingroup errors
    DE_ERROR(ResultTypeError);

    using Namespace = struct { Record *names; unsigned nsType; };
    using Namespaces = std::list<Namespace>;

public:
    Evaluator(Context &owner);

    Context &context();

    /**
     * Returns the process that owns this evaluator.
     */
    Process &process();

    /**
     * Returns the process that owns this evaluator.
     */
    const Process &process() const;

    /**
     * Resets the evaluator so it's ready for another expression.
     * Called when the statement changes in the context.
     */
    void reset();

    /**
     * Fully evaluate the given expression. The result value will remain
     * in the results stack.
     *
     * @return  Result of the evaluation.
     */
    Value &evaluate(const Expression *expression);

    template <typename Type>
    Type &evaluateTo(const Expression *expr) {
        Type *r = dynamic_cast<Type *>(&evaluate(expr));
        if (!r) {
            throw ResultTypeError("Evaluator::result<Type>", "Unexpected result type");
        }
        return *r;
    }

    /**
     * Determines the namespace for the currently evaluated expression.
     * Those expressions whose operation depends on the current
     * namespace scope, should use this to look identifiers from.
     * This changes as expressions are popped off the stack.
     *
     * @return  Namespace scope of the current evaluation. If @c NULL,
     *          expressions should assume that all namespaces are available.
     */
    Record *names() const;

    /**
     * Collect the namespaces currently visible.
     *
     * @param spaces  List of namespaces. The order is important: the earlier
     *                namespaces shadow the subsequent ones.
     */
    void namespaces(Namespaces &spaces) const;

    /**
     * Returns the current local namespace (topmost namespace from namespaces()).
     */
    Record *localNamespace() const;

    /**
     * Insert the given expression to the top of the expression stack.
     *
     * @param expression  Expression to push on the stack.
     * @param scope       Scope for this expression only (using memberScope()).
     *                    Evaluator takes ownership of this value.
     */
    void push(const Expression *expression, Value *scope = 0);
    
    /**
     * Push a value onto the result stack.
     *
     * @param value  Value to push on the result stack. The evaluator
     *               gets ownership of the value.
     */
    void pushResult(Value *value);

    /**
     * Pop a value off of the result stack.
     *
     * @param evaluationScope  If not @c NULL, the scope used for evaluating the
     *                         result is passed here to the caller. If there was
     *                         a scope, ownership of the scope value is given to
     *                         the caller.
     *
     * @return  Value resulting from expression evaluation. Caller
     *          gets ownership of the returned object.
     */
    Value *popResult(Value **evaluationScope = 0);

    /**
     * Pop a value off of the result stack, making sure it has a specific type.
     *
     * @return  Value resulting from expression evaluation. Caller
     *          gets ownership of the returned object.
     */
    template <typename Type>
    Type *popResultAs() {
        if (!dynamic_cast<Type *>(&result())) {
            throw ResultTypeError("Evaluator::result<Type>",
                "Result type is not compatible with Type");
        }
        return dynamic_cast<Type *>(popResult());
    }

    /**
     * Determines whether a final result has been evaluated.
     */
    bool hasResult() const;

    /**
     * Determines the result of the evaluation without relinquishing
     * ownership of the value instances.
     *
     * @return  The final result of the evaluation.
     */
    Value &result();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_EVALUATOR_H */
